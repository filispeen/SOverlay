#include <obs-module.h>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("soverlay-plugin", "en-US")

extern struct obs_source_info soverlay_show_onscreen_filter;

extern void soverlay_link_server_start(void);
extern void soverlay_link_server_stop(void);

bool obs_module_load(void)
{
    obs_register_source(&soverlay_show_onscreen_filter);
    soverlay_link_server_start();

    blog(LOG_INFO, "[soverlay-plugin] loaded, version %s", SOVERLAY_PLUGIN_VERSION);
    return true;
}

void obs_module_unload(void)
{
    soverlay_link_server_stop();
    blog(LOG_INFO, "[soverlay-plugin] unloaded");
}
