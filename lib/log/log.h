#ifndef _LOG_H
#define _LOG_H

#include <stdio.h>

void pb_log(const char *fmt, ...);
void pb_log_set_stream(FILE *stream);

#endif /* _LOG_H */
