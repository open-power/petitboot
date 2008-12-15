#ifndef _WAITER_H
#define _WAITER_H

#include <poll.h>

struct waiter;

enum events {
	WAIT_IN  = POLLIN,
	WAIT_OUT = POLLOUT,
};

typedef int (*waiter_cb)(void *);

struct waiter *waiter_register(int fd, int events,
		waiter_cb callback, void *arg);

void waiter_remove(struct waiter *waiter);

int waiter_poll(void);
#endif /* _WAITER_H */


