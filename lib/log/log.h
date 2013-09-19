#ifndef _LOG_H
#define _LOG_H

#include <stdbool.h>
#include <stdio.h>

void __attribute__ ((format (printf, 1, 2))) pb_log(const char *fmt, ...);
void __attribute__ ((format (printf, 1, 2))) pb_debug(const char *fmt, ...);

void __pb_log_init(FILE *stream, bool debug);

#ifdef DEBUG
#define pb_log_init(s) __pb_log_init(s, true)
#else
#define pb_log_init(s) __pb_log_init(s, false)
#endif

FILE *pb_log_get_stream(void);

#endif /* _LOG_H */
