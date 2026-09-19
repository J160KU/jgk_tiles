#include "evdev.h"
#include "log.h"
#include <dirent.h>
#include <fcntl.h>
#include <linux/input-event-codes.h>
#include <linux/input.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define EVDEV_MAX 8

static int ev_fds[EVDEV_MAX];
static int ev_count;
static int ctrl_down;

static int has_key(int fd, int key)
{
	unsigned long bits[(KEY_MAX + 1) / (8 * sizeof(long)) + 1];

	memset(bits, 0, sizeof(bits));
	if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(bits)), bits) < 0)
		return 0;
	return (bits[key / (8 * sizeof(long))] >> (key % (8 * sizeof(long)))) & 1;
}

static int is_keyboard(int fd)
{
	char name[256];

	if (!has_key(fd, KEY_ESC) || !has_key(fd, KEY_A) || !has_key(fd, KEY_LEFTCTRL))
		return 0;
	if (ioctl(fd, EVIOCGNAME(sizeof(name)), name) < 0)
		return 0;
	if (strstr(name, "Mouse") || strstr(name, "Consumer") ||
	    strstr(name, "System Control") || strstr(name, "Power Button") ||
	    strstr(name, "HDMI") || strstr(name, "Mic") || strstr(name, "Headphone"))
		return 0;
	return strstr(name, "Keyboard") || strstr(name, "keyboard");
}

int evdev_init(void)
{
	DIR *dir = opendir("/dev/input");
	struct dirent *ent;

	ev_count = 0;
	ctrl_down = 0;
	if (!dir)
		return 0;

	while ((ent = readdir(dir)) && ev_count < EVDEV_MAX) {
		char path[64];
		char name[256];
		int fd;

		if (strncmp(ent->d_name, "event", 5) != 0)
			continue;
		snprintf(path, sizeof(path), "/dev/input/%s", ent->d_name);
		fd = open(path, O_RDONLY | O_NONBLOCK);
		if (fd < 0 || !is_keyboard(fd)) {
			if (fd >= 0)
				close(fd);
			continue;
		}
		if (ioctl(fd, EVIOCGNAME(sizeof(name)), name) >= 0)
			log_msg("evdev keyboard: %s (%s)", name, path);
		ev_fds[ev_count++] = fd;
	}
	closedir(dir);
	return ev_count > 0;
}

void evdev_shutdown(void)
{
	int i;

	for (i = 0; i < ev_count; i++) {
		close(ev_fds[i]);
		ev_fds[i] = -1;
	}
	ev_count = 0;
	ctrl_down = 0;
}

void evdev_set_pollfds(struct pollfd *fds, int *nfds)
{
	int i;

	for (i = 0; i < ev_count; i++) {
		fds[*nfds].fd = ev_fds[i];
		fds[*nfds].events = POLLIN;
		(*nfds)++;
	}
}

static int is_ctrl(int code)
{
	return code == KEY_LEFTCTRL || code == KEY_RIGHTCTRL;
}

void evdev_handle(App *app, int fd)
{
	struct input_event ev;

	(void)fd;
	while (read(fd, &ev, sizeof(ev)) == (ssize_t)sizeof(ev)) {
		if (ev.type != EV_KEY)
			continue;
		if (is_ctrl(ev.code)) {
			ctrl_down = ev.value ? 1 : 0;
			continue;
		}
		if (ev.code != KEY_ESC || !ev.value)
			continue;
		if (ctrl_down) {
			log_msg("evdev Ctrl+Esc");
			overlay_toggle(app);
		} else if (app->overlay_visible) {
			log_msg("evdev Esc");
			overlay_hide_any(app);
		}
	}
}

void evdev_poll(App *app, struct pollfd *fds, int nfds)
{
	int i;

	for (i = 0; i < nfds; i++) {
		if (!(fds[i].revents & POLLIN))
			continue;
		evdev_handle(app, fds[i].fd);
	}
}
