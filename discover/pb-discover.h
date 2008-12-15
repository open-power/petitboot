#ifndef _PB_DISCOVER_H
#define _PB_DISCOVER_H

int register_waiter(int fd, int *callback(void *), void *arg);

#endif /* _PB_DISCOVER_H */
