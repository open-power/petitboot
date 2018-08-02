#ifndef _LOG_H
#define _LOG_H

#include <stdbool.h>
#include <stdio.h>

void __attribute__ ((format (printf, 1, 2))) pb_log(const char *fmt, ...);
void __attribute__ ((format (printf, 2, 3))) _pb_log_fn(const char *func,
	const char *fmt, ...);
#define pb_log_fn(args...) _pb_log_fn(__func__, args)

void __attribute__ ((format (printf, 1, 2))) pb_debug(const char *fmt, ...);
void __attribute__ ((format (printf, 2, 3))) _pb_debug_fn(const char *func,
	const char *fmt, ...);
#define pb_debug_fn(args...) _pb_debug_fn(__func__, args)
void __attribute__ ((format (printf, 3, 4))) _pb_debug_fl(const char *func,
	int line, const char *fmt, ...);
#define pb_debug_fl(args...) _pb_debug_fl(__func__, __LINE__, args)

void __pb_log_init(FILE *stream, bool debug);

#ifdef DEBUG
#define pb_log_init(s) __pb_log_init(s, true)
#else
#define pb_log_init(s) __pb_log_init(s, false)
#endif

void pb_log_set_debug(bool debug);
FILE *pb_log_get_stream(void);

#endif /* _LOG_H */
