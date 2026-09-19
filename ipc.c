#define _DEFAULT_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "ipc.h"

static volatile sig_atomic_t toggle_req;

static void pid_path(char *buf, size_t len)
{
	const char *home = getenv("HOME");

	snprintf(buf, len, "%s/.cache/jgk_tiles.pid",
	         home ? home : "/tmp");
}

void ipc_set_toggle_flag(int sig)
{
	(void)sig;
	toggle_req = 1;
}

int ipc_toggle_requested(void)
{
	if (!toggle_req)
		return 0;
	toggle_req = 0;
	return 1;
}

void ipc_write_pid(void)
{
	char path[256];
	FILE *f;

	pid_path(path, sizeof(path));
	f = fopen(path, "w");
	if (!f)
		return;
	fprintf(f, "%d\n", (int)getpid());
	fclose(f);
}

void ipc_remove_pid(void)
{
	char path[256];

	pid_path(path, sizeof(path));
	unlink(path);
}

int ipc_send_toggle(void)
{
	char path[256];
	FILE *f;
	pid_t pid;

	pid_path(path, sizeof(path));
	f = fopen(path, "r");
	if (!f)
		return 0;
	if (fscanf(f, "%d", &pid) != 1) {
		fclose(f);
		return 0;
	}
	fclose(f);
	if (pid <= 0 || kill(pid, SIGUSR1) != 0)
		return 0;
	return 1;
}

void ipc_install_signals(void)
{
	signal(SIGUSR1, ipc_set_toggle_flag);
}
