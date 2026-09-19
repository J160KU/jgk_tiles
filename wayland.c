#define _GNU_SOURCE
#include "jgk_tiles.h"
#include "bridge.h"
#include "log.h"
#include "xdg-shell-client-protocol.h"
#include "xdg-decoration-unstable-v1-client-protocol.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/input-event-codes.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <wayland-client.h>

struct wl_state {
	struct wl_compositor *compositor;
	struct wl_shm *shm;
	struct xdg_wm_base *wm_base;
	struct zxdg_decoration_manager_v1 *deco_mgr;
	struct zxdg_toplevel_decoration_v1 *deco;
	struct wl_seat *seat;
	struct wl_pointer *pointer;
	struct wl_keyboard *keyboard;
	struct wl_buffer *buffer;
	void *data;
	size_t size;
	int configured;
	int ptr_x, ptr_y;
	int want_show;
	int buf_w, buf_h;
};

static struct wl_state g_wl;
static App *g_app;

static int wl_buffer_resize(App *app);
static void wl_draw(App *app);

static int anon_fd(off_t size)
{
	int fd;

#ifdef __linux__
	fd = memfd_create("jgk_tiles", 0);
	if (fd >= 0) {
		if (ftruncate(fd, size) < 0) {
			close(fd);
			return -1;
		}
		return fd;
	}
#endif
	char path[] = "/tmp/jgk-tiles-shm-XXXXXX";
	fd = mkstemp(path);
	if (fd < 0)
		return -1;
	unlink(path);
	if (ftruncate(fd, size) < 0) {
		close(fd);
		return -1;
	}
	return fd;
}

static void xdg_wm_base_ping(void *data, struct xdg_wm_base *wm, uint32_t serial)
{
	(void)data;
	xdg_wm_base_pong(wm, serial);
}

static const struct xdg_wm_base_listener wm_listener = { xdg_wm_base_ping };

static void xdg_surface_configure(void *data, struct xdg_surface *surf, uint32_t serial)
{
	App *app = data;

	xdg_surface_ack_configure(surf, serial);
	log_msg("xdg configure serial=%u want_show=%d size=%dx%d",
	        serial, g_wl.want_show, app->wl.width, app->wl.height);
	if (!app->overlay_visible && !g_wl.want_show) {
		wl_surface_commit(app->wl.surface);
		wl_display_flush(app->wl.display);
		return;
	}
	if (app->wl.width <= 0 || app->wl.height <= 0)
		return;
	if (g_wl.buf_w != app->wl.width || g_wl.buf_h != app->wl.height) {
		if (!wl_buffer_resize(app)) {
			log_msg("buffer resize failed");
			return;
		}
	}
	if (!g_wl.data) {
		log_msg("buffer missing");
		return;
	}
	wl_draw(app);
	g_wl.want_show = 0;
}

static const struct xdg_surface_listener xdg_surface_listener = {
	xdg_surface_configure
};

static void toplevel_configure(void *data, struct xdg_toplevel *top,
                               int32_t w, int32_t h, struct wl_array *states)
{
	App *app = data;
	(void)top;
	(void)states;
	if (w > 0 && h > 0) {
		app->wl.width = w;
		app->wl.height = h;
	}
	app->wl.visible = 1;
	g_wl.configured = 1;
}

static void toplevel_closed(void *data, struct xdg_toplevel *top)
{
	App *app = data;
	(void)top;
	app->wl.visible = 0;
}

static void toplevel_configure_bounds(void *data, struct xdg_toplevel *top,
                                      int32_t w, int32_t h)
{
	(void)data;
	(void)top;
	(void)w;
	(void)h;
}

static void toplevel_wm_capabilities(void *data, struct xdg_toplevel *top,
                                       struct wl_array *caps)
{
	(void)data;
	(void)top;
	(void)caps;
}

static const struct xdg_toplevel_listener toplevel_listener = {
	toplevel_configure, toplevel_closed, toplevel_configure_bounds,
	toplevel_wm_capabilities
};

static void hover_update(int px, int py)
{
	int col, row;

	if (!g_app || !g_app->overlay_visible)
		return;
	if (!grid_hit_span(px, py, g_app->wl.width, g_app->wl.height, &col, &row))
		return;
	if (g_app->has_hover && g_app->hover_col == col && g_app->hover_row == row)
		return;
	g_app->has_hover = 1;
	g_app->hover_col = col;
	g_app->hover_row = row;
	wl_draw(g_app);
}

static void pointer_enter(void *data, struct wl_pointer *pointer, uint32_t serial,
                          struct wl_surface *surface, wl_fixed_t sx, wl_fixed_t sy)
{
	(void)data;
	(void)pointer;
	(void)serial;
	(void)surface;
	g_wl.ptr_x = wl_fixed_to_int(sx);
	g_wl.ptr_y = wl_fixed_to_int(sy);
	hover_update(g_wl.ptr_x, g_wl.ptr_y);
}

static void pointer_leave(void *data, struct wl_pointer *pointer, uint32_t serial,
                          struct wl_surface *surface)
{
	(void)data;
	(void)pointer;
	(void)serial;
	(void)surface;
}

static void pointer_motion(void *data, struct wl_pointer *pointer,
                           uint32_t time, wl_fixed_t sx, wl_fixed_t sy)
{
	(void)data;
	(void)pointer;
	(void)time;
	g_wl.ptr_x = wl_fixed_to_int(sx);
	g_wl.ptr_y = wl_fixed_to_int(sy);
	hover_update(g_wl.ptr_x, g_wl.ptr_y);
}

static void pointer_button(void *data, struct wl_pointer *pointer, uint32_t serial,
                           uint32_t time, uint32_t button, uint32_t state)
{
	(void)data;
	(void)pointer;
	(void)serial;
	(void)time;
	if (state != WL_POINTER_BUTTON_STATE_PRESSED || button != BTN_LEFT || !g_app)
		return;
	if (g_app->overlay_visible) {
		log_msg("pointer click %d,%d", g_wl.ptr_x, g_wl.ptr_y);
		wl_overlay_click(g_app, g_wl.ptr_x, g_wl.ptr_y);
	}
}

static void pointer_axis(void *data, struct wl_pointer *wl_pointer, uint32_t time,
                         uint32_t axis, wl_fixed_t value)
{
	(void)data;
	(void)wl_pointer;
	(void)time;
	(void)axis;
	(void)value;
}

static void pointer_frame(void *data, struct wl_pointer *wl_pointer)
{
	(void)data;
	(void)wl_pointer;
}

static void pointer_axis_source(void *data, struct wl_pointer *wl_pointer, uint32_t axis)
{
	(void)data;
	(void)wl_pointer;
	(void)axis;
}

static void pointer_axis_stop(void *data, struct wl_pointer *wl_pointer, uint32_t time,
                              uint32_t axis)
{
	(void)data;
	(void)wl_pointer;
	(void)time;
	(void)axis;
}

static void pointer_axis_discrete(void *data, struct wl_pointer *wl_pointer,
                                  uint32_t axis, int32_t discrete)
{
	(void)data;
	(void)wl_pointer;
	(void)axis;
	(void)discrete;
}

static void pointer_axis_value120(void *data, struct wl_pointer *wl_pointer,
                                  uint32_t axis, int32_t value120)
{
	(void)data;
	(void)wl_pointer;
	(void)axis;
	(void)value120;
}

static void pointer_axis_relative_direction(void *data, struct wl_pointer *wl_pointer,
                                            uint32_t axis, uint32_t direction)
{
	(void)data;
	(void)wl_pointer;
	(void)axis;
	(void)direction;
}

static const struct wl_pointer_listener pointer_listener = {
	pointer_enter, pointer_leave, pointer_motion,
	pointer_button, pointer_axis, pointer_frame,
	pointer_axis_source, pointer_axis_stop,
	pointer_axis_discrete, pointer_axis_value120,
	pointer_axis_relative_direction
};

static void keyboard_keymap(void *data, struct wl_keyboard *kb, uint32_t fmt,
                            int fd, uint32_t size)
{
	(void)data;
	(void)kb;
	(void)fmt;
	close(fd);
	(void)size;
}

static void keyboard_enter(void *data, struct wl_keyboard *kb, uint32_t serial,
                           struct wl_surface *surface, struct wl_array *keys)
{
	(void)data;
	(void)kb;
	(void)serial;
	(void)surface;
	(void)keys;
}

static void keyboard_leave(void *data, struct wl_keyboard *kb, uint32_t serial,
                           struct wl_surface *surface)
{
	(void)data;
	(void)kb;
	(void)serial;
	(void)surface;
}

static void keyboard_key(void *data, struct wl_keyboard *kb, uint32_t serial,
                         uint32_t time, uint32_t key, uint32_t state)
{
	(void)data;
	(void)kb;
	(void)serial;
	(void)time;
	if (state != WL_KEYBOARD_KEY_STATE_PRESSED || !g_app || !g_app->overlay_visible)
		return;
	if (key == KEY_ESC || key == (uint32_t)KEY_ESC + 8U)
		overlay_hide_any(g_app);
}

static void keyboard_modifiers(void *data, struct wl_keyboard *kb, uint32_t serial,
                               uint32_t mods_depressed, uint32_t mods_latched,
                               uint32_t mods_locked, uint32_t group)
{
	(void)data;
	(void)kb;
	(void)serial;
	(void)mods_depressed;
	(void)mods_latched;
	(void)mods_locked;
	(void)group;
}

static void keyboard_repeat_info(void *data, struct wl_keyboard *kb,
                                 int32_t rate, int32_t delay)
{
	(void)data;
	(void)kb;
	(void)rate;
	(void)delay;
}

static const struct wl_keyboard_listener keyboard_listener = {
	keyboard_keymap, keyboard_enter, keyboard_leave,
	keyboard_key, keyboard_modifiers, keyboard_repeat_info
};

static void seat_capabilities(void *data, struct wl_seat *seat, uint32_t caps)
{
	(void)data;
	if ((caps & WL_SEAT_CAPABILITY_POINTER) && !g_wl.pointer) {
		g_wl.pointer = wl_seat_get_pointer(seat);
		wl_pointer_add_listener(g_wl.pointer, &pointer_listener, NULL);
	}
	if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !g_wl.keyboard) {
		g_wl.keyboard = wl_seat_get_keyboard(seat);
		wl_keyboard_add_listener(g_wl.keyboard, &keyboard_listener, NULL);
	}
}

static void seat_name(void *data, struct wl_seat *seat, const char *name)
{
	(void)data;
	(void)seat;
	(void)name;
}

static const struct wl_seat_listener seat_listener = {
	seat_capabilities, seat_name
};

static void registry_global(void *data, struct wl_registry *reg,
                            uint32_t name, const char *iface, uint32_t version)
{
	(void)version;
	(void)data;
	if (!strcmp(iface, "wl_compositor"))
		g_wl.compositor = wl_registry_bind(reg, name, &wl_compositor_interface, 4);
	else if (!strcmp(iface, "wl_shm"))
		g_wl.shm = wl_registry_bind(reg, name, &wl_shm_interface, 1);
	else if (!strcmp(iface, "xdg_wm_base"))
		g_wl.wm_base = wl_registry_bind(reg, name, &xdg_wm_base_interface, 1);
	else if (!strcmp(iface, "zxdg_decoration_manager_v1"))
		g_wl.deco_mgr = wl_registry_bind(reg, name,
		        &zxdg_decoration_manager_v1_interface, 1);
	else if (!strcmp(iface, "wl_seat"))
		g_wl.seat = wl_registry_bind(reg, name, &wl_seat_interface, 5);
}

static void registry_global_remove(void *data, struct wl_registry *reg, uint32_t name)
{
	(void)data;
	(void)reg;
	(void)name;
}

static const struct wl_registry_listener registry_listener = {
	registry_global, registry_global_remove
};

/* Wayland ARGB8888 is premultiplied: RGB' = RGB * A / 255 */
static uint32_t premul(unsigned a, unsigned r, unsigned g, unsigned b)
{
	return (a << 24) | (((r * a) / 255) << 16) | (((g * a) / 255) << 8) | ((b * a) / 255);
}

static void fill_rect(uint32_t *px, int stride, int max_w, int max_h,
                     int x, int y, int w, int h, uint32_t color)
{
	int i, j;

	if (x < 0) {
		w += x;
		x = 0;
	}
	if (y < 0) {
		h += y;
		y = 0;
	}
	if (x >= max_w || y >= max_h)
		return;
	if (x + w > max_w)
		w = max_w - x;
	if (y + h > max_h)
		h = max_h - y;
	if (w <= 0 || h <= 0)
		return;
	for (j = 0; j < h; j++) {
		uint32_t *row = px + (y + j) * stride + x;
		for (i = 0; i < w; i++)
			row[i] = color;
	}
}

static void stroke_rect(uint32_t *px, int stride, int max_w, int max_h,
                       int x, int y, int w, int h, int t, uint32_t color)
{
	fill_rect(px, stride, max_w, max_h, x, y, w, t, color);
	fill_rect(px, stride, max_w, max_h, x, y + h - t, w, t, color);
	fill_rect(px, stride, max_w, max_h, x, y, t, h, color);
	fill_rect(px, stride, max_w, max_h, x + w - t, y, t, h, color);
}

static void wl_draw(App *app)
{
	WlOverlay *wl = &app->wl;
	int w = wl->width;
	int h = wl->height;
	int cw, rh, i;
	int sx, sy, sw, sh;
	int c2, r2;
	uint32_t *px;
	int stride;
	uint32_t col_bg = premul(0x80, 0, 0, 0);
	uint32_t col_hover = premul(0x90, 0xFF, 0x00, 0xFF);
	uint32_t col_hover_border = premul(0xD0, 0xFF, 0x40, 0xFF);
	uint32_t col_sel = premul(0xC0, 0x40, 0xE0, 0xD0);
	uint32_t col_sel_border = premul(0xF0, 0x40, 0xE0, 0xD0);
	uint32_t col_grid = premul(0x55, 0xFF, 0xFF, 0xFF);

	if (!g_wl.data || w <= 0 || h <= 0)
		return;

	stride = w;
	px = g_wl.data;
	fill_rect(px, stride, w, h, 0, 0, w, h, col_bg);

	if (wl->has_first) {
		c2 = app->has_hover ? app->hover_col : wl->first_col;
		r2 = app->has_hover ? app->hover_row : wl->first_row;
		grid_rect_span(wl->first_col, wl->first_row, c2, r2,
		               0, 0, w, h, &sx, &sy, &sw, &sh);
		fill_rect(px, stride, w, h, sx, sy, sw, sh, col_sel);
		stroke_rect(px, stride, w, h, sx, sy, sw, sh, 4, col_sel_border);
	} else if (app->has_hover) {
		grid_rect_span(app->hover_col, app->hover_row,
		               app->hover_col, app->hover_row,
		               0, 0, w, h, &sx, &sy, &sw, &sh);
		fill_rect(px, stride, w, h, sx, sy, sw, sh, col_hover);
		stroke_rect(px, stride, w, h, sx, sy, sw, sh, 3, col_hover_border);
	}

	for (i = 1; i < GRID_COLS; i++) {
		cw = (int)((double)w * i / GRID_COLS);
		fill_rect(px, stride, w, h, cw - 1, 0, 2, h, col_grid);
	}
	for (i = 1; i < GRID_ROWS; i++) {
		rh = (int)((double)h * i / GRID_ROWS);
		fill_rect(px, stride, w, h, 0, rh - 1, w, 2, col_grid);
	}

	wl_surface_attach(wl->surface, g_wl.buffer, 0, 0);
	wl_surface_damage_buffer(wl->surface, 0, 0, w, h);
	wl_surface_set_opaque_region(wl->surface, NULL);
	wl_surface_commit(wl->surface);
	wl_display_flush(app->wl.display);
}

static int wl_buffer_resize(App *app)
{
	WlOverlay *wl = &app->wl;
	int w = wl->width;
	int h = wl->height;
	size_t size = (size_t)w * (size_t)h * 4;
	int fd;
	struct wl_shm_pool *pool;

	if (g_wl.buffer) {
		wl_buffer_destroy(g_wl.buffer);
		g_wl.buffer = NULL;
	}
	if (g_wl.data && g_wl.data != MAP_FAILED)
		munmap(g_wl.data, g_wl.size);
	g_wl.data = NULL;
	g_wl.size = 0;

	fd = anon_fd((off_t)size);
	if (fd < 0) {
		log_msg("buffer fd fail %dx%d", w, h);
		return 0;
	}

	g_wl.data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (g_wl.data == MAP_FAILED) {
		log_msg("buffer mmap fail %dx%d", w, h);
		close(fd);
		return 0;
	}
	g_wl.size = size;

	pool = wl_shm_create_pool(g_wl.shm, fd, (int32_t)size);
	g_wl.buffer = wl_shm_pool_create_buffer(pool, 0, w, h, w * 4, WL_SHM_FORMAT_ARGB8888);
	wl_shm_pool_destroy(pool);
	close(fd);
	g_wl.buf_w = w;
	g_wl.buf_h = h;
	return 1;
}

static void wl_registry_setup(App *app)
{
	struct wl_registry *reg = wl_display_get_registry(app->wl.display);

	g_app = app;
	wl_registry_add_listener(reg, &registry_listener, app);
	wl_display_roundtrip(app->wl.display);
	if (g_wl.wm_base)
		xdg_wm_base_add_listener(g_wl.wm_base, &wm_listener, app);
	if (g_wl.seat)
		wl_seat_add_listener(g_wl.seat, &seat_listener, app);
	wl_display_roundtrip(app->wl.display);
}

int wl_overlay_init(App *app)
{
	WlOverlay *wl = &app->wl;

	memset(&g_wl, 0, sizeof(g_wl));
	wl->display = wl_display_connect(NULL);
	if (!wl->display)
		return 0;

	wl_registry_setup(app);
	if (!g_wl.compositor || !g_wl.shm || !g_wl.wm_base) {
		wl_overlay_destroy(app);
		return 0;
	}

	wl->surface = wl_compositor_create_surface(g_wl.compositor);
	wl_surface_set_opaque_region(wl->surface, NULL);
	wl->xdg_surface = xdg_wm_base_get_xdg_surface(g_wl.wm_base, wl->surface);
	xdg_surface_add_listener(wl->xdg_surface, &xdg_surface_listener, app);
	wl->toplevel = xdg_surface_get_toplevel(wl->xdg_surface);
	xdg_toplevel_set_app_id(wl->toplevel, "jgk_tiles");
	xdg_toplevel_set_title(wl->toplevel, "jgk_tiles");
	xdg_toplevel_add_listener(wl->toplevel, &toplevel_listener, app);
	if (g_wl.deco_mgr) {
		g_wl.deco = zxdg_decoration_manager_v1_get_toplevel_decoration(
			g_wl.deco_mgr, wl->toplevel);
		zxdg_toplevel_decoration_v1_set_mode(
			g_wl.deco, ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE);
		log_msg("xdg decoration client-side");
	}
	wl_surface_commit(wl->surface);
	wl_display_roundtrip(wl->display);

	window_get_workarea(app);
	wl->width = app->work_w;
	wl->height = app->work_h;
	if (!wl_buffer_resize(app)) {
		wl_overlay_destroy(app);
		return 0;
	}
	return 1;
}

void wl_overlay_destroy(App *app)
{
	WlOverlay *wl = &app->wl;

	if (g_wl.buffer)
		wl_buffer_destroy(g_wl.buffer);
	if (g_wl.data && g_wl.data != MAP_FAILED)
		munmap(g_wl.data, g_wl.size);
	if (g_wl.deco)
		zxdg_toplevel_decoration_v1_destroy(g_wl.deco);
	if (wl->toplevel)
		xdg_toplevel_destroy(wl->toplevel);
	if (wl->xdg_surface)
		xdg_surface_destroy(wl->xdg_surface);
	if (wl->surface)
		wl_surface_destroy(wl->surface);
	if (g_wl.deco_mgr)
		zxdg_decoration_manager_v1_destroy(g_wl.deco_mgr);
	if (g_wl.wm_base)
		xdg_wm_base_destroy(g_wl.wm_base);
	if (wl->display)
		wl_display_disconnect(wl->display);
	memset(&g_wl, 0, sizeof(g_wl));
	memset(wl, 0, sizeof(*wl));
}

void wl_overlay_show(App *app)
{
	WlOverlay *wl = &app->wl;

	log_msg("wl_overlay_show");
	bridge_store_focus();
	if (!bridge_get_workarea(&app->work_x, &app->work_y, &app->work_w, &app->work_h))
		window_get_workarea(app);
	log_msg("workarea %d,%d %dx%d", app->work_x, app->work_y, app->work_w, app->work_h);
	grid_reset(app);
	window_get_active(app, &app->target);
	wl->has_first = 0;
	g_wl.want_show = 1;
	app->overlay_visible = 1;
	wl->visible = 1;

	xdg_toplevel_set_maximized(wl->toplevel);
	wl_display_flush(wl->display);
}

void wl_overlay_hide(App *app)
{
	Window refocus = app->target;

	if (!app->overlay_visible)
		return;

	app->overlay_visible = 0;
	app->wl.visible = 0;
	app->wl.has_first = 0;
	g_wl.want_show = 0;
	grid_reset(app);
	xdg_toplevel_unset_fullscreen(app->wl.toplevel);
	xdg_toplevel_unset_maximized(app->wl.toplevel);
	wl_surface_attach(app->wl.surface, NULL, 0, 0);
	wl_surface_commit(app->wl.surface);
	wl_display_flush(app->wl.display);
	window_focus(app, refocus);
}

void wl_overlay_redraw(App *app)
{
	if (app->overlay_visible)
		wl_draw(app);
}

void wl_overlay_click(App *app, int px, int py)
{
	WlOverlay *wl = &app->wl;
	int col, row, x, y, w, h;

	if (!grid_hit_span(px, py, wl->width, wl->height, &col, &row))
		return;

	log_msg("click tile col=%d row=%d first=%d", col, row, wl->has_first);

	if (!wl->has_first) {
		wl->has_first = 1;
		wl->first_col = col;
		wl->first_row = row;
		app->has_first = 1;
		app->first_col = col;
		app->first_row = row;
		wl_draw(app);
		return;
	}

	grid_rect(app, wl->first_col, wl->first_row, col, row, &x, &y, &w, &h);
	log_msg("apply %d,%d %dx%d work=%d,%d %dx%d",
	        x, y, w, h, app->work_x, app->work_y, app->work_w, app->work_h);
	wl_overlay_hide(app);
	window_apply_rect(app, x, y, w, h);
}

int wl_overlay_fd(App *app)
{
	if (!app->wl.display)
		return -1;
	return wl_display_get_fd(app->wl.display);
}

int wl_overlay_poll(App *app)
{
	struct pollfd pfd;
	int ret;

	if (!app->wl.display)
		return 0;

	while (wl_display_prepare_read(app->wl.display) != 0) {
		if (wl_display_dispatch_pending(app->wl.display) < 0)
			return -1;
		wl_display_flush(app->wl.display);
	}
	wl_display_flush(app->wl.display);

	pfd.fd = wl_display_get_fd(app->wl.display);
	pfd.events = POLLIN;
	ret = poll(&pfd, 1, 0);
	if (ret <= 0) {
		wl_display_cancel_read(app->wl.display);
		return 0;
	}

	if (wl_display_read_events(app->wl.display) < 0)
		return -1;
	ret = wl_display_dispatch_pending(app->wl.display);
	wl_display_flush(app->wl.display);
	return ret;
}
