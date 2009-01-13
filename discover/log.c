
#include <stdarg.h>

#include "log.h"

static FILE *logf;

void pb_log(const char *fmt, ...)
{
	va_list ap;
	FILE *stream;

	stream = logf ? logf : stdout;

	va_start(ap, fmt);
	vfprintf(stream, fmt, ap);
	va_end(ap);
}

void pb_log_set_stream(FILE *stream)
{
	logf = stream;
}
