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

typedef void (*load_url_callback)(void *data, int status);

/* Load a (potentially remote) file, and return a guaranteed-local name */
char *load_url_async(void *ctx, struct pb_url *url, unsigned int *tempfile,
		load_url_callback url_cb);

char *load_url(void *ctx, struct pb_url *url, unsigned int *tempfile);

#endif /* PATHS_H */
