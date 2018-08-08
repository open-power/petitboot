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

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

#include <linux/fs.h>
#include <sys/ioctl.h>
#include <sys/stat.h>

#include "efivar.h"
#include "log/log.h"
#include "talloc/talloc.h"

static const char *efivarfs_path;

inline void set_efivarfs_path(const char *path)
{
	efivarfs_path = path;
}

inline const char *get_efivarfs_path(void)
{

	return efivarfs_path;
}

static int efi_open(const char *name, const char *guidstr, int flags,
	mode_t mode, char **path)
{
	int fd;

	*path = NULL;

	if (!get_efivarfs_path())
		return -1;

	*path = talloc_asprintf(NULL, "%s%s-%s", get_efivarfs_path(), name, guidstr);
	if (!*path)
		return -1;

	flags = flags ? flags : O_RDONLY | O_NONBLOCK;

	fd = open(*path, flags, mode);

	if (fd < 0) {
		pb_log("%s: open failed %s: %s\n", __func__, *path,
			strerror(errno));
		talloc_free(*path);
		*path = NULL;
		return -1;
	}

	return fd;
}

int efi_del_variable(const char *guidstr, const char *name)
{
	int fd, flag;
	int rc = -1;
	char *path;

	fd = efi_open(name, guidstr, 0, 0, &path);
	if (fd < 0)
		return -1;

	rc = ioctl(fd, FS_IOC_GETFLAGS, &flag);
	if (rc == -1 && errno == ENOTTY) {
		pb_debug_fn("'%s' does not support ioctl_iflags.\n",
			efivarfs_path);
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
exit:
	talloc_free(path);
	close(fd);
	return rc;
}

int efi_get_variable(void *ctx, const char *guidstr, const char *name,
		struct efi_data **efi_data)
{
	int fd;
	int rc = -1;
	char *p;
	char buf[4096];
	ssize_t total;
	ssize_t count;
	char *path;

	*efi_data = NULL;

	fd = efi_open(name, guidstr, 0, 0, &path);
	if (fd < 0)
		return -1;

	for (p = buf, total = 0; ; p = buf + count) {
		count = read(fd, p, sizeof(buf) - total);
		if (count < 0) {
			if (errno == EAGAIN || errno == EWOULDBLOCK)
				continue;

			pb_log("%s: read failed %s: (%ld) %s\n", __func__, path,
				count, strerror(errno));
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

	rc = 0;
exit:
	talloc_free(path);
	close(fd);
	return rc;
}

int efi_set_variable(const char *guidstr, const char *name,
		const struct efi_data *efi_data)
{
	int rc = -1;
	int fd;
	ssize_t count;
	void *buf;
	size_t bufsize;
	char *path;

	efi_del_variable(guidstr, name);

	fd = efi_open(name, guidstr, O_CREAT | O_WRONLY,
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
		pb_log("%s: write failed %s: (%ld) %s\n", __func__, name,
			count, strerror(errno));
		goto exit;
	}
	rc = 0;

exit:
	talloc_free(path);
	close(fd);
	return rc;
}
