#include "source-filter.hpp"
#include "scene-sync.hpp"

#include <obs-module.h>

#define FILTER_ID "soverlay_show_onscreen_filter"
#define PROP_SHOW_ONSCREEN "show_onscreen"

struct show_onscreen_filter {
	obs_source_t *context;
	bool show_onscreen;
};

static const char *filter_get_name(void *unused)
{
	(void)unused;
	return obs_module_text("ShowOnscreenFilter.Name");
}

static void filter_update(void *data, obs_data_t *settings)
{
	auto *filter = static_cast<show_onscreen_filter *>(data);
	filter->show_onscreen = obs_data_get_bool(settings, PROP_SHOW_ONSCREEN);
	scene_sync::notify_filter_changed(filter->context);
}

static void *filter_create(obs_data_t *settings, obs_source_t *context)
{
	auto *filter = new show_onscreen_filter();
	filter->context = context;
	filter_update(filter, settings);
	return filter;
}

static void filter_destroy(void *data)
{
	auto *filter = static_cast<show_onscreen_filter *>(data);
	obs_source_t *context = filter->context;
	delete filter;
	scene_sync::notify_filter_changed(context);
}

static obs_properties_t *filter_get_properties(void *data)
{
	(void)data;
	obs_properties_t *props = obs_properties_create();
	obs_properties_add_bool(props, PROP_SHOW_ONSCREEN, obs_module_text("ShowOnscreenFilter.ShowOnscreen"));
	return props;
}

static void filter_get_defaults(obs_data_t *settings)
{
	obs_data_set_default_bool(settings, PROP_SHOW_ONSCREEN, false);
}

static void filter_video_render(void *data, gs_effect_t *effect)
{
	auto *filter = static_cast<show_onscreen_filter *>(data);

	if (filter->show_onscreen) {
		obs_source_skip_video_filter(filter->context);
		return;
	}

	obs_source_t *target = obs_filter_get_target(filter->context);
	if (!target) {
		obs_source_skip_video_filter(filter->context);
		return;
	}

	if (!obs_source_process_filter_begin(filter->context, GS_RGBA, OBS_ALLOW_DIRECT_RENDERING)) {
		obs_source_skip_video_filter(filter->context);
		return;
	}

	obs_source_process_filter_end(filter->context, effect ? effect : obs_get_base_effect(OBS_EFFECT_DEFAULT), 0,
				       0);
}

bool source_filter_is_show_onscreen(obs_source_t *filter_source)
{
	(void)filter_source;
	return false;
}

void register_source_filter()
{
	struct obs_source_info info = {};
	info.id = FILTER_ID;
	info.type = OBS_SOURCE_TYPE_FILTER;
	info.output_flags = OBS_SOURCE_VIDEO;
	info.get_name = filter_get_name;
	info.create = filter_create;
	info.destroy = filter_destroy;
	info.update = filter_update;
	info.get_properties = filter_get_properties;
	info.get_defaults = filter_get_defaults;
	info.video_render = filter_video_render;

	obs_register_source(&info);
}
