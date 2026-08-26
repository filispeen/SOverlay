#pragma once

#include <obs-module.h>

namespace scene_sync {

void start();
void stop();

void notify_filter_changed(obs_source_t *filter_context);

} // namespace scene_sync
