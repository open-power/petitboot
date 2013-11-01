
#include <assert.h>
#include <stdarg.h>

#include "log.h"

static FILE *logf;
static bool debug;

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
	__log(fmt, ap);
	va_end(ap);
}

void pb_debug(const char *fmt, ...)
{
	va_list ap;
	if (!debug)
		return;
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
