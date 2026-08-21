#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include <algorithm>
#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

extern "C" {
#include <util/base.h>
}

#define SOVERLAY_LINK_PORT 51873

namespace {

std::atomic<bool> g_running{false};
std::thread g_accept_thread;
SOCKET g_listen_socket = INVALID_SOCKET;

std::mutex g_clients_mutex;
std::vector<SOCKET> g_clients;

std::string json_escape(const char *s)
{
    std::string out;
    for (const char *p = s; *p; ++p) {
        switch (*p) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        default: out += *p;
        }
    }
    return out;
}

void broadcast(const std::string &line)
{
    std::string framed = line + "\n";

    std::lock_guard<std::mutex> lock(g_clients_mutex);
    for (auto it = g_clients.begin(); it != g_clients.end();) {
        int sent = send(*it, framed.c_str(), (int)framed.size(), 0);
        if (sent == SOCKET_ERROR) {
            closesocket(*it);
            it = g_clients.erase(it);
        } else {
            ++it;
        }
    }
}

void client_thread(SOCKET client)
{
    char buf[512];
    while (g_running.load()) {
        int received = recv(client, buf, sizeof(buf), 0);
        if (received <= 0)
            break;
    }

    {
        std::lock_guard<std::mutex> lock(g_clients_mutex);
        auto it = std::find(g_clients.begin(), g_clients.end(), client);
        if (it != g_clients.end())
            g_clients.erase(it);
    }
    closesocket(client);
}

void accept_loop()
{
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        blog(LOG_ERROR, "[soverlay-plugin] WSAStartup failed");
        return;
    }

    g_listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (g_listen_socket == INVALID_SOCKET) {
        blog(LOG_ERROR, "[soverlay-plugin] failed to create listen socket");
        WSACleanup();
        return;
    }

    BOOL reuse = TRUE;
    setsockopt(g_listen_socket, SOL_SOCKET, SO_REUSEADDR, (const char *)&reuse, sizeof(reuse));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_port = htons(SOVERLAY_LINK_PORT);

    if (bind(g_listen_socket, (sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR) {
        blog(LOG_ERROR, "[soverlay-plugin] bind failed on port %d", SOVERLAY_LINK_PORT);
        closesocket(g_listen_socket);
        g_listen_socket = INVALID_SOCKET;
        WSACleanup();
        return;
    }

    if (listen(g_listen_socket, 4) == SOCKET_ERROR) {
        blog(LOG_ERROR, "[soverlay-plugin] listen failed");
        closesocket(g_listen_socket);
        g_listen_socket = INVALID_SOCKET;
        WSACleanup();
        return;
    }

    blog(LOG_INFO, "[soverlay-plugin] link server listening on 127.0.0.1:%d", SOVERLAY_LINK_PORT);

    while (g_running.load()) {
        sockaddr_in client_addr{};
        int client_addr_len = sizeof(client_addr);
        SOCKET client = accept(g_listen_socket, (sockaddr *)&client_addr, &client_addr_len);
        if (client == INVALID_SOCKET) {
            if (!g_running.load())
                break;
            continue;
        }

        {
            std::lock_guard<std::mutex> lock(g_clients_mutex);
            g_clients.push_back(client);
        }

        std::thread(client_thread, client).detach();
    }

    WSACleanup();
}

} // namespace

extern "C" void soverlay_link_server_start(void)
{
    if (g_running.exchange(true))
        return;

    g_accept_thread = std::thread(accept_loop);
}

extern "C" void soverlay_link_server_stop(void)
{
    if (!g_running.exchange(false))
        return;

    if (g_listen_socket != INVALID_SOCKET) {
        closesocket(g_listen_socket);
        g_listen_socket = INVALID_SOCKET;
    }

    {
        std::lock_guard<std::mutex> lock(g_clients_mutex);
        for (SOCKET s : g_clients)
            closesocket(s);
        g_clients.clear();
    }

    if (g_accept_thread.joinable())
        g_accept_thread.join();
}

extern "C" void soverlay_link_notify_show_onscreen(const char *source_name, bool show_onscreen)
{
    std::string line = "{\"type\":\"show_onscreen\",\"source\":\"" + json_escape(source_name) +
                        "\",\"value\":" + (show_onscreen ? "true" : "false") + "}";
    broadcast(line);
}

extern "C" void soverlay_link_notify_enabled(const char *source_name, bool enabled)
{
    std::string line = "{\"type\":\"enabled\",\"source\":\"" + json_escape(source_name) +
                        "\",\"value\":" + (enabled ? "true" : "false") + "}";
    broadcast(line);
}

extern "C" void soverlay_link_notify_transform(const char *source_name, const char *url, int pos_x, int pos_y,
                                                int cx, int cy, int base_cx, int base_cy)
{
    std::string line = "{\"type\":\"transform\",\"source\":\"" + json_escape(source_name) + "\",\"url\":\"" +
                        json_escape(url) + "\",\"x\":" + std::to_string(pos_x) + ",\"y\":" + std::to_string(pos_y) +
                        ",\"cx\":" + std::to_string(cx) + ",\"cy\":" + std::to_string(cy) +
                        ",\"base_cx\":" + std::to_string(base_cx) + ",\"base_cy\":" + std::to_string(base_cy) + "}";
    broadcast(line);
}

extern "C" void soverlay_link_notify_removed(const char *source_name)
{
    std::string line = "{\"type\":\"removed\",\"source\":\"" + json_escape(source_name) + "\"}";
    broadcast(line);
}
