#include <obs-module.h>
#include <util/dstr.h>

#define S_SHOW_ONSCREEN "show_onscreen"

extern void soverlay_link_notify_show_onscreen(const char *source_name, bool show_onscreen);
extern void soverlay_link_notify_enabled(const char *source_name, bool enabled);
extern void soverlay_link_notify_removed(const char *source_name);
extern void soverlay_link_notify_transform(const char *source_name, const char *url, int pos_x, int pos_y, int cx,
                                            int cy, int base_cx, int base_cy);

struct soverlay_filter_data {
    obs_source_t *context;
    bool show_onscreen;
    bool destroyed;
};

struct find_sceneitem_ctx {
    obs_source_t *target;
    obs_sceneitem_t *result;
};

static bool find_sceneitem_cb(obs_scene_t *scene, obs_sceneitem_t *item, void *param)
{
    UNUSED_PARAMETER(scene);
    struct find_sceneitem_ctx *ctx = param;

    if (obs_sceneitem_get_source(item) == ctx->target) {
        ctx->result = item;
        return false;
    }
    return true;
}

static bool find_in_scene_cb(void *param, obs_source_t *scene_source)
{
    struct find_sceneitem_ctx *ctx = param;
    obs_scene_t *scene = obs_scene_from_source(scene_source);
    if (!scene)
        return true;

    obs_scene_enum_items(scene, find_sceneitem_cb, ctx);
    return ctx->result == NULL;
}

static obs_sceneitem_t *soverlay_find_sceneitem(obs_source_t *target)
{
    struct find_sceneitem_ctx ctx = {.target = target, .result = NULL};
    obs_enum_scenes(find_in_scene_cb, &ctx);
    return ctx.result;
}

static const char *soverlay_filter_get_name(void *unused)
{
    UNUSED_PARAMETER(unused);
    return obs_module_text("SOverlay.ShowOnscreenFilter");
}

static void soverlay_filter_notify_current_state(struct soverlay_filter_data *filter)
{
    obs_source_t *target = obs_filter_get_parent(filter->context);
    if (!target)
        return;

    const char *name = obs_source_get_name(target);
    if (!name || !*name)
        return;

    soverlay_link_notify_show_onscreen(name, filter->show_onscreen);
    soverlay_link_notify_enabled(name, obs_source_enabled(target));

    const char *url = NULL;
    obs_data_t *target_settings = obs_source_get_settings(target);
    if (target_settings) {
        url = obs_data_get_string(target_settings, "url");
    }

    struct obs_video_info ovi;
    int base_cx = 0, base_cy = 0;
    if (obs_get_video_info(&ovi)) {
        base_cx = (int)ovi.base_width;
        base_cy = (int)ovi.base_height;
    }

    int pos_x = 0, pos_y = 0;
    int cx = (int)obs_source_get_width(target);
    int cy = (int)obs_source_get_height(target);

    obs_sceneitem_t *item = soverlay_find_sceneitem(target);
    if (item) {
        struct vec2 pos;
        struct vec2 scale;
        obs_sceneitem_get_pos(item, &pos);
        obs_sceneitem_get_scale(item, &scale);
        pos_x = (int)pos.x;
        pos_y = (int)pos.y;
        cx = (int)((float)cx * scale.x);
        cy = (int)((float)cy * scale.y);
    }

    soverlay_link_notify_transform(name, url ? url : "", pos_x, pos_y, cx, cy, base_cx, base_cy);

    if (target_settings)
        obs_data_release(target_settings);
}

static void soverlay_filter_update(void *data, obs_data_t *settings)
{
    struct soverlay_filter_data *filter = data;
    bool new_value = obs_data_get_bool(settings, S_SHOW_ONSCREEN);

    filter->show_onscreen = new_value;
    soverlay_filter_notify_current_state(filter);
}

static void *soverlay_filter_create(obs_data_t *settings, obs_source_t *context)
{
    struct soverlay_filter_data *filter = bzalloc(sizeof(struct soverlay_filter_data));
    filter->context = context;

    soverlay_filter_update(filter, settings);
    return filter;
}

static void soverlay_filter_destroy(void *data)
{
    struct soverlay_filter_data *filter = data;

    obs_source_t *target = obs_filter_get_parent(filter->context);
    if (target) {
        const char *name = obs_source_get_name(target);
        if (name && *name)
            soverlay_link_notify_removed(name);
    }

    bfree(filter);
}

static obs_properties_t *soverlay_filter_get_properties(void *data)
{
    UNUSED_PARAMETER(data);

    obs_properties_t *props = obs_properties_create();
    obs_properties_add_bool(props, S_SHOW_ONSCREEN, obs_module_text("SOverlay.ShowOnscreen"));
    return props;
}

static void soverlay_filter_get_defaults(obs_data_t *settings)
{
    obs_data_set_default_bool(settings, S_SHOW_ONSCREEN, false);
}

static void soverlay_filter_video_render(void *data, gs_effect_t *effect)
{
    UNUSED_PARAMETER(effect);
    struct soverlay_filter_data *filter = data;

    if (filter->show_onscreen) {
        obs_source_skip_video_filter(filter->context);
        return;
    }

    obs_source_t *target = obs_filter_get_target(filter->context);
    obs_source_t *parent = obs_filter_get_parent(filter->context);
    if (!target || !parent) {
        obs_source_skip_video_filter(filter->context);
        return;
    }

    int cx = obs_source_get_base_width(target);
    int cy = obs_source_get_base_height(target);
    if (cx <= 0 || cy <= 0) {
        obs_source_skip_video_filter(filter->context);
        return;
    }

    if (!obs_source_process_filter_begin(filter->context, GS_RGBA, OBS_ALLOW_DIRECT_RENDERING))
        return;

    obs_source_process_filter_end(filter->context, obs_get_base_effect(OBS_EFFECT_DEFAULT), cx, cy);
}

struct obs_source_info soverlay_show_onscreen_filter = {
    .id = "soverlay_show_onscreen_filter",
    .type = OBS_SOURCE_TYPE_FILTER,
    .output_flags = OBS_SOURCE_VIDEO,
    .get_name = soverlay_filter_get_name,
    .create = soverlay_filter_create,
    .destroy = soverlay_filter_destroy,
    .update = soverlay_filter_update,
    .get_properties = soverlay_filter_get_properties,
    .get_defaults = soverlay_filter_get_defaults,
    .video_render = soverlay_filter_video_render,
};
