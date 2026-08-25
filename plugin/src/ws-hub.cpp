#include "ws-hub.hpp"

#include <ixwebsocket/IXNetSystem.h>
#include <ixwebsocket/IXWebSocketServer.h>

#include <obs-module.h>
#include <plugin-support.h>

#include <mutex>
#include <set>
#include <sstream>
#include <cctype>

namespace {

std::unique_ptr<ix::WebSocketServer> g_server;
std::mutex g_mutex;
std::vector<SourceState> g_last_visible_set;
ws_hub::MediaCommandHandler g_media_command_handler = nullptr;

std::string escape_json(const std::string &in)
{
	std::string out;
	out.reserve(in.size());
	for (char c : in) {
		switch (c) {
		case '"':
			out += "\\\"";
			break;
		case '\\':
			out += "\\\\";
			break;
		case '\n':
			out += "\\n";
			break;
		default:
			out += c;
		}
	}
	return out;
}

std::string state_to_json_fields(const SourceState &s)
{
	std::ostringstream o;
	o << "\"uuid\":\"" << escape_json(s.uuid) << "\",";
	o << "\"name\":\"" << escape_json(s.name) << "\",";
	o << "\"enabled\":" << (s.enabled ? "true" : "false") << ",";
	o << "\"show_onscreen\":" << (s.show_onscreen ? "true" : "false") << ",";
	o << "\"z_index\":" << s.z_index << ",";
	o << "\"source_kind\":\"" << escape_json(s.source_kind) << "\",";
	o << "\"browser_url\":\"" << escape_json(s.browser_url) << "\",";
	o << "\"browser_css\":\"" << escape_json(s.browser_css) << "\",";
	o << "\"image_file\":\"" << escape_json(s.image_file) << "\",";
	o << "\"source_width\":" << s.source_width << ",";
	o << "\"source_height\":" << s.source_height << ",";
	o << "\"bounds_type\":" << s.bounds_type << ",";
	o << "\"rotation\":" << s.rotation << ",";
	o << "\"flip_x\":" << (s.flip_x ? "true" : "false") << ",";
	o << "\"flip_y\":" << (s.flip_y ? "true" : "false") << ",";
	o << "\"anchor_x\":" << s.anchor_x << ",";
	o << "\"anchor_y\":" << s.anchor_y << ",";
	o << "\"media_file\":\"" << escape_json(s.media_file) << "\",";
	o << "\"media_loop\":" << (s.media_loop ? "true" : "false") << ",";
	o << "\"media_state\":\"" << escape_json(s.media_state) << "\",";
	o << "\"media_time_ms\":" << s.media_time_ms << ",";
	o << "\"media_duration_ms\":" << s.media_duration_ms << ",";
	o << "\"media_seek\":" << (s.media_seek ? "true" : "false") << ",";
	o << "\"transform\":{";
	o << "\"x\":" << s.x << ",";
	o << "\"y\":" << s.y << ",";
	o << "\"width\":" << s.width << ",";
	o << "\"height\":" << s.height;
	o << "}";
	return o.str();
}

std::string build_visible_set_message(const std::vector<SourceState> &states)
{
	std::ostringstream o;
	o << "{\"type\":\"visible_set\",\"sources\":[";
	bool first = true;
	for (const auto &s : states) {
		if (!first)
			o << ",";
		first = false;
		o << "{" << state_to_json_fields(s) << "}";
	}
	o << "]}";
	return o.str();
}

bool extract_json_string(const std::string &json, const std::string &key, std::string &out)
{
	std::string needle = "\"" + key + "\":\"";
	size_t pos = json.find(needle);
	if (pos == std::string::npos)
		return false;
	pos += needle.size();
	size_t end = pos;
	std::string result;
	while (end < json.size() && json[end] != '"') {
		if (json[end] == '\\' && end + 1 < json.size()) {
			end++;
			switch (json[end]) {
			case 'n':
				result += '\n';
				break;
			case '"':
				result += '"';
				break;
			case '\\':
				result += '\\';
				break;
			default:
				result += json[end];
			}
		} else {
			result += json[end];
		}
		end++;
	}
	out = result;
	return true;
}

bool extract_json_number(const std::string &json, const std::string &key, double &out)
{
	std::string needle = "\"" + key + "\":";
	size_t pos = json.find(needle);
	if (pos == std::string::npos)
		return false;
	pos += needle.size();
	size_t end = pos;
	while (end < json.size() && (isdigit(static_cast<unsigned char>(json[end])) || json[end] == '-' ||
				      json[end] == '.' || json[end] == '+' || json[end] == 'e' || json[end] == 'E'))
		end++;
	if (end == pos)
		return false;
	out = std::stod(json.substr(pos, end - pos));
	return true;
}

void handle_incoming_message(const std::string &text)
{
	std::string type;
	if (!extract_json_string(text, "type", type) || type != "media_command")
		return;

	ws_hub::MediaCommand cmd;
	extract_json_string(text, "uuid", cmd.uuid);
	extract_json_string(text, "action", cmd.action);

	double seek_ms = 0.0;
	if (extract_json_number(text, "seek_ms", seek_ms))
		cmd.seek_ms = static_cast<int64_t>(seek_ms);

	double volume = -1.0;
	if (extract_json_number(text, "volume", volume))
		cmd.volume = volume;

	if (cmd.uuid.empty() || cmd.action.empty())
		return;

	if (g_media_command_handler)
		g_media_command_handler(cmd);
}

} // namespace

namespace ws_hub {

void start(int port)
{
	std::lock_guard<std::mutex> lock(g_mutex);
	if (g_server) {
		obs_log(LOG_WARNING, "ws_hub::start called while server already running");
		return;
	}

	ix::initNetSystem();

	g_server = std::make_unique<ix::WebSocketServer>(port, "127.0.0.1");

	g_server->setOnConnectionCallback(
		[](std::weak_ptr<ix::WebSocket> weak_ws, std::shared_ptr<ix::ConnectionState> connection_state) {
			(void)connection_state;
			auto ws = weak_ws.lock();
			if (!ws)
				return;

			ws->setOnMessageCallback([weak_ws](const ix::WebSocketMessagePtr &msg) {
				if (msg->type == ix::WebSocketMessageType::Message) {
					handle_incoming_message(msg->str);
					return;
				}

				if (msg->type != ix::WebSocketMessageType::Open)
					return;

				auto ws2 = weak_ws.lock();
				if (!ws2)
					return;

				std::string snapshot;
				{
					std::lock_guard<std::mutex> lock(g_mutex);
					snapshot = build_visible_set_message(g_last_visible_set);
				}
				ws2->send(snapshot, false);
			});
		});

	auto result = g_server->listen();
	if (!result.first) {
		obs_log(LOG_ERROR, "ws_hub: failed to listen on port %d: %s", port, result.second.c_str());
		g_server.reset();
		ix::uninitNetSystem();
		return;
	}

	g_server->start();
	obs_log(LOG_INFO, "ws_hub: listening on ws://127.0.0.1:%d", port);
}

void stop()
{
	std::lock_guard<std::mutex> lock(g_mutex);
	if (!g_server)
		return;

	g_server->stop();
	g_server.reset();
	ix::uninitNetSystem();
	obs_log(LOG_INFO, "ws_hub: stopped");
}

void publish_visible_set(const std::vector<SourceState> &states)
{
	std::string message;
	std::set<std::shared_ptr<ix::WebSocket>> clients;

	{
		std::lock_guard<std::mutex> lock(g_mutex);
		g_last_visible_set = states;
		if (!g_server)
			return;
		message = build_visible_set_message(states);
		clients = g_server->getClients();
	}

	for (auto &client : clients) {
		client->send(message, false);
	}
}

void set_media_command_handler(MediaCommandHandler handler)
{
	g_media_command_handler = handler;
}

} // namespace ws_hub
