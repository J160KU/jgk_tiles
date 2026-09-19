#define _DEFAULT_SOURCE
#include "bridge.h"
#include "log.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

static void socket_path(char *buf, size_t len)
{
	const char *rt = getenv("XDG_RUNTIME_DIR");

	snprintf(buf, len, "%s/jgk_tiles.sock", rt ? rt : "/tmp");
}

static int bridge_connect(void)
{
	char path[108];
	struct sockaddr_un addr;
	int fd, i;

	socket_path(path, sizeof(path));
	for (i = 0; i < 3; i++) {
		fd = socket(AF_UNIX, SOCK_STREAM, 0);
		if (fd < 0) {
			log_msg("bridge socket: %s", strerror(errno));
			return -1;
		}
		memset(&addr, 0, sizeof(addr));
		addr.sun_family = AF_UNIX;
		strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);
		if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == 0)
			return fd;
		log_msg("bridge connect %s: %s", path, strerror(errno));
		close(fd);
		if (i < 2)
			usleep(40000);
	}
	return -1;
}

static int bridge_transact(const char *cmd, char *reply, size_t reply_len)
{
	int fd, n;

	fd = bridge_connect();
	if (fd < 0)
		return 0;

	n = (int)strlen(cmd);
	if (write(fd, cmd, (size_t)n) != n) {
		log_msg("bridge write: %s", strerror(errno));
		close(fd);
		return 0;
	}

	n = read(fd, reply, reply_len - 1);
	close(fd);
	if (n <= 0)
		return 0;
	reply[n] = '\0';
	return 1;
}

static int bridge_send(const char *cmd)
{
	char reply[16];

	if (!bridge_transact(cmd, reply, sizeof(reply)))
		return 0;
	return reply[0] == 'O';
}

int bridge_available(void)
{
	return bridge_send("PING\n");
}

int bridge_store_focus(void)
{
	int ok = bridge_send("STORE_FOCUS\n");

	if (ok)
		log_msg("bridge STORE_FOCUS ok");
	else
		log_msg("bridge STORE_FOCUS failed");
	return ok;
}

int bridge_get_workarea(int *x, int *y, int *w, int *h)
{
	char reply[80];

	if (!bridge_transact("WORKAREA\n", reply, sizeof(reply)))
		return 0;
	if (sscanf(reply, "OK %d %d %d %d", x, y, w, h) != 4)
		return 0;
	if (*w <= 0 || *h <= 0)
		return 0;
	log_msg("bridge WORKAREA %d,%d %dx%d", *x, *y, *w, *h);
	return 1;
}

int bridge_overlay_toggle(void)
{
	int ok = bridge_send("OVERLAY_TOGGLE\n");

	log_msg("bridge OVERLAY_TOGGLE -> %s", ok ? "ok" : "fail");
	return ok;
}

int bridge_overlay_hide(void)
{
	int ok = bridge_send("OVERLAY_HIDE\n");

	log_msg("bridge OVERLAY_HIDE -> %s", ok ? "ok" : "fail");
	return ok;
}

int bridge_resize(int x, int y, int w, int h)
{
	char cmd[64];
	int ok;

	snprintf(cmd, sizeof(cmd), "RESIZE %d %d %d %d\n", x, y, w, h);
	ok = bridge_send(cmd);
	log_msg("bridge RESIZE %d %d %d %d -> %s", x, y, w, h, ok ? "ok" : "fail");
	return ok;
}
