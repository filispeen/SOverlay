#pragma once

#include <obs-module.h>

namespace scene_sync {

void start();
void stop();

void notify_filter_changed(obs_source_t *filter_context);
void apply_show_onscreen_visibility(obs_source_t *filter_context, bool show_onscreen);

} // namespace scene_sync
