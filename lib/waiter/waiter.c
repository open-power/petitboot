
#include <poll.h>
#include <string.h>
#include <assert.h>

#include <talloc/talloc.h>

#include "waiter.h"

struct waiter {
	int		fd;
	int		events;
	waiter_cb	callback;
	void		*arg;
};

static struct waiter *waiters;
static int n_waiters;

struct waiter *waiter_register(int fd, int events,
		waiter_cb callback, void *arg)
{
	struct waiter *waiter;

	n_waiters++;

	waiters = talloc_realloc(NULL, waiters, struct waiter, n_waiters);
	waiter = &waiters[n_waiters - 1];

	waiter->fd = fd;
	waiter->events = events;
	waiter->callback = callback;
	waiter->arg = arg;

	return 0;
}

void waiter_remove(struct waiter *waiter)
{
	int i;

	i = waiter - waiters;
	assert(i >= 0 && i < n_waiters);

	n_waiters--;
	memmove(&waiters[i], &waiters[i+1], n_waiters - i);

	waiters = talloc_realloc(NULL, waiters, struct waiter, n_waiters);
}

int waiter_poll(void)
{
	static struct pollfd *pollfds;
	static int n_pollfds;
	int i, rc;

	if (n_waiters != n_pollfds) {
		pollfds = talloc_realloc(NULL, pollfds,
				struct pollfd, n_waiters);
		n_pollfds = n_waiters;
	}

	for (i = 0; i < n_waiters; i++) {
		pollfds[i].fd = waiters[i].fd;
		pollfds[i].events = waiters[i].events;
		pollfds[i].revents = 0;
	}

	rc = poll(pollfds, n_waiters, -1);

	if (rc <= 0)
		return rc;

	for (i = 0; i < n_waiters; i++) {
		if (pollfds[i].revents) {
			rc = waiters[i].callback(waiters[i].arg);

			if (rc)
				waiter_remove(&waiters[i]);
		}
	}

	return 0;
}
