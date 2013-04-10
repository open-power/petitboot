
#include <poll.h>
#include <string.h>
#include <assert.h>

#include <talloc/talloc.h>

#include "waiter.h"

struct waiter {
	struct waitset	*set;
	int		fd;
	int		events;
	waiter_cb	callback;
	void		*arg;
};

struct waitset {
	struct waiter	*waiters;
	int		n_waiters;
	struct pollfd	*pollfds;
	int		n_pollfds;
};

struct waitset *waitset_create(void *ctx)
{
	struct waitset *set = talloc_zero(ctx, struct waitset);
	return set;
}

void waitset_destroy(struct waitset *set)
{
	talloc_free(set);
}

struct waiter *waiter_register(struct waitset *set, int fd, int events,
		waiter_cb callback, void *arg)
{
	struct waiter *waiters, *waiter;

	waiters = talloc_realloc(set, set->waiters,
			struct waiter, set->n_waiters + 1);

	if (!waiters)
		return NULL;

	set->n_waiters++;
	set->waiters = waiters;

	waiter = &set->waiters[set->n_waiters - 1];

	waiter->set = set;
	waiter->fd = fd;
	waiter->events = events;
	waiter->callback = callback;
	waiter->arg = arg;

	return waiter;
}

void waiter_remove(struct waiter *waiter)
{
	struct waitset *set = waiter->set;
	int i;

	i = waiter - set->waiters;
	assert(i >= 0 && i < set->n_waiters);

	set->n_waiters--;
	memmove(&set->waiters[i], &set->waiters[i+1],
		(set->n_waiters - i) * sizeof(set->waiters[0]));

	set->waiters = talloc_realloc(set->waiters, set->waiters, struct waiter,
			set->n_waiters);
}

int waiter_poll(struct waitset *set)
{
	int i, rc;

	if (set->n_waiters != set->n_pollfds) {
		set->pollfds = talloc_realloc(set, set->pollfds,
				struct pollfd, set->n_waiters);
		set->n_pollfds = set->n_waiters;
	}

	for (i = 0; i < set->n_waiters; i++) {
		set->pollfds[i].fd = set->waiters[i].fd;
		set->pollfds[i].events = set->waiters[i].events;
		set->pollfds[i].revents = 0;
	}

	rc = poll(set->pollfds, set->n_waiters, -1);

	if (rc <= 0)
		return rc;

	for (i = 0; i < set->n_waiters; i++) {
		if (set->pollfds[i].revents) {
			rc = set->waiters[i].callback(set->waiters[i].arg);

			if (rc)
				waiter_remove(&set->waiters[i]);
		}
	}

	return 0;
}
