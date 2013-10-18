/*
 *  Copyright (C) 2013 Jeremy Kerr <jk@ozlabs.org>
 *
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
 */

#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <talloc/talloc.h>

#include "file.h"

static const int max_file_size = 1024 * 1024;

int read_file(void *ctx, const char *filename, char **bufp, int *lenp)
{
	struct stat statbuf;
	int rc, fd, i, len;
	char *buf;

	fd = open(filename, O_RDONLY);
	if (fd < 0)
		return -1;

	rc = fstat(fd, &statbuf);
	if (rc < 0)
		goto err_close;

	len = statbuf.st_size;
	if (len > max_file_size)
		goto err_close;

	buf = talloc_array(ctx, char, len + 1);
	if (!buf)
		goto err_close;

	for (i = 0; i < len; i += rc) {
		rc = read(fd, buf + i, len - i);

		/* unexpected EOF: trim and return */
		if (rc == 0) {
			len = i;
			break;
		}

		if (rc < 0)
			goto err_free;

	}

	buf[len] = '\0';

	close(fd);
	*bufp = buf;
	*lenp = len;
	return 0;

err_free:
	talloc_free(buf);
err_close:
	close(fd);
	return -1;
}

static int write_fd(int fd, char *buf, int len)
{
	int i, rc;

	for (i = 0; i < len; i += rc) {
		rc = write(fd, buf + i, len - i);
		if (rc < 0 && errno != -EINTR)
			return rc;
	}

	return 0;
}

int replace_file(const char *filename, char *buf, int len)
{
	char *tempfile;
	mode_t oldmask;
	int rc, fd;

	tempfile = talloc_asprintf(NULL, "%s.XXXXXX", filename);

	oldmask = umask(0644);
	fd = mkstemp(tempfile);
	umask(oldmask);
	if (fd < 0) {
		talloc_free(tempfile);
		return fd;
	}

	rc = write_fd(fd, buf, len);
	if (rc) {
		unlink(tempfile);
	} else {
		rc = rename(tempfile, filename);
	}

	free(tempfile);

	fchmod(fd, 0644);

	close(fd);
	return rc;
}
