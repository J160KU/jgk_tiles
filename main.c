#define _DEFAULT_SOURCE
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <unistd.h>
#include "jgk_tiles.h"
#include "evdev.h"
#include "ipc.h"
#include "log.h"

#define POLL_MAX (2 + 32)

static int acquire_lock(void)
{
	char path[256];
	int fd;

	snprintf(path, sizeof(path), "%s/.cache/jgk_tiles.lock",
	         getenv("HOME") ? getenv("HOME") : "/tmp");
	fd = open(path, O_CREAT | O_RDWR, 0600);
	if (fd < 0)
		return -1;
	if (flock(fd, LOCK_EX | LOCK_NB) < 0) {
		close(fd);
		return -1;
	}
	return fd;
}

static int app_init(App *app)
{
	const char *session = getenv("XDG_SESSION_TYPE");

	if (!getenv("DISPLAY")) {
		fprintf(stderr, "jgk_tiles: needs $DISPLAY (X11 or XWayland)\n");
		return 0;
	}

	memset(app, 0, sizeof(*app));
	app->dpy = XOpenDisplay(NULL);
	if (!app->dpy) {
		fprintf(stderr, "jgk_tiles: XOpenDisplay failed\n");
		return 0;
	}

	app->screen = DefaultScreen(app->dpy);
	app->root = RootWindow(app->dpy, app->screen);
	app->net_active_window = XInternAtom(app->dpy, "_NET_ACTIVE_WINDOW", False);
	app->net_workarea = XInternAtom(app->dpy, "_NET_WORKAREA", False);
	app->net_move_resize = XInternAtom(app->dpy, "_NET_MOVERESIZE_WINDOW", False);
	app->esc_keycode = XKeysymToKeycode(app->dpy, XK_Escape);

	if (session && strcmp(session, "wayland") == 0 && getenv("WAYLAND_DISPLAY")) {
		app->use_wayland = wl_overlay_init(app);
		log_msg("wayland overlay init=%d", app->use_wayland);
	}
	if (!app->use_wayland)
		overlay_create(app);

	return 1;
}

static void handle_key(App *app, XKeyEvent *ev)
{
	if (hotkey_is_ctrl_esc(app, ev) || hotkey_is_esc(app, ev))
		overlay_toggle(app);
}

static void dispatch_x11(App *app)
{
	XEvent ev;

	while (XPending(app->dpy)) {
		XNextEvent(app->dpy, &ev);
		switch (ev.type) {
		case KeyPress:
			if (key_is_repeat(app->dpy, &ev.xkey))
				break;
			if (ev.xkey.window == app->root ||
			    (app->overlay_visible && !app->use_wayland))
				handle_key(app, &ev.xkey);
			break;
		case ButtonPress:
			if (!app->use_wayland && app->overlay_visible &&
			    ev.xbutton.window == app->overlay)
				overlay_click(app, ev.xbutton.x, ev.xbutton.y);
			break;
		case MotionNotify:
			if (!app->use_wayland && app->overlay_visible &&
			    ev.xmotion.window == app->overlay) {
				int col, row;
				if (grid_hit(app, ev.xmotion.x, ev.xmotion.y, &col, &row) &&
				    (!app->has_hover || col != app->hover_col ||
				     row != app->hover_row)) {
					app->has_hover = 1;
					app->hover_col = col;
					app->hover_row = row;
					overlay_redraw(app);
				}
			}
			break;
		case Expose:
			if (!app->use_wayland && app->overlay_visible &&
			    ev.xexpose.window == app->overlay &&
			    ev.xexpose.count == 0)
				overlay_redraw(app);
			break;
		case ConfigureNotify:
			if (!app->use_wayland && app->overlay_visible &&
			    ev.xconfigure.window == app->overlay)
				overlay_redraw(app);
			break;
		}
	}
}

static int run_daemon(void)
{
	App app;
	struct pollfd fds[POLL_MAX];
	int nfds, lock_fd, ev_start, i, use_evdev;

	lock_fd = acquire_lock();
	if (lock_fd < 0) {
		log_msg("already running");
		return 0;
	}

	log_init();
	log_msg("starting pid=%d DISPLAY=%s WAYLAND=%s",
	        (int)getpid(), getenv("DISPLAY") ? getenv("DISPLAY") : "-",
	        getenv("WAYLAND_DISPLAY") ? getenv("WAYLAND_DISPLAY") : "-");

	if (!app_init(&app))
		return 1;

	ipc_install_signals();
	ipc_write_pid();

	if (app.use_wayland) {
		use_evdev = 0;
		log_msg("hotkey via GNOME: jgk_tiles --toggle (Ctrl+Esc)");
	} else {
		use_evdev = evdev_init();
		if (!use_evdev)
			hotkey_grab_ctrl_esc(&app);
		log_msg("evdev=%d", use_evdev);
	}

	nfds = 0;
	fds[nfds].fd = ConnectionNumber(app.dpy);
	fds[nfds].events = POLLIN;
	nfds++;
	if (app.use_wayland) {
		fds[nfds].fd = wl_overlay_fd(&app);
		fds[nfds].events = POLLIN;
		nfds++;
	}
	ev_start = nfds;
	evdev_set_pollfds(fds, &nfds);

	for (;;) {
		if (ipc_toggle_requested())
			overlay_toggle(&app);

		while (XPending(app.dpy))
			dispatch_x11(&app);
		if (app.use_wayland)
			wl_overlay_poll(&app);

		for (i = 0; i < nfds; i++)
			fds[i].revents = 0;

		if (poll(fds, nfds, -1) < 0)
			continue;

		if (ipc_toggle_requested())
			overlay_toggle(&app);
		if (fds[0].revents & POLLIN)
			dispatch_x11(&app);
		if (app.use_wayland && nfds > 1 && (fds[1].revents & POLLIN))
			wl_overlay_poll(&app);
		if (use_evdev)
			evdev_poll(&app, fds + ev_start, nfds - ev_start);
	}

	evdev_shutdown();
	ipc_remove_pid();
	if (app.use_wayland)
		wl_overlay_destroy(&app);
	XCloseDisplay(app.dpy);
	return 0;
}

static int run_test_show(void)
{
	App app;
	int i;

	log_init();
	if (!app_init(&app))
		return 1;
	log_msg("test show for 3s wayland=%d", app.use_wayland);
	overlay_toggle(&app);
	for (i = 0; i < 30; i++) {
		if (app.use_wayland)
			wl_overlay_poll(&app);
		usleep(100000);
	}
	overlay_hide_any(&app);
	if (app.use_wayland)
		wl_overlay_destroy(&app);
	XCloseDisplay(app.dpy);
	return 0;
}

int main(int argc, char **argv)
{
	if (argc > 1 && strcmp(argv[1], "--toggle") == 0) {
		if (ipc_send_toggle())
			return 0;
		fprintf(stderr, "jgk_tiles: daemon not running\n");
		return 1;
	}
	if (argc > 1 && strcmp(argv[1], "--test-show") == 0)
		return run_test_show();
	return run_daemon();
}
