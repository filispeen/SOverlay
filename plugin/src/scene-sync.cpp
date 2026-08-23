#include "scene-sync.hpp"
#include "ws-hub.hpp"

#include <obs-frontend-api.h>
#include <obs.h>

#include <graphics/matrix4.h>

#include <string>
#include <vector>
#include <cstring>

#define FILTER_ID "soverlay_show_onscreen_filter"
#define PROP_SHOW_ONSCREEN "show_onscreen"

namespace {

obs_source_t *g_current_scene_source = nullptr;
signal_handler_t *g_current_scene_signals = nullptr;
std::vector<SourceState> g_last_published_states;

void refresh_and_publish();

bool collect_item_cb(obs_scene_t *scene, obs_sceneitem_t *item, void *param)
{
	(void)scene;
	auto *out = static_cast<std::vector<SourceState> *>(param);

	obs_source_t *item_source = obs_sceneitem_get_source(item);
	if (!item_source)
		return true;

	struct find_filter_ctx {
		obs_source_t *found = nullptr;
	} ctx;

	obs_source_enum_filters(
		item_source,
		[](obs_source_t *, obs_source_t *filter, void *p) {
			auto *c = static_cast<find_filter_ctx *>(p);
			if (c->found)
				return;
			const char *unversioned_id = obs_source_get_unversioned_id(filter);
			if (unversioned_id && strcmp(unversioned_id, FILTER_ID) == 0)
				c->found = filter;
		},
		&ctx);

	if (!ctx.found)
		return true;

	obs_data_t *settings = obs_source_get_settings(ctx.found);
	bool show_onscreen = obs_data_get_bool(settings, PROP_SHOW_ONSCREEN);
	obs_data_release(settings);

	if (!show_onscreen)
		return true;

	SourceState state;
	state.uuid = obs_source_get_uuid(item_source);
	state.name = obs_source_get_name(item_source);
	state.enabled = obs_sceneitem_visible(item);
	state.show_onscreen = show_onscreen;

	obs_video_info ovi;
	if (obs_get_video_info(&ovi) && ovi.base_width > 0 && ovi.base_height > 0) {
		matrix4 box_transform;
		obs_sceneitem_get_box_transform(item, &box_transform);

		vec3 corners[4];
		vec3_set(&corners[0], 0.0f, 0.0f, 0.0f);
		vec3_set(&corners[1], 1.0f, 0.0f, 0.0f);
		vec3_set(&corners[2], 0.0f, 1.0f, 0.0f);
		vec3_set(&corners[3], 1.0f, 1.0f, 0.0f);

		float min_x = 0.0f, max_x = 0.0f, min_y = 0.0f, max_y = 0.0f;
		for (int i = 0; i < 4; i++) {
			vec3 transformed;
			vec3_transform(&transformed, &corners[i], &box_transform);
			if (i == 0) {
				min_x = max_x = transformed.x;
				min_y = max_y = transformed.y;
			} else {
				if (transformed.x < min_x)
					min_x = transformed.x;
				if (transformed.x > max_x)
					max_x = transformed.x;
				if (transformed.y < min_y)
					min_y = transformed.y;
				if (transformed.y > max_y)
					max_y = transformed.y;
			}
		}

		state.x = min_x / static_cast<double>(ovi.base_width);
		state.y = min_y / static_cast<double>(ovi.base_height);
		state.width = (max_x - min_x) / static_cast<double>(ovi.base_width);
		state.height = (max_y - min_y) / static_cast<double>(ovi.base_height);
	}

	out->push_back(std::move(state));
	return true;
}

void refresh_and_publish()
{
	std::vector<SourceState> states;

	if (g_current_scene_source) {
		obs_scene_t *scene = obs_scene_from_source(g_current_scene_source);
		if (scene) {
			obs_scene_enum_items(scene, collect_item_cb, &states);
		}
	}

	if (states == g_last_published_states)
		return;

	g_last_published_states = states;
	ws_hub::publish_visible_set(states);
}

void on_scene_item_signal(void *data, calldata_t *cd)
{
	(void)data;
	(void)cd;
	refresh_and_publish();
}

void unsubscribe_current_scene()
{
	if (g_current_scene_signals) {
		signal_handler_disconnect(g_current_scene_signals, "item_transform", on_scene_item_signal, nullptr);
		signal_handler_disconnect(g_current_scene_signals, "item_visible", on_scene_item_signal, nullptr);
		signal_handler_disconnect(g_current_scene_signals, "item_add", on_scene_item_signal, nullptr);
		signal_handler_disconnect(g_current_scene_signals, "item_remove", on_scene_item_signal, nullptr);
		g_current_scene_signals = nullptr;
	}
	if (g_current_scene_source) {
		obs_source_release(g_current_scene_source);
		g_current_scene_source = nullptr;
	}
}

void subscribe_to_current_scene()
{
	unsubscribe_current_scene();

	obs_source_t *scene_source = obs_frontend_get_current_scene();
	if (!scene_source)
		return;

	g_current_scene_source = scene_source;
	g_current_scene_signals = obs_source_get_signal_handler(scene_source);

	if (g_current_scene_signals) {
		signal_handler_connect(g_current_scene_signals, "item_transform", on_scene_item_signal, nullptr);
		signal_handler_connect(g_current_scene_signals, "item_visible", on_scene_item_signal, nullptr);
		signal_handler_connect(g_current_scene_signals, "item_add", on_scene_item_signal, nullptr);
		signal_handler_connect(g_current_scene_signals, "item_remove", on_scene_item_signal, nullptr);
	}
}

void on_frontend_event(enum obs_frontend_event event, void *private_data)
{
	(void)private_data;
	switch (event) {
	case OBS_FRONTEND_EVENT_SCENE_CHANGED:
	case OBS_FRONTEND_EVENT_FINISHED_LOADING:
		subscribe_to_current_scene();
		refresh_and_publish();
		break;
	default:
		break;
	}
}

} // namespace

namespace scene_sync {

void start()
{
	obs_frontend_add_event_callback(on_frontend_event, nullptr);
	subscribe_to_current_scene();
	refresh_and_publish();
}

void stop()
{
	obs_frontend_remove_event_callback(on_frontend_event, nullptr);
	unsubscribe_current_scene();
}

void notify_filter_changed(obs_source_t *filter_context)
{
	(void)filter_context;
	refresh_and_publish();
}

} // namespace scene_sync
