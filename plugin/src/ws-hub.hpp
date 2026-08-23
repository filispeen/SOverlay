#pragma once

#include <string>
#include <vector>

struct SourceState {
	std::string uuid;
	std::string name;
	bool enabled = false;
	bool show_onscreen = false;
	double x = 0.0;
	double y = 0.0;
	double width = 0.0;
	double height = 0.0;
};

inline bool operator==(const SourceState &a, const SourceState &b)
{
	return a.uuid == b.uuid && a.name == b.name && a.enabled == b.enabled &&
	       a.show_onscreen == b.show_onscreen && a.x == b.x && a.y == b.y && a.width == b.width &&
	       a.height == b.height;
}

inline bool operator!=(const SourceState &a, const SourceState &b)
{
	return !(a == b);
}

namespace ws_hub {

void start(int port);
void stop();

void publish_visible_set(const std::vector<SourceState> &states);

} // namespace ws_hub
