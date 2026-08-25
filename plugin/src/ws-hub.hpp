#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct SourceState {
	std::string uuid;
	std::string name;
	bool enabled = false;
	bool show_onscreen = false;
	int z_index = 0;
	double x = 0.0;
	double y = 0.0;
	double width = 0.0;
	double height = 0.0;
	int bounds_type = 0;
	uint32_t source_width = 0;
	uint32_t source_height = 0;
	double rotation = 0.0;
	bool flip_x = false;
	bool flip_y = false;
	double anchor_x = 0.0;
	double anchor_y = 0.0;
	bool media_loop = false;
	std::string media_state;
	int64_t media_time_ms = 0;
	int64_t media_duration_ms = 0;
	bool media_seek = false;
	std::string source_kind;
	std::string browser_url;
	std::string browser_css;
	std::string image_file;
	std::string media_file;
};

inline bool operator==(const SourceState &a, const SourceState &b)
{
	return a.uuid == b.uuid && a.name == b.name && a.enabled == b.enabled &&
	       a.show_onscreen == b.show_onscreen && a.z_index == b.z_index && a.x == b.x && a.y == b.y && a.width == b.width &&
	       a.height == b.height && a.bounds_type == b.bounds_type && a.source_width == b.source_width &&
	       a.source_height == b.source_height && a.rotation == b.rotation && a.flip_x == b.flip_x &&
	       a.flip_y == b.flip_y && a.anchor_x == b.anchor_x && a.anchor_y == b.anchor_y &&
	       a.media_loop == b.media_loop &&
	       a.media_state == b.media_state && a.media_time_ms == b.media_time_ms &&
	       a.media_duration_ms == b.media_duration_ms &&
	       a.source_kind == b.source_kind &&
	       a.browser_url == b.browser_url && a.browser_css == b.browser_css && a.image_file == b.image_file &&
	       a.media_file == b.media_file;
}

inline bool operator!=(const SourceState &a, const SourceState &b)
{
	return !(a == b);
}

namespace ws_hub {

struct MediaCommand {
	std::string uuid;
	std::string action;
	int64_t seek_ms = 0;
	double volume = -1.0;
};

using MediaCommandHandler = void (*)(const MediaCommand &cmd);

void start(int port);
void stop();

void publish_visible_set(const std::vector<SourceState> &states);
void set_media_command_handler(MediaCommandHandler handler);

} // namespace ws_hub
