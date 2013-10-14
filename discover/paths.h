#ifndef PATHS_H
#define PATHS_H

#include <url/url.h>

/**
 * Utility function for joining two paths. Adds a / between a and b if
 * required.
 *
 * Returns a newly-allocated string.
 */
char *join_paths(void *alloc_ctx, const char *a, const char *b);

/**
 * Returns the base path for mount points
 */
const char *mount_base(void);

struct load_task;

struct load_url_result {
	enum {
		LOAD_OK,    /* load complete. other members should only be
			       accessed if status == LOAD_OK */

		LOAD_ERROR, /* only signalled to async loaders
			     * (sync will see a NULL result) */

		LOAD_ASYNC, /* async load still in progress */

		LOAD_CANCELLED,
	} status;
	const char		*local;
	bool			cleanup_local;
	struct load_task	*task;
};

/* callback type for asynchronous loads. The callback implementation is
 * responsible for freeing result.
 */
typedef void (*load_url_complete)(struct load_url_result *result, void *data);

/* Load a (potentially remote) file, and return a guaranteed-local name */
struct load_url_result *load_url_async(void *ctx, struct pb_url *url,
		load_url_complete complete, void *data);

/* Cancel a pending load */
void load_url_async_cancel(struct load_url_result *res);

struct load_url_result *load_url(void *ctx, struct pb_url *url);

#endif /* PATHS_H */
