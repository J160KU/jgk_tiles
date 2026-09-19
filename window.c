#include "jgk_tiles.h"
#include "bridge.h"
#include "log.h"
#include <stdlib.h>
#include <string.h>

int window_get_workarea(App *app)
{
	Atom type;
	int fmt;
	unsigned long n, extra;
	unsigned char *data = NULL;

	if (XGetWindowProperty(app->dpy, app->root, app->net_workarea,
	                       0, 4, False, XA_CARDINAL, &type, &fmt,
	                       &n, &extra, &data) != Success || !data || n < 4) {
		if (data)
			XFree(data);
		app->work_x = 0;
		app->work_y = 0;
		app->work_w = DisplayWidth(app->dpy, app->screen);
		app->work_h = DisplayHeight(app->dpy, app->screen);
		return 0;
	}

	unsigned long *v = (unsigned long *)data;
	app->work_x = (int)v[0];
	app->work_y = (int)v[1];
	app->work_w = (int)v[2];
	app->work_h = (int)v[3];
	XFree(data);
	return 1;
}

int window_get_active(App *app, Window *out)
{
	Atom type;
	int fmt;
	unsigned long n, extra;
	unsigned char *data = NULL;

	*out = 0;
	if (XGetWindowProperty(app->dpy, app->root, app->net_active_window,
	                       0, 1, False, XA_WINDOW, &type, &fmt,
	                       &n, &extra, &data) != Success || !data || n < 1) {
		if (data)
			XFree(data);
		return 0;
	}
	*out = *(Window *)data;
	XFree(data);
	return *out != 0;
}

int window_valid(App *app, Window w)
{
	XWindowAttributes wa;

	if (!w || w == app->root || w == app->overlay)
		return 0;
	if (!XGetWindowAttributes(app->dpy, w, &wa))
		return 0;
	return wa.class != InputOnly;
}

void window_move_resize(App *app, Window win, int x, int y, int width, int height)
{
	XEvent ev;
	memset(&ev, 0, sizeof(ev));
	ev.type = ClientMessage;
	ev.xclient.window = win;
	ev.xclient.message_type = app->net_move_resize;
	ev.xclient.format = 32;
	ev.xclient.data.l[0] = -2;
	ev.xclient.data.l[1] = 0; /* gravity */
	ev.xclient.data.l[2] = x;
	ev.xclient.data.l[3] = y;
	ev.xclient.data.l[4] = width;
	ev.xclient.data.l[5] = height;
	XSendEvent(app->dpy, app->root, False,
	           SubstructureRedirectMask | SubstructureNotifyMask, &ev);
	XMoveResizeWindow(app->dpy, win, x, y, (unsigned)width, (unsigned)height);
	XFlush(app->dpy);
}

void window_apply_rect(App *app, int x, int y, int width, int height)
{
	int max_x = app->work_x + app->work_w;
	int max_y = app->work_y + app->work_h;

	if (app->work_w > 0 && app->work_h > 0) {
		if (x < app->work_x) x = app->work_x;
		if (y < app->work_y) y = app->work_y;
		if (x + width > max_x) width = max_x - x;
		if (y + height > max_y) height = max_y - y;
		if (width < 1 || height < 1)
			return;
	}
	if (bridge_resize(x, y, width, height))
		return;
	if (app->use_wayland) {
		log_msg("bridge RESIZE failed on Wayland; skip X11 fallback");
		return;
	}
	if (window_valid(app, app->target)) {
		log_msg("X11 fallback resize target=0x%lx", app->target);
		window_move_resize(app, app->target, x, y, width, height);
	}
}

void window_focus(App *app, Window w)
{
	if (!window_valid(app, w))
		return;
	XSetInputFocus(app->dpy, w, RevertToPointerRoot, CurrentTime);
	XFlush(app->dpy);
}
