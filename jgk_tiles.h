#ifndef JGK_TILES_H
#define JGK_TILES_H

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <X11/cursorfont.h>

#define GRID_COLS 16
#define GRID_ROWS 8

typedef struct App App;

typedef struct {
	struct wl_display *display;
	struct wl_surface *surface;
	struct xdg_surface *xdg_surface;
	struct xdg_toplevel *toplevel;
	int width, height;
	int visible;
	int has_first;
	int first_col, first_row;
} WlOverlay;

struct App {
	Display *dpy;
	Window root;
	int screen;
	Atom net_active_window;
	Atom net_workarea;
	Atom net_move_resize;
	Window target;
	Window overlay;
	GC overlay_gc;
	Cursor grab_cursor;
	int overlay_visible;
	int use_wayland;
	WlOverlay wl;
	int work_x, work_y, work_w, work_h;
	int has_first;
	int first_col, first_row;
	int has_hover;
	int hover_col, hover_row;
	KeyCode esc_keycode;
	unsigned long color_bg;
	unsigned long color_grid;
	unsigned long color_sel;
};

void grid_reset(App *app);
int grid_hit_span(int px, int py, int span_w, int span_h, int *col, int *row);
int grid_hit(const App *app, int px, int py, int *col, int *row);
void grid_rect_span(int c1, int r1, int c2, int r2,
                    int ox, int oy, int span_w, int span_h,
                    int *x, int *y, int *w, int *h);
void grid_rect(const App *app, int c1, int r1, int c2, int r2,
               int *x, int *y, int *w, int *h);

int window_get_active(App *app, Window *out);
int window_valid(App *app, Window w);
void window_move_resize(App *app, Window win, int x, int y, int width, int height);
void window_apply_rect(App *app, int x, int y, int width, int height);
void window_focus(App *app, Window w);
int window_get_workarea(App *app);

void overlay_create(App *app);
void overlay_show(App *app);
void overlay_hide(App *app);
void overlay_redraw(App *app);
void overlay_click(App *app, int px, int py);

int wl_overlay_init(App *app);
void wl_overlay_destroy(App *app);
void wl_overlay_show(App *app);
void wl_overlay_hide(App *app);
void wl_overlay_redraw(App *app);
void wl_overlay_click(App *app, int px, int py);
int wl_overlay_poll(App *app);
int wl_overlay_fd(App *app);

void hotkey_grab_ctrl_esc(App *app);
int hotkey_is_ctrl_esc(const App *app, const XKeyEvent *ev);
int hotkey_is_esc(const App *app, const XKeyEvent *ev);
int key_is_repeat(Display *dpy, XKeyEvent *ev);

void overlay_toggle(App *app);
void overlay_hide_any(App *app);

#endif
