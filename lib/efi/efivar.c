/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *  Copyright (C) 2018 Huaxintong Semiconductor Technology Co.,Ltd. All rights
 *  reserved.
 *  Author: Ge Song <ge.song@hxt-semitech.com>
 */

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

#include <linux/fs.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/statfs.h>

#include "efivar.h"
#include "log/log.h"
#include "talloc/talloc.h"

void efi_init_mount(struct efi_mount *efi_mount, const char *path,
	const char *guid)
{
	assert(efi_mount);

	efi_mount->path = path;
	efi_mount->guid = guid;

	pb_debug_fn("%s--%s", efi_mount->path, efi_mount->guid);
}

bool efi_check_mount_magic(const struct efi_mount *efi_mount, bool check_magic)
{
	struct statfs s;

	assert(efi_mount);

	if (!efi_mount->guid) {
		pb_debug_fn("guid not set\n");
		return false;
	}

	if (access(efi_mount->path, R_OK | W_OK)) {
		pb_debug_fn("Can't access %s\n", efi_mount->path);
		return false;
	}

	memset(&s, '\0', sizeof(s));
	if (statfs(efi_mount->path, &s)) {
		pb_debug_fn("statfs failed: %s: (%d) %s\n", efi_mount->path,
			errno, strerror(errno));
		return false;
	}

	if (check_magic && s.f_type != EFIVARFS_MAGIC) {
		pb_debug_fn("Bad magic = 0x%lx\n", (unsigned long)s.f_type);
		return false;
	}

	return true;
}

static int efi_open(const struct efi_mount *efi_mount, const char *name,
	int flags, mode_t mode, char **path)
{
	int fd;

	assert(efi_mount);

	*path = NULL;

	if (!efi_mount->path || !efi_mount->guid)
		return -1;

	*path = talloc_asprintf(NULL, "%s/%s-%s", efi_mount->path, name,
		efi_mount->guid);
	if (!*path)
		return -1;

	flags = flags ? flags : O_RDONLY | O_NONBLOCK;

	fd = open(*path, flags, mode);

	if (fd < 0) {
		pb_log("%s: open failed '%s': (%d) %s\n", __func__, *path,
			errno, strerror(errno));
		talloc_free(*path);
		*path = NULL;
		return -1;
	}

	return fd;
}

int efi_del_variable(const struct efi_mount *efi_mount, const char *name)
{
	int fd, flag;
	int rc = -1;
	char *path;

	assert(efi_mount);

	fd = efi_open(efi_mount, name, 0, 0, &path);
	if (fd < 0)
		return -1;

	rc = ioctl(fd, FS_IOC_GETFLAGS, &flag);
	if (rc == -1 && errno == ENOTTY) {
		pb_debug_fn("'%s' does not support ioctl_iflags.\n",
			efi_mount->path);
		goto delete;
	} else if (rc == -1) {
		pb_log_fn("FS_IOC_GETFLAGS failed: (%d) %s\n", errno,
			strerror(errno));
		goto exit;
	}

	flag &= ~FS_IMMUTABLE_FL;
	rc = ioctl(fd, FS_IOC_SETFLAGS, &flag);
	if (rc == -1) {
		pb_log_fn("FS_IOC_SETFLAGS failed: (%d) %s\n", errno,
			strerror(errno));
		goto exit;
	}

delete:
	close(fd);
	fd = 0;
	rc = unlink(path);
	if (rc == -1) {
		pb_log_fn("unlink failed: (%d) %s\n", errno, strerror(errno));
		goto exit;
	}
	pb_debug_fn("Deleted: '%s'\n", name);
exit:
	talloc_free(path);
	close(fd);
	return rc;
}

int efi_get_variable(void *ctx, const struct efi_mount *efi_mount,
	const char *name, struct efi_data **efi_data)
{
	int fd;
	int rc = -1;
	char *p;
	char buf[4096];
	ssize_t total;
	ssize_t count;
	char *path;

	assert(efi_mount);

	*efi_data = NULL;

	fd = efi_open(efi_mount, name, 0, 0, &path);
	if (fd < 0)
		return -1;

	for (p = buf, total = 0; ; p = buf + count) {
		count = read(fd, p, sizeof(buf) - total);
		if (count < 0) {
			if (errno == EAGAIN || errno == EWOULDBLOCK)
				continue;

			pb_log("%s: read failed %s: (%ld) (%d) %s\n", __func__, path,
				count, errno, strerror(errno));
			goto exit;
		}
		if (p >= (buf + sizeof(buf))) {
			pb_log("%s: buffer full %s: (%ld)\n", __func__, path,
				sizeof(buf));
			goto exit;
		}
		if (count == 0)
			break;
		total += count;
	};

	*efi_data = (void*)talloc_zero_array(ctx, char,
		sizeof (struct efi_data) + total);

	(*efi_data)->attributes = *(uint32_t *)buf;
	(*efi_data)->data_size = total;
	(*efi_data)->data = (*efi_data)->fill;
	memcpy((*efi_data)->data, buf + sizeof (uint32_t), total);
	pb_debug_fn("Found: '%s'='%s'\n", name, (const char *)(*efi_data)->data);

	rc = 0;
exit:
	talloc_free(path);
	close(fd);
	return rc;
}

int efi_set_variable(const struct efi_mount *efi_mount, const char *name,
		const struct efi_data *efi_data)
{
	int rc = -1;
	int fd;
	ssize_t count;
	void *buf;
	size_t bufsize;
	char *path;

	assert(efi_mount);

	efi_del_variable(efi_mount, name);

	fd = efi_open(efi_mount, name, O_CREAT | O_WRONLY,
		S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH, &path);
	if (fd < 0)
		return -1;

	bufsize = sizeof(uint32_t) + efi_data->data_size;
	buf = talloc_size(path, bufsize);
	if (!buf)
		goto exit;

	*(uint32_t *)buf = efi_data->attributes;
	memcpy(buf + sizeof(uint32_t), efi_data->data, efi_data->data_size);

	count = write(fd, buf, bufsize);
	if ((size_t)count != bufsize) {
		pb_log("%s: write failed %s: (%ld) (%d) %s\n", __func__, name,
			count, errno, strerror(errno));
		goto exit;
	}
	rc = 0;
	pb_debug_fn("Set: '%s'='%s'\n", name,  (const char *)efi_data->data);

exit:
	talloc_free(path);
	close(fd);
	return rc;
}
