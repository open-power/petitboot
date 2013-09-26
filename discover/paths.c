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

struct load_task {
	struct pb_url		*url;
	struct process		*process;
	struct load_url_result	*result;
	bool			async;
	load_url_complete	async_cb;
	void			*async_data;
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

static void load_url_result_cleanup_local(struct load_url_result *result)
{
	if (result->cleanup_local)
		unlink(result->local);
}

static void load_url_process_exit(struct process *process)
{
	struct load_task *task = process->data;
	struct load_url_result *result;
	load_url_complete cb;
	void *data;

	pb_debug("The download client '%s' [pid %d, url %s] exited, rc %d\n",
			process->path, process->pid, task->url->full,
			process->exit_status);

	result = task->result;
	data = task->async_data;
	cb = task->async_cb;

	result->status = process->exit_status == 0 ? LOAD_OK : LOAD_ERROR;
	if (result->status == LOAD_ERROR)
		load_url_result_cleanup_local(result);

	/* The load callback may well free the ctx, which was the
	 * talloc parent of the task. Therefore, we want to do our cleanup
	 * before invoking it
	 */
	process_release(process);
	talloc_free(task);

	cb(result, data);
}

static void load_process_to_local_file(struct load_task *task,
		const char **argv, int argv_local_idx)
{
	int rc;

	task->result->local = local_name(task->result);
	if (!task->result->local) {
		task->result->status = LOAD_ERROR;
		return;
	}
	task->result->cleanup_local = true;

	if (argv_local_idx)
		argv[argv_local_idx] = task->result->local;

	task->process->argv = argv;
	task->process->path = argv[0];

	if (task->async) {
		rc = process_run_async(task->process);
		if (rc) {
			process_release(task->process);
			task->process = NULL;
		}
		task->result->status = rc ? LOAD_ERROR : LOAD_ASYNC;
	} else {
		rc = process_run_sync(task->process);
		task->result->status = rc ? LOAD_ERROR : LOAD_OK;
		process_release(task->process);
		task->process = NULL;
	}
}

/**
 * pb_load_nfs - Create a mountpoint, set the local file within that
 * mountpoint, and run the appropriate mount command
 */

static void load_nfs(struct load_task *task)
{
	char *mountpoint, *opts;
	int rc;
	const char *argv[] = {
			pb_system_apps.mount,
			"-t", "nfs",
			NULL,			/* 3: opts */
			task->url->host,
			task->url->dir,
			NULL,			/* 6: mountpoint */
			NULL,
	};

	task->result->status = LOAD_ERROR;
	mountpoint = local_name(task->result);
	if (!mountpoint)
		return;
	task->result->cleanup_local = true;
	argv[6] = mountpoint;

	rc = pb_mkdir_recursive(mountpoint);
	if (rc)
		return;

	opts = talloc_strdup(NULL, "ro,nolock,nodiratime");
	argv[3] = opts;

	if (task->url->port)
		opts = talloc_asprintf_append(opts, ",port=%s",
						task->url->port);

	task->result->local = talloc_asprintf(task->result, "%s/%s",
							mountpoint,
							task->url->path);

	task->process->path = pb_system_apps.mount;
	task->process->argv = argv;

	if (task->async) {
		rc = process_run_async(task->process);
		if (rc) {
			process_release(task->process);
			task->process = NULL;
		}
		task->result->status = rc ? LOAD_ERROR : LOAD_ASYNC;
	} else {
		rc = process_run_sync(task->process);
		task->result->status = rc ? LOAD_ERROR : LOAD_OK;
		process_release(task->process);
		task->process = NULL;
	}

	talloc_free(opts);
}

static void load_sftp(struct load_task *task)
{
	const char *argv[] = {
			pb_system_apps.sftp,
			NULL,		/* 1: host:path */
			NULL,		/* 2: local file */
			NULL,
	};

	argv[1] = talloc_asprintf(task, "%s:%s",
				task->url->host, task->url->path);
	load_process_to_local_file(task, argv, 2);
}

static enum tftp_type check_tftp_type(void *ctx)
{
	const char *argv[] = { pb_system_apps.tftp, "-V", NULL };
	struct process *process;
	enum tftp_type type;

	process = process_create(ctx);
	process->path = pb_system_apps.tftp;
	process->argv = argv;
	process->keep_stdout = true;
	process_run_sync(process);

	if (!process->stdout_buf || process->stdout_len == 0) {
		pb_log("Can't check TFTP client type!\n");
		type = TFTP_TYPE_BROKEN;

	} else if (memmem(process->stdout_buf, process->stdout_len,
				"tftp-hpa", strlen("tftp-hpa"))) {
		pb_debug("Found TFTP client type: tftp-hpa\n");
		type = TFTP_TYPE_HPA;

	} else if (memmem(process->stdout_buf, process->stdout_len,
				"BusyBox", strlen("BusyBox"))) {
		pb_debug("Found TFTP client type: BusyBox tftp\n");
		type = TFTP_TYPE_BUSYBOX;

	} else {
		pb_log("Unknown TFTP client type!\n");
		type = TFTP_TYPE_BROKEN;
	}

	process_release(process);
	return type;
}

static void load_tftp(struct load_task *task)
{
	const char *port = "69";
	const char *argv[10] = {
		pb_system_apps.tftp,
	};

	if (task->url->port)
		port = task->url->port;

	if (tftp_type == TFTP_TYPE_UNKNOWN)
		tftp_type = check_tftp_type(task);

	if (tftp_type == TFTP_TYPE_BUSYBOX) {
		argv[1] = "-g";
		argv[2] = "-l";
		argv[3] = NULL;	/* 3: local file */
		argv[4] = "-r";
		argv[5] = task->url->path;
		argv[6] = task->url->host;
		argv[7] = port;
		argv[8] = NULL;

		load_process_to_local_file(task, argv, 3);

	} else if (tftp_type == TFTP_TYPE_HPA) {
		argv[1] = "-m";
		argv[2] = "binary";
		argv[3] = task->url->host;
		argv[4] = port;
		argv[5] = "-c";
		argv[6] = "get";
		argv[7] = task->url->path;
		argv[8] = NULL; /* 8: local file */
		argv[9] = NULL;
		load_process_to_local_file(task, argv, 8);

	} else
		task->result->status = LOAD_ERROR;
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

static void load_wget(struct load_task *task, int flags)
{
	const char *argv[] = {
		pb_system_apps.wget,
		"-O",
		NULL, /* 2: local file */
		NULL,
		NULL,
		NULL,
	};
	int i;

	i = 3;
#if !defined(DEBUG)
	argv[i++] = "--quiet";
#endif
	if (flags & wget_no_check_certificate)
		argv[i++] = "--no-check-certificate";

	argv[i] = task->url->full;

	load_process_to_local_file(task, argv, 2);
}

/* Although we don't need to load anything for a local path (we just return
 * the path from the file:// URL), the other load helpers will error-out on
 * non-existant files. So, do the same here with an access() check on the local
 * filename.
 */
static void load_local(struct load_task *task)
{
	int rc;

	rc = access(task->url->path, F_OK);
	if (rc) {
		task->result->status = LOAD_ERROR;
	} else {
		task->result->local = talloc_strdup(task->result,
						    task->url->path);
		task->result->status = LOAD_OK;
	}
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

struct load_url_result *load_url_async(void *ctx, struct pb_url *url,
		load_url_complete async_cb, void *async_data)
{
	struct load_url_result *result;
	struct load_task *task;

	if (!url)
		return NULL;

	task = talloc_zero(ctx, struct load_task);
	task->url = url;
	task->async = async_cb != NULL;
	task->result = talloc_zero(ctx, struct load_url_result);
	task->process = process_create(task);
	if (task->async) {
		task->async_cb = async_cb;
		task->async_data = async_data;
		task->process->exit_cb = load_url_process_exit;
		task->process->data = task;
	}

	switch (url->scheme) {
	case pb_url_ftp:
	case pb_url_http:
		load_wget(task, 0);
		break;
	case pb_url_https:
		load_wget(task, wget_no_check_certificate);
		break;
	case pb_url_nfs:
		load_nfs(task);
		break;
	case pb_url_sftp:
		load_sftp(task);
		break;
	case pb_url_tftp:
		load_tftp(task);
		break;
	default:
		load_local(task);
		break;
	}

	result = task->result;
	if (result->status == LOAD_ERROR) {
		load_url_result_cleanup_local(task->result);
		talloc_free(result);
		talloc_free(task);
		return NULL;
	}

	if (!task->async)
		talloc_free(task);

	return result;
}

struct load_url_result *load_url(void *ctx, struct pb_url *url)
{
	return load_url_async(ctx, url, NULL, NULL);
}
