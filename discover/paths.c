#define _GNU_SOURCE

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <talloc/talloc.h>
#include <system/system.h>
#include <url/url.h>
#include <log/log.h>

#include "paths.h"

#define DEVICE_MOUNT_BASE (LOCAL_STATE_DIR "/petitboot/mnt")

struct mount_map {
	char *dev, *mnt;
};

static struct mount_map *mount_map;
static int mount_map_size;

static int is_prefix(const char *str, const char *prefix)
{
	return !strncmp(str, prefix, strlen(prefix));
}

static int is_prefix_ignorecase(const char *str, const char *prefix)
{
	return !strncasecmp(str, prefix, strlen(prefix));
}

const char *mount_base(void)
{
	return DEVICE_MOUNT_BASE;
}

char *encode_label(void *alloc_ctx, const char *label)
{
	char *str, *c;
	unsigned int i;

	/* the label can be expanded by up to four times */
	str = talloc_size(alloc_ctx, strlen(label) * 4 + 1);
	c = str;

	for (i = 0; i < strlen(label); i++) {

		if (label[i] == '/' || label[i] == '\\') {
			sprintf(c, "\\x%02x", label[i]);
			c += 4;
			continue;
		}

		*(c++) = label[i];
	}

	*c = '\0';

	return str;
}

char *parse_device_path(void *alloc_ctx,
		const char *dev_str, const char __attribute__((unused)) *cur_dev)
{
	char *dev, *enc;

	if (is_prefix_ignorecase(dev_str, "uuid=")) {
		dev = talloc_asprintf(alloc_ctx, "/dev/disk/by-uuid/%s",
				dev_str + strlen("uuid="));
		return dev;
	}

	if (is_prefix_ignorecase(dev_str, "label=")) {
		enc = encode_label(NULL, dev_str + strlen("label="));
		dev = talloc_asprintf(alloc_ctx, "/dev/disk/by-label/%s", enc);
		talloc_free(enc);
		return dev;
	}

	/* normalise '/dev/foo' to 'foo' for easy comparisons, we'll expand
	 * back before returning.
	 */
	if (is_prefix(dev_str, "/dev/"))
		dev_str += strlen("/dev/");

	return join_paths(alloc_ctx, "/dev", dev_str);
}

const char *mountpoint_for_device(const char *dev)
{
	int i;

	if (is_prefix(dev, "/dev/"))
		dev += strlen("/dev/");

	/* check existing entries in the map */
	for (i = 0; i < mount_map_size; i++)
		if (!strcmp(mount_map[i].dev, dev))
			return mount_map[i].mnt;

	/* no existing entry, create a new one */
	i = mount_map_size++;
	mount_map = talloc_realloc(NULL, mount_map,
			struct mount_map, mount_map_size);

	mount_map[i].dev = talloc_strdup(mount_map, dev);
	mount_map[i].mnt = join_paths(mount_map, DEVICE_MOUNT_BASE, dev);
	return mount_map[i].mnt;
}

char *resolve_path(void *alloc_ctx, const char *path, const char *current_dev)
{
	static const char s_file[] = "file://";
	char *ret;
	const char *devpath, *sep;

	/* test for urls */

	if (!strncasecmp(path, s_file, sizeof(s_file) - 1))
		path += sizeof(s_file) - 1;
	else if (strstr(path, "://"))
		return talloc_strdup(alloc_ctx, path);

	sep = strchr(path, ':');
	if (!sep) {
		devpath = mountpoint_for_device(current_dev);
		ret = join_paths(alloc_ctx, devpath, path);
	} else {
		/* parse just the device name into dev */
		char *tmp, *dev;
		tmp = talloc_strndup(NULL, path, sep - path);
		dev = parse_device_path(NULL, tmp, current_dev);

		devpath = mountpoint_for_device(dev);
		ret = join_paths(alloc_ctx, devpath, sep + 1);

		talloc_free(dev);
		talloc_free(tmp);
	}

	return ret;
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

/**
 * pb_load_nfs - Mounts the NFS export and returns the local file path.
 *
 * Returns the local file path in a talloc'ed character string on success,
 * or NULL on error.
 */
static char *load_nfs(void *ctx, struct pb_url *url)
{
	int result;
	const char *argv[8];
	const char **p;
	char *local;
	char *opts;

	local = local_name(ctx);

	if (!local)
		return NULL;

	result = pb_mkdir_recursive(local);

	if (result)
		goto fail;

	opts = talloc_strdup(NULL, "ro,nolock,nodiratime");

	if (url->port)
		opts = talloc_asprintf_append(opts, ",port=%s", url->port);

	p = argv;
	*p++ = pb_system_apps.mount;	/* 1 */
	*p++ = "-t";			/* 2 */
	*p++ = "nfs";			/* 3 */
	*p++ = opts;			/* 4 */
	*p++ = url->host;		/* 5 */
	*p++ = url->dir;		/* 6 */
	*p++ = local;			/* 7 */
	*p++ = NULL;			/* 8 */

	result = pb_run_cmd(argv, 1, 0);

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
static char *load_sftp(void *ctx, struct pb_url *url)
{
	int result;
	const char *argv[4];
	const char **p;
	char *local;

	local = local_name(ctx);

	if (!local)
		return NULL;

	p = argv;
	*p++ = pb_system_apps.sftp;					/* 1 */
	*p++ = talloc_asprintf(local, "%s:%s", url->host, url->path);	/* 2 */
	*p++ = local;							/* 3 */
	*p++ = NULL;							/* 4 */

	result = pb_run_cmd(argv, 1, 0);

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

static char *load_tftp(void *ctx, struct pb_url *url)
{
	int result;
	const char *argv[10];
	const char **p;
	char *local;

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

	result = pb_run_cmd(argv, 1, 0);

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

	result = pb_run_cmd(argv, 1, 0);

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

static char *load_wget(void *ctx, struct pb_url *url, enum wget_flags flags)
{
	int result;
	const char *argv[7];
	const char **p;
	char *local;

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

	result = pb_run_cmd(argv, 1, 0);

	if (result)
		goto fail;

	return local;

fail:
	talloc_free(local);
	return NULL;
}

/**
 * pb_load_file - Loads a (possibly) remote file and returns the local file
 * path.
 * @ctx: The talloc context to associate with the returned string.
 * @remote: The remote file URL.
 * @tempfile: An optional variable pointer to be set when a temporary local
 *  file is created.
 *
 * Returns the local file path in a talloc'ed character string on success,
 * or NULL on error.
 */

char *load_file(void *ctx, const char *remote, unsigned int *tempfile)
{
	struct pb_url *url = pb_url_parse(ctx, remote);
	char *local;
	int tmp = 0;

	if (!url)
		return NULL;

	switch (url->scheme) {
	case pb_url_ftp:
	case pb_url_http:
		local = load_wget(ctx, url, 0);
		tmp = !!local;
		break;
	case pb_url_https:
		local = load_wget(ctx, url, wget_no_check_certificate);
		tmp = !!local;
		break;
	case pb_url_nfs:
		local = load_nfs(ctx, url);
		tmp = !!local;
		break;
	case pb_url_sftp:
		local = load_sftp(ctx, url);
		tmp = !!local;
		break;
	case pb_url_tftp:
		local = load_tftp(ctx, url);
		tmp = !!local;
		break;
	default:
		local = talloc_strdup(ctx, url->full);
		tmp = 0;
		break;
	}

	if (tempfile)
		*tempfile = tmp;

	talloc_free(url);
	return local;
}
