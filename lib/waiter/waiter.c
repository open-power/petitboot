
#include <poll.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <sys/time.h>

#include <talloc/talloc.h>
#include <list/list.h>

#include "waiter.h"

struct waiter {
	struct waitset	*set;
	enum {
		WAITER_IO,
		WAITER_TIME,
	} type;
	union {
		struct {
			int	fd;
			int	events;
		} io;
		struct timeval	timeout;
	};
	waiter_cb	callback;
	void		*arg;

	bool			active;
	struct list_item	list;
};

struct waitset {
	struct waiter	**waiters;
	int		n_waiters;
	bool		waiters_changed;

	struct timeval	next_timeout;

	/* These are kept consistent over each call to waiter_poll, as
	 * set->waiters may be updated (by waiters' callbacks calling
	 * waiter_register or waiter_remove) during iteration. */
	struct pollfd	*pollfds;
	int		n_pollfds;
	struct waiter	**io_waiters;
	int		n_io_waiters;
	struct waiter	**time_waiters;
	int		n_time_waiters;

	struct list	free_list;
};

struct waitset *waitset_create(void *ctx)
{
	struct waitset *set = talloc_zero(ctx, struct waitset);
	list_init(&set->free_list);
	return set;
}

void waitset_destroy(struct waitset *set)
{
	talloc_free(set);
}

static struct waiter *waiter_new(struct waitset *set)
{
	struct waiter **waiters, *waiter;

	waiter = talloc(set, struct waiter);
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
	waiter->active = true;
	return waiter;
}

struct waiter *waiter_register_io(struct waitset *set, int fd, int events,
		waiter_cb callback, void *arg)
{
	struct waiter *waiter = waiter_new(set);

	waiter->type = WAITER_IO;
	waiter->set = set;
	waiter->io.fd = fd;
	waiter->io.events = events;
	waiter->callback = callback;
	waiter->arg = arg;

	return waiter;
}

struct waiter *waiter_register_timeout(struct waitset *set, int delay_ms,
		waiter_cb callback, void *arg)
{
	struct waiter *waiter = waiter_new(set);
	struct timeval now, delay;

	delay.tv_sec = delay_ms / 1000;
	delay.tv_usec = 1000 * (delay_ms % 1000);

	gettimeofday(&now, NULL);

	timeradd(&now, &delay, &waiter->timeout);

	waiter->type = WAITER_TIME;
	waiter->set = set;
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

	set->waiters = talloc_realloc(set, set->waiters,
			struct waiter *, set->n_waiters);
	set->waiters_changed = true;

	waiter->active = false;
	list_add(&set->free_list, &waiter->list);
}

static void update_waiters(struct waitset *set)
{
	int n_io, n_time, i_io, i_time, i;

	if (!set->waiters_changed)
		return;

	n_io = n_time = 0;

	for (i = 0; i < set->n_waiters; i++) {
		if (set->waiters[i]->type == WAITER_IO)
			n_io++;
		else if (set->waiters[i]->type == WAITER_TIME)
			n_time++;
	}

	/* realloc if counts have changed */
	if (set->n_io_waiters != n_io) {
		set->io_waiters = talloc_realloc(set, set->io_waiters,
				struct waiter *, n_io);
		set->pollfds = talloc_realloc(set, set->pollfds,
				struct pollfd, n_io);
		set->n_io_waiters = n_io;
	}
	if (set->n_time_waiters != n_time) {
		set->time_waiters = talloc_realloc(set, set->time_waiters,
				struct waiter *, n_time);
		set->n_time_waiters = n_time;
	}

	i_io = 0;
	i_time = 0;

	timerclear(&set->next_timeout);

	for (i = 0; i < set->n_waiters; i++) {
		struct waiter *waiter = set->waiters[i];

		/* IO waiters: copy to io_waiters, populate pollfds */
		if (waiter->type == WAITER_IO) {
			set->pollfds[i_io].fd = waiter->io.fd;
			set->pollfds[i_io].events = waiter->io.events;
			set->io_waiters[i_io] = waiter;
			i_io++;
		}

		/* time waiters: copy to time_waiters, calculate next expiry */
		if (waiter->type == WAITER_TIME) {
			if (!timerisset(&set->next_timeout) ||
					timercmp(&waiter->timeout,
						&set->next_timeout, <))
				set->next_timeout = waiter->timeout;

			set->time_waiters[i_time] = waiter;
			i_time++;
		}
	}
}

int waiter_poll(struct waitset *set)
{
	struct timeval now, timeout;
	struct waiter *waiter, *tmp;
	int timeout_ms;
	int i, rc;

	/* If the waiters have been updated, we need to update our
	 * consistent copy */
	update_waiters(set);

	if (timerisset(&set->next_timeout)) {
		gettimeofday(&now, NULL);
		timersub(&set->next_timeout, &now, &timeout);
		timeout_ms = timeout.tv_sec * 1000 +
				timeout.tv_usec / 1000;
		if (timeout_ms < 0)
			timeout_ms = 0;
	} else {
		timeout_ms = -1;
	}


	rc = poll(set->pollfds, set->n_io_waiters, timeout_ms);

	if (rc < 0)
		goto out;

	for (i = 0; i < set->n_io_waiters; i++) {
		struct waiter *waiter = set->io_waiters[i];

		if (!waiter->active)
			continue;

		if (!set->pollfds[i].revents)
			continue;
		rc = waiter->callback(waiter->arg);

		if (rc)
			waiter_remove(waiter);
	}

	if (set->n_time_waiters > 0)
		gettimeofday(&now, NULL);

	for (i = 0; i < set->n_time_waiters; i++) {
		struct waiter *waiter = set->time_waiters[i];

		if (!waiter->active)
			continue;

		if (timercmp(&waiter->timeout, &now, >))
			continue;

		waiter->callback(waiter->arg);

		waiter_remove(waiter);
	}

	rc = 0;

out:
	/* free any waiters that have been removed */
	list_for_each_entry_safe(&set->free_list, waiter, tmp, list)
		talloc_free(waiter);
	list_init(&set->free_list);

	return rc;
}
