
#include <stdarg.h>

#include "log.h"

static FILE *logf;
static int always_flush;

void pb_log(const char *fmt, ...)
{
	va_list ap;
	FILE *stream;

	stream = logf ? logf : stderr;

	va_start(ap, fmt);
	vfprintf(stream, fmt, ap);
	va_end(ap);

	if (always_flush)
		fflush(stream);
}

void pb_log_set_stream(FILE *stream)
{
	fflush(logf ? logf : stderr);
	logf = stream;
}

FILE * pb_log_get_stream(void)
{
	return logf ? logf : stderr;
}

void pb_log_always_flush(int state)
{
	always_flush = state;
}
