#include "jgk_tiles.h"
#include "bridge.h"
#include <string.h>

static unsigned long color_pixel(App *app, const char *name)
{
	Colormap cmap = DefaultColormap(app->dpy, app->screen);
	XColor c, exact;

	if (!XAllocNamedColor(app->dpy, cmap, name, &c, &exact))
		return WhitePixel(app->dpy, app->screen);
	return c.pixel;
}

static void wm_set_state(App *app)
{
	Atom state = XInternAtom(app->dpy, "_NET_WM_STATE", False);
	Atom above = XInternAtom(app->dpy, "_NET_WM_STATE_ABOVE", False);
	Atom fullscreen = XInternAtom(app->dpy, "_NET_WM_STATE_FULLSCREEN", False);
	Atom atoms[2] = { above, fullscreen };

	XChangeProperty(app->dpy, app->overlay, state, XA_ATOM, 32,
	                PropModeReplace, (unsigned char *)atoms, 2);

	Atom opacity_atom = XInternAtom(app->dpy, "_NET_WM_WINDOW_OPACITY", False);
	unsigned long opacity = 0x80000000UL;

	XChangeProperty(app->dpy, app->overlay, opacity_atom, XA_CARDINAL, 32,
	                PropModeReplace, (unsigned char *)&opacity, 1);
}

static void draw_selection(App *app, int c1, int r1, int c2, int r2)
{
	int x, y, w, h;

	grid_rect_span(c1, r1, c2, r2, 0, 0, app->work_w, app->work_h, &x, &y, &w, &h);
	XSetForeground(app->dpy, app->overlay_gc, app->color_sel);
	XFillRectangle(app->dpy, app->overlay, app->overlay_gc,
	               x, y, (unsigned)w, (unsigned)h);
}

void overlay_create(App *app)
{
	XSetWindowAttributes attrs;

	app->color_bg = color_pixel(app, "#1a1a1a");
	app->color_grid = color_pixel(app, "#ffffff");
	app->color_sel = color_pixel(app, "#40e0d0");
	app->grab_cursor = XCreateFontCursor(app->dpy, XC_crosshair);

	attrs.override_redirect = True;
	attrs.event_mask = ExposureMask | ButtonPressMask | KeyPressMask |
	                   PointerMotionMask | StructureNotifyMask;
	attrs.background_pixel = app->color_bg;
	attrs.colormap = DefaultColormap(app->dpy, app->screen);
	app->overlay = XCreateWindow(
		app->dpy, app->root,
		0, 0, 1, 1, 0,
		DefaultDepth(app->dpy, app->screen), InputOutput,
		DefaultVisual(app->dpy, app->screen),
		CWOverrideRedirect | CWEventMask | CWBackPixel | CWColormap, &attrs);
	XStoreName(app->dpy, app->overlay, "jgk_tiles");
	app->overlay_gc = XCreateGC(app->dpy, app->overlay, 0, NULL);
	wm_set_state(app);
}

void overlay_redraw(App *app)
{
	int cw, rh, i;

	if (!app->overlay_visible)
		return;

	XSetForeground(app->dpy, app->overlay_gc, app->color_bg);
	XFillRectangle(app->dpy, app->overlay, app->overlay_gc,
	               0, 0, (unsigned)app->work_w, (unsigned)app->work_h);

	if (app->has_first) {
		int c2 = app->has_hover ? app->hover_col : app->first_col;
		int r2 = app->has_hover ? app->hover_row : app->first_row;
		draw_selection(app, app->first_col, app->first_row, c2, r2);
	} else if (app->has_hover) {
		draw_selection(app, app->hover_col, app->hover_row,
		               app->hover_col, app->hover_row);
	}

	XSetForeground(app->dpy, app->overlay_gc, app->color_grid);
	for (i = 1; i < GRID_COLS; i++) {
		cw = (int)((double)app->work_w * i / GRID_COLS);
		XFillRectangle(app->dpy, app->overlay, app->overlay_gc,
		               cw - 1, 0, 2, (unsigned)app->work_h);
	}
	for (i = 1; i < GRID_ROWS; i++) {
		rh = (int)((double)app->work_h * i / GRID_ROWS);
		XFillRectangle(app->dpy, app->overlay, app->overlay_gc,
		               0, rh - 1, (unsigned)app->work_w, 2);
	}
	XSync(app->dpy, False);
}

void overlay_show(App *app)
{
	bridge_store_focus();
	if (!bridge_get_workarea(&app->work_x, &app->work_y, &app->work_w, &app->work_h))
		window_get_workarea(app);
	grid_reset(app);
	window_get_active(app, &app->target);

	XMoveResizeWindow(app->dpy, app->overlay,
	                  app->work_x, app->work_y,
	                  (unsigned)app->work_w, (unsigned)app->work_h);
	wm_set_state(app);
	XMapRaised(app->dpy, app->overlay);
	XSetInputFocus(app->dpy, app->overlay, RevertToPointerRoot, CurrentTime);
	if (XGrabPointer(app->dpy, app->overlay, True,
	                 ButtonPressMask | ButtonReleaseMask,
	                 GrabModeAsync, GrabModeAsync, app->overlay,
	                 app->grab_cursor, CurrentTime) != GrabSuccess)
		XGrabPointer(app->dpy, app->overlay, True,
		             ButtonPressMask | ButtonReleaseMask,
		             GrabModeAsync, GrabModeAsync, app->overlay,
		             None, CurrentTime);
	XGrabKeyboard(app->dpy, app->overlay, True,
	              GrabModeAsync, GrabModeAsync, CurrentTime);
	app->overlay_visible = 1;
	overlay_redraw(app);
	XFlush(app->dpy);
}

void overlay_hide(App *app)
{
	Window refocus = app->target;

	if (!app->overlay_visible)
		return;

	app->overlay_visible = 0;
	grid_reset(app);
	XUngrabPointer(app->dpy, CurrentTime);
	XUngrabKeyboard(app->dpy, CurrentTime);
	XUnmapWindow(app->dpy, app->overlay);
	window_focus(app, refocus);
	XFlush(app->dpy);
}

void overlay_click(App *app, int px, int py)
{
	int col, row, x, y, w, h;

	if (!grid_hit(app, px, py, &col, &row))
		return;

	if (!app->has_first) {
		app->has_first = 1;
		app->first_col = col;
		app->first_row = row;
		overlay_redraw(app);
		return;
	}

	grid_rect(app, app->first_col, app->first_row, col, row, &x, &y, &w, &h);
	overlay_hide(app);
	window_apply_rect(app, x, y, w, h);
}
