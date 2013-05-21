#ifndef _WAITER_H
#define _WAITER_H

#include <poll.h>

struct waiter;
struct waitset;

enum events {
	WAIT_IN  = POLLIN,
	WAIT_OUT = POLLOUT,
};

typedef int (*waiter_cb)(void *);

struct waitset *waitset_create(void *ctx);
void waitset_destroy(struct waitset *waitset);

struct waiter *waiter_register_io(struct waitset *waitset, int fd, int events,
		waiter_cb callback, void *arg);

struct waiter *waiter_register_timeout(struct waitset *set, int delay_ms,
		waiter_cb callback, void *arg);

void waiter_remove(struct waiter *waiter);

int waiter_poll(struct waitset *waitset);
#endif /* _WAITER_H */


