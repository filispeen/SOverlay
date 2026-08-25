/*
SOverlay Source Control
Copyright (C) 2026 filispeen

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program. If not, see <https://www.gnu.org/licenses/>
*/

#include "source-filter.hpp"
#include "scene-sync.hpp"
#include "ws-hub.hpp"
#include "overlay-launcher.hpp"

#include <obs-module.h>
#include <plugin-support.h>

#define WS_HUB_PORT 7853

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

bool obs_module_load(void)
{
	register_source_filter();
	ws_hub::start(WS_HUB_PORT);
	scene_sync::start();
	overlay_launcher::start();

	obs_log(LOG_INFO, "plugin loaded successfully (version %s)", PLUGIN_VERSION);
	return true;
}

void obs_module_unload(void)
{
	overlay_launcher::stop();
	scene_sync::stop();
	ws_hub::stop();

	obs_log(LOG_INFO, "plugin unloaded");
}
