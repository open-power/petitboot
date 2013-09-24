#define _GNU_SOURCE

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <talloc/talloc.h>
#include <system/system.h>
#include <process/process.h>
#include <url/url.h>
#include <log/log.h>

#include "paths.h"

#define DEVICE_MOUNT_BASE (LOCAL_STATE_DIR "/petitboot/mnt")

struct load_url_async_data {
	load_url_callback url_cb;
	void *ctx;
};

const char *mount_base(void)
{
	return DEVICE_MOUNT_BASE;
}

char *join_paths(void *alloc_ctx, const char *a, const char *b)
{
	char *full_path;

	full_path = talloc_array(alloc_ctx, char, strlen(a) + strlen(b) + 2);

	strcpy(full_path, a);
	if (b[0] != '/' && a[strlen(a) - 1] != '/')
		strcat(full_path, "/");
	strcat(full_path, b);

	return full_path;
}


static char *local_name(void *ctx)
{
	char *tmp, *ret;

	tmp = tempnam(NULL, "pb-");

	if (!tmp)
		return NULL;

	ret = talloc_strdup(ctx, tmp);
	free(tmp);

	return ret;
}

static void load_url_exit_cb(struct process *process)
{
	struct load_url_async_data *url_data = process->data;

	pb_log("The download client '%s' [pid %d] exited, rc %d\n",
			process->path, process->pid, process->exit_status);

	url_data->url_cb(url_data->ctx, process->exit_status);

	process_release(process);
}

/**
 * pb_load_nfs - Mounts the NFS export and returns the local file path.
 *
 * Returns the local file path in a talloc'ed character string on success,
 * or NULL on error.
 */
static char *load_nfs(void *ctx, struct pb_url *url,
		struct load_url_async_data *url_data)
{
	char *local, *opts;
	int result;
	struct process *process;
	const char *argv[] = {
			pb_system_apps.mount,
			"-t", "nfs",
			NULL,
			url->host,
			url->dir,
			NULL,
			NULL,
	};

	local = local_name(ctx);
	if (!local)
		return NULL;
	argv[6] = local;

	result = pb_mkdir_recursive(local);
	if (result)
		goto fail;

	opts = talloc_strdup(NULL, "ro,nolock,nodiratime");
	argv[3] = opts;

	if (url->port)
		opts = talloc_asprintf_append(opts, ",port=%s", url->port);

	if (url_data) {
		process = process_create(ctx);

		process->path = pb_system_apps.mount;
		process->argv = argv;
		process->exit_cb = load_url_exit_cb;
		process->data = url_data;

		result = process_run_async(process);
		if (result)
			process_release(process);
	} else {
		result = process_run_simple_argv(ctx, argv);
	}

	talloc_free(opts);

	if (result)
		goto fail;

	local = talloc_asprintf_append(local,  "/%s", url->path);
	pb_log("%s: local '%s'\n", __func__, local);

	return local;

fail:
	pb_rmdir_recursive("/", local);
	talloc_free(local);
	return NULL;
}

/**
 * pb_load_sftp - Loads a remote file via sftp and returns the local file path.
 *
 * Returns the local file path in a talloc'ed character string on success,
 * or NULL on error.
 */
static char *load_sftp(void *ctx, struct pb_url *url,
		struct load_url_async_data *url_data)
{
	char *host_path, *local;
	int result;
	struct process *process;
	const char *argv[] = {
			pb_system_apps.sftp,
			NULL,
			NULL,
			NULL,
	};

	local = local_name(ctx);
	if (!local)
		return NULL;
	argv[2] = local;

	host_path = talloc_asprintf(local, "%s:%s", url->host, url->path);
	argv[1] = host_path;

	if (url_data) {
		process = process_create(ctx);

		process->path = pb_system_apps.sftp;
		process->argv = argv;
		process->exit_cb = load_url_exit_cb;
		process->data = url_data;

		result = process_run_async(process);
		if (result)
			process_release(process);
	} else {
		result = process_run_simple_argv(ctx, argv);
	}

	if (result)
		goto fail;

	return local;

fail:
	talloc_free(local);
	return NULL;
}

/**
 * pb_load_tftp - Loads a remote file via tftp and returns the local file path.
 *
 * Returns the local file path in a talloc'ed character string on success,
 * or NULL on error.
 */

static char *load_tftp(void *ctx, struct pb_url *url,
		struct load_url_async_data *url_data)
{
	int result;
	const char *argv[10];
	const char **p;
	char *local;
	struct process *process;

	local = local_name(ctx);

	if (!local)
		return NULL;

	/* first try busybox tftp args */

	p = argv;
	*p++ = pb_system_apps.tftp;	/* 1 */
	*p++ = "-g";			/* 2 */
	*p++ = "-l";			/* 3 */
	*p++ = local;			/* 4 */
	*p++ = "-r";			/* 5 */
	*p++ = url->path;		/* 6 */
	*p++ = url->host;		/* 7 */
	if (url->port)
		*p++ = url->port;	/* 8 */
	*p++ = NULL;			/* 9 */

	if (url_data) {
		process = process_create(ctx);

		process->path = pb_system_apps.tftp;
		process->argv = argv;
		process->exit_cb = load_url_exit_cb;
		process->data = url_data;

		result = process_run_async(process);
	} else {
		result = process_run_simple_argv(ctx, argv);
	}

	if (!result)
		return local;

	/* next try tftp-hpa args */
	p = argv;
	*p++ = pb_system_apps.tftp;	/* 1 */
	*p++ = "-m";			/* 2 */
	*p++ = "binary";		/* 3 */
	*p++ = url->host;		/* 4 */
	if (url->port)
		*p++ = url->port;	/* 5 */
	*p++ = "-c";			/* 6 */
	*p++ = "get";			/* 7 */
	*p++ = url->path;		/* 8 */
	*p++ = local;			/* 9 */
	*p++ = NULL;			/* 10 */

	if (url_data) {
		process->argv = argv;
		result = process_run_async(process);
		if (result)
			process_release(process);
	} else {
		result = process_run_simple_argv(ctx, argv);
	}

	if (!result)
		return local;

	talloc_free(local);
	return NULL;
}

enum wget_flags {
	wget_empty = 0,
	wget_no_check_certificate = 1,
};

/**
 * pb_load_wget - Loads a remote file via wget and returns the local file path.
 *
 * Returns the local file path in a talloc'ed character string on success,
 * or NULL on error.
 */

static char *load_wget(void *ctx, struct pb_url *url, enum wget_flags flags,
		struct load_url_async_data *url_data)
{
	int result;
	const char *argv[7];
	const char **p;
	char *local;
	struct process *process;

	local = local_name(ctx);

	if (!local)
		return NULL;

	p = argv;
	*p++ = pb_system_apps.wget;			/* 1 */
#if !defined(DEBUG)
	*p++ = "--quiet";				/* 2 */
#endif
	*p++ = "-O";					/* 3 */
	*p++ = local;					/* 4 */
	*p++ = url->full;				/* 5 */
	if (flags & wget_no_check_certificate)
		*p++ = "--no-check-certificate";	/* 6 */
	*p++ = NULL;					/* 7 */

	if (url_data) {
		process = process_create(ctx);

		process->path = pb_system_apps.wget;
		process->argv = argv;
		process->exit_cb = load_url_exit_cb;
		process->data = url_data;

		result = process_run_async(process);
		if (result)
			process_release(process);
	} else {
		result = process_run_simple_argv(ctx, argv);
	}

	if (result)
		goto fail;

	return local;

fail:
	talloc_free(local);
	return NULL;
}

/**
 * load_url - Loads a (possibly) remote URL and returns the local file
 * path.
 * @ctx: The talloc context to associate with the returned string.
 * @url: The remote file URL.
 * @tempfile: An optional variable pointer to be set when a temporary local
 *  file is created.
 * @url_cb: An optional callback pointer if the caller wants to load url
 *  asynchronously.
 *
 * Returns the local file path in a talloc'ed character string on success,
 * or NULL on error.
 */

char *load_url_async(void *ctx, struct pb_url *url, unsigned int *tempfile,
		load_url_callback url_cb)
{
	char *local;
	int tmp = 0;
	struct load_url_async_data *url_data;

	if (!url)
		return NULL;

	url_data = NULL;

	if (url_cb) {
		url_data = talloc_zero(ctx, struct load_url_async_data);
		url_data->url_cb = url_cb;
		url_data->ctx = ctx;
	}

	switch (url->scheme) {
	case pb_url_ftp:
	case pb_url_http:
		local = load_wget(ctx, url, 0, url_data);
		tmp = !!local;
		break;
	case pb_url_https:
		local = load_wget(ctx, url, wget_no_check_certificate,
				url_data);
		tmp = !!local;
		break;
	case pb_url_nfs:
		local = load_nfs(ctx, url, url_data);
		tmp = !!local;
		break;
	case pb_url_sftp:
		local = load_sftp(ctx, url, url_data);
		tmp = !!local;
		break;
	case pb_url_tftp:
		local = load_tftp(ctx, url, url_data);
		tmp = !!local;
		break;
	default:
		local = talloc_strdup(ctx, url->full);
		tmp = 0;
		break;
	}

	if (tempfile)
		*tempfile = tmp;

	return local;
}

char *load_url(void *ctx, struct pb_url *url, unsigned int *tempfile)
{
	return load_url_async(ctx, url, tempfile, NULL);
}
