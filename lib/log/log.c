
#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>
#include <time.h>

#include "log.h"

static FILE *logf;
static bool debug;

static void __log_timestamp(void)
{
	char hms[20] = {'\0'};
	time_t t;

	if (!logf)
		return;

	t = time(NULL);
	strftime(hms, sizeof(hms), "%T", localtime(&t));
	fprintf(logf, "[%s] ", hms);
}

static void __log(const char *fmt, va_list ap)
{
	if (!logf)
		return;

	vfprintf(logf, fmt, ap);
	fflush(logf);
}

void pb_log(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	__log_timestamp();
	__log(fmt, ap);
	va_end(ap);
}

void _pb_log_fn(const char *func, const char *fmt, ...)
{
	va_list ap;
	pb_log("%s: ", func);
	va_start(ap, fmt);
	__log(fmt, ap);
	va_end(ap);
}

void pb_debug(const char *fmt, ...)
{
	va_list ap;
	if (!debug)
		return;
	va_start(ap, fmt);
	__log_timestamp();
	__log(fmt, ap);
	va_end(ap);
}

void _pb_debug_fn(const char *func, const char *fmt, ...)
{
	va_list ap;
	if (!debug)
		return;
	pb_log("%s: ", func);
	va_start(ap, fmt);
	__log(fmt, ap);
	va_end(ap);
}

void _pb_debug_fl(const char *func, int line, const char *fmt, ...)
{
	va_list ap;
	if (!debug)
		return;
	pb_log("%s:%d: ", func, line);
	va_start(ap, fmt);
	__log(fmt, ap);
	va_end(ap);
}

void __pb_log_init(FILE *fp, bool _debug)
{
	if (logf)
		fflush(logf);
	logf = fp;
	debug = _debug;
}

void pb_log_set_debug(bool _debug)
{
	debug = _debug;
}

bool pb_log_get_debug(void)
{
	return debug;
}

FILE *pb_log_get_stream(void)
{
	static FILE *null_stream;
	if (!logf) {
		if (!null_stream)
			null_stream = fopen("/dev/null", "a");
		return null_stream;
	}
	return logf;
}
