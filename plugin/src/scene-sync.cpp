#include "scene-sync.hpp"
#include "ws-hub.hpp"

#include <obs-frontend-api.h>
#include <obs.h>

#include <string>
#include <vector>
#include <cstring>
#include <cmath>

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

	const char *item_kind = obs_source_get_unversioned_id(item_source);
	state.source_kind = item_kind ? item_kind : "";

	obs_data_t *item_settings = obs_source_get_settings(item_source);
	if (item_settings) {
		if (state.source_kind == "browser_source") {
			const char *url = obs_data_get_string(item_settings, "url");
			state.browser_url = url ? url : "";
			const char *css = obs_data_get_string(item_settings, "css");
			state.browser_css = css ? css : "";
		} else if (state.source_kind == "image_source") {
			const char *file = obs_data_get_string(item_settings, "file");
			state.image_file = file ? file : "";
		}
		obs_data_release(item_settings);
	}

	obs_video_info ovi;
	if (obs_get_video_info(&ovi) && ovi.base_width > 0 && ovi.base_height > 0) {
		struct vec2 pos;
		struct vec2 scale;
		obs_sceneitem_get_pos(item, &pos);
		obs_sceneitem_get_scale(item, &scale);

		uint32_t source_width = obs_source_get_width(item_source);
		uint32_t source_height = obs_source_get_height(item_source);

		struct obs_sceneitem_crop crop;
		obs_sceneitem_get_crop(item, &crop);

		float cropped_width = static_cast<float>(source_width) -
				       static_cast<float>(crop.left + crop.right);
		float cropped_height = static_cast<float>(source_height) -
					static_cast<float>(crop.top + crop.bottom);

		bool flip_x = scale.x < 0.0f;
		bool flip_y = scale.y < 0.0f;

		float item_width = cropped_width * fabsf(scale.x);
		float item_height = cropped_height * fabsf(scale.y);

		enum obs_bounds_type bounds_type = obs_sceneitem_get_bounds_type(item);
		float box_x = pos.x;
		float box_y = pos.y;
		float box_width = item_width;
		float box_height = item_height;

		if (bounds_type != OBS_BOUNDS_NONE) {
			struct vec2 bounds;
			obs_sceneitem_get_bounds(item, &bounds);
			box_width = fabsf(bounds.x);
			box_height = fabsf(bounds.y);
			if (bounds.x < 0.0f)
				flip_x = !flip_x;
			if (bounds.y < 0.0f)
				flip_y = !flip_y;
		}

		uint32_t alignment = obs_sceneitem_get_alignment(item);
		if (alignment & OBS_ALIGN_RIGHT)
			box_x -= box_width;
		else if (!(alignment & OBS_ALIGN_LEFT))
			box_x -= box_width / 2.0f;

		if (alignment & OBS_ALIGN_BOTTOM)
			box_y -= box_height;
		else if (!(alignment & OBS_ALIGN_TOP))
			box_y -= box_height / 2.0f;

		state.x = box_x / static_cast<double>(ovi.base_width);
		state.y = box_y / static_cast<double>(ovi.base_height);
		state.width = box_width / static_cast<double>(ovi.base_width);
		state.height = box_height / static_cast<double>(ovi.base_height);
		state.bounds_type = static_cast<int>(bounds_type);
		state.source_width = source_width;
		state.source_height = source_height;
		state.rotation = static_cast<double>(obs_sceneitem_get_rot(item));
		state.flip_x = flip_x;
		state.flip_y = flip_y;
		state.anchor_x = pos.x / static_cast<double>(ovi.base_width);
		state.anchor_y = pos.y / static_cast<double>(ovi.base_height);
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

void on_source_update_signal(void *data, calldata_t *cd)
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
	signal_handler_t *global_signals = obs_get_signal_handler();
	if (global_signals)
		signal_handler_connect(global_signals, "source_update", on_source_update_signal, nullptr);
	subscribe_to_current_scene();
	refresh_and_publish();
}

void stop()
{
	obs_frontend_remove_event_callback(on_frontend_event, nullptr);
	signal_handler_t *global_signals = obs_get_signal_handler();
	if (global_signals)
		signal_handler_disconnect(global_signals, "source_update", on_source_update_signal, nullptr);
	unsubscribe_current_scene();
}

void notify_filter_changed(obs_source_t *filter_context)
{
	(void)filter_context;
	refresh_and_publish();
}

} // namespace scene_sync
