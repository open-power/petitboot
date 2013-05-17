
#include <poll.h>
#include <stdbool.h>
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
	struct waiter	**waiters;
	int		n_waiters;
	bool		waiters_changed;

	/* These are kept consistent over each call to waiter_poll, as
	 * set->waiters may be updated (by waiters' callbacks calling
	 * waiter_register or waiter_remove) during iteration. */
	struct pollfd	*pollfds;
	struct waiter	**cur_waiters;
	int		cur_n_waiters;
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
	struct waiter **waiters, *waiter;

	waiter = talloc(set->waiters, struct waiter);
	if (!waiter)
		return NULL;

	waiters = talloc_realloc(set, set->waiters,
			struct waiter *, set->n_waiters + 1);

	if (!waiters) {
		talloc_free(waiter);
		return NULL;
	}

	set->waiters_changed = true;
	set->waiters = waiters;
	set->n_waiters++;

	set->waiters[set->n_waiters - 1] = waiter;

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

	for (i = 0; i < set->n_waiters; i++)
		if (set->waiters[i] == waiter)
			break;

	assert(i < set->n_waiters);

	set->n_waiters--;
	memmove(&set->waiters[i], &set->waiters[i+1],
		(set->n_waiters - i) * sizeof(set->waiters[0]));

	set->waiters = talloc_realloc(set->waiters, set->waiters,
			struct waiter *, set->n_waiters);
	set->waiters_changed = true;

	talloc_free(waiter);
}

int waiter_poll(struct waitset *set)
{
	int i, rc;

	/* If the waiters have been updated, we need to update our
	 * consistent copy */
	if (set->waiters_changed) {

		/* We need to reallocate if the count has changes */
		if (set->cur_n_waiters != set->n_waiters) {
			set->cur_waiters = talloc_realloc(set, set->cur_waiters,
					struct waiter *, set->n_waiters);
			set->pollfds = talloc_realloc(set, set->pollfds,
					struct pollfd, set->n_waiters);
			set->cur_n_waiters = set->n_waiters;
		}

		/* Populate cur_waiters and pollfds from ->waiters data */
		for (i = 0; i < set->n_waiters; i++) {
			set->pollfds[i].fd = set->waiters[i]->fd;
			set->pollfds[i].events = set->waiters[i]->events;
			set->pollfds[i].revents = 0;
			set->cur_waiters[i] = set->waiters[i];
		}

		set->waiters_changed = false;
	}

	rc = poll(set->pollfds, set->cur_n_waiters, -1);

	if (rc <= 0)
		return rc;

	for (i = 0; i < set->cur_n_waiters; i++) {
		if (set->pollfds[i].revents) {
			rc = set->cur_waiters[i]->callback(
					set->cur_waiters[i]->arg);

			if (rc)
				waiter_remove(set->cur_waiters[i]);
		}
	}

	return 0;
}
