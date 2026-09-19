#include "jgk_tiles.h"

void hotkey_grab_ctrl_esc(App *app)
{
	XGrabKey(app->dpy, app->esc_keycode, ControlMask, app->root,
	         True, GrabModeAsync, GrabModeAsync);
	XSync(app->dpy, False);
}

int hotkey_is_ctrl_esc(const App *app, const XKeyEvent *ev)
{
	return ev->keycode == app->esc_keycode && (ev->state & ControlMask);
}

int hotkey_is_esc(const App *app, const XKeyEvent *ev)
{
	return ev->keycode == app->esc_keycode && !(ev->state & ControlMask);
}

int key_is_repeat(Display *dpy, XKeyEvent *ev)
{
	if (XEventsQueued(dpy, QueuedAfterReading)) {
		XEvent next;

		XPeekEvent(dpy, &next);
		if (next.type == KeyPress && next.xkey.keycode == ev->keycode &&
		    next.xkey.time == ev->time)
			return 1;
	}
	return 0;
}
