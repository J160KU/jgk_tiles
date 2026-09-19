#include "log.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static FILE *logf;

void log_init(void)
{
	const char *home = getenv("HOME");
	char path[256];

	if (logf)
		return;
	snprintf(path, sizeof(path), "%s/.cache/jgk_tiles.log",
	         home ? home : "/tmp");
	logf = fopen(path, "a");
}

void log_msg(const char *fmt, ...)
{
	va_list ap;
	time_t now;
	char ts[32];

	if (!logf)
		log_init();
	if (!logf)
		return;

	time(&now);
	strftime(ts, sizeof(ts), "%H:%M:%S", localtime(&now));
	fprintf(logf, "[%s] ", ts);
	va_start(ap, fmt);
	vfprintf(logf, fmt, ap);
	va_end(ap);
	fputc('\n', logf);
	fflush(logf);
}
