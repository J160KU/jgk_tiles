#include "jgk_tiles.h"
#include "bridge.h"
#include "log.h"

void overlay_toggle(App *app)
{
	log_msg("overlay_toggle visible=%d wayland=%d", app->overlay_visible, app->use_wayland);
	if (app->use_wayland && bridge_overlay_toggle())
		return;
	if (app->overlay_visible) {
		if (app->use_wayland)
			wl_overlay_hide(app);
		else
			overlay_hide(app);
	} else {
		if (app->use_wayland)
			wl_overlay_show(app);
		else
			overlay_show(app);
	}
}

void overlay_hide_any(App *app)
{
	if (app->use_wayland)
		bridge_overlay_hide();
	if (!app->overlay_visible)
		return;
	if (app->use_wayland)
		wl_overlay_hide(app);
	else
		overlay_hide(app);
}
