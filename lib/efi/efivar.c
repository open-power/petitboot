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

int efi_del_variable(void *ctx, const char *guidstr,
		const char *name)
{
	int fd, flag, errno_value;
	int rc = -1;
	const char *dir;
	char *path;

	dir = get_efivarfs_path();
	if (!dir)
		return -1;

	path = talloc_asprintf(ctx, "%s%s-%s", dir, name, guidstr);
	if (!path)
		return -1;

	fd = open(path, O_RDONLY|O_NONBLOCK);
	if (fd == -1)
		goto err;

	rc = ioctl(fd, FS_IOC_GETFLAGS, &flag);
	if (rc == -1)
		goto err;

	flag &= ~FS_IMMUTABLE_FL;
	rc = ioctl(fd, FS_IOC_SETFLAGS, &flag);
	if (rc == -1)
		goto err;

	close(fd);
	rc = unlink(path);

err:
	errno_value = errno;
	if (fd > 0)
		close(fd);

	errno = errno_value;
	return rc;
}

int efi_get_variable(void *ctx, const char *guidstr, const char *name,
		uint8_t **data, size_t *data_size, uint32_t *attributes)
{
	int fd, errno_value;
	int rc = -1;
	void *p, *buf;
	size_t bufsize = 4096;
	size_t filesize = 0;
	ssize_t sz;
	const char *dir;
	char *path;

	dir = get_efivarfs_path();
	if (!dir)
		return EFAULT;

	path = talloc_asprintf(ctx, "%s%s-%s", dir, name, guidstr);
	if (!path)
		return ENOMEM;

	fd = open(path, O_RDONLY|O_NONBLOCK);
	if (fd < 0)
		goto err;

	buf = talloc_size(ctx, bufsize);
	if (!buf)
		goto err;

	do {
		p = buf + filesize;
		sz = read(fd, p, bufsize);
		if (sz < 0 && errno == EAGAIN) {
			continue;
		} else if (sz == 0) {
			break;
		}
		filesize += sz;
	} while (1);

	*attributes = *(uint32_t *)buf;
	*data = (uint8_t *)(buf + sizeof(uint32_t));
	*data_size = strlen(buf + sizeof(uint32_t));
	rc = 0;

err:
	errno_value = errno;
	if (fd > 0)
		close(fd);

	errno = errno_value;
	return rc;
}

int efi_set_variable(void *ctx, const char *guidstr, const char *name,
		uint8_t *data, size_t data_size, uint32_t attributes)
{
	int rc = -1, errno_value;
	int fd = -1;
	ssize_t len;
	const char *dir;
	char *path;
	void *buf;
	size_t bufsize;
	mode_t mask = 0644;

	dir = get_efivarfs_path();
	if (!dir)
		return EFAULT;

	path = talloc_asprintf(ctx, "%s%s-%s", dir, name, guidstr);
	if (!path)
		return ENOMEM;

	if (!access(path, F_OK)) {
		rc = efi_del_variable(ctx, guidstr, name);
		if (rc < 0) {
			goto err;
		}
	}

	fd = open(path, O_CREAT|O_WRONLY, mask);
	if (fd < 0)
		goto err;

	bufsize = sizeof(uint32_t) + data_size;
	buf = talloc_size(ctx, bufsize);
	if (!buf)
		goto err;

	*(uint32_t *)buf = attributes;
	memcpy(buf + sizeof(uint32_t), data, data_size);

	len = write(fd, buf, bufsize);
	if ((size_t)len != bufsize)
		goto err;
	else
		rc = 0;

err:
	errno_value = errno;
	if (fd > 0)
		close(fd);

	errno = errno_value;
	return rc;
}
