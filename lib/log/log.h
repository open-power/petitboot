#ifndef _LOG_H
#define _LOG_H

#include <stdio.h>

void pb_log(const char *fmt, ...);
void pb_log_set_stream(FILE *stream);
FILE * pb_log_get_stream(void);
void pb_log_always_flush(int state);

#endif /* _LOG_H */
