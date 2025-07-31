/*
 *  Copyright (C) 2013 Jeremy Kerr <jk@ozlabs.org>
 *  Copyright (C) 2016 Raptor Engineering, LLC
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
#include <log/log.h>

#include "file.h"

#define MAX_FILENAME_SIZE	8192
#define FILE_XFER_BUFFER_SIZE	8192

static const int max_file_size = 1024 * 1024;

int copy_file_secure_dest(void *ctx, const char *source_file,
		char **destination_file)
{
	char readlink_buffer[MAX_FILENAME_SIZE + 1];
	char dest_filename[MAX_FILENAME_SIZE + 1] = "";
	char template[] = "/tmp/petitbootXXXXXX";
	FILE *destination_handle, *source_handle;
	int destination_fd, result = 0;
	unsigned char *buffer;
	ssize_t r;
	size_t l1;

	struct stat statbuf;
	stat(source_file, &statbuf);
	if (!S_ISREG(statbuf.st_mode)) {
		pb_log("%s: unable to stat source file '%s': %m\n",
		       __func__, source_file);
		return -1;
	}

	source_handle = fopen(source_file, "r");
	if (!source_handle) {
		pb_log("%s: unable to open source file '%s': %m\n",
			__func__, source_file);
			return -1;
	}

	destination_fd = mkstemp(template);
	if (destination_fd < 0) {
		pb_log_fn("unable to create temp file, %m\n");
		fclose(source_handle);
		return -1;
	}
	destination_handle = fdopen(destination_fd, "w");
	if (!destination_handle) {
		pb_log_fn("unable to open destination file, %m\n");
		fclose(source_handle);
		close(destination_fd);
		return -1;
	}

	buffer = talloc_array(ctx, unsigned char, FILE_XFER_BUFFER_SIZE);
	if (!buffer) {
		pb_log("%s: failed: unable to allocate file transfer buffer\n",
			__func__);
		result = -1;
		goto out;
	}

	/* Copy data */
	while ((l1 = fread(buffer, 1, FILE_XFER_BUFFER_SIZE, source_handle)) > 0) {
		size_t l2 = fwrite(buffer, 1, l1, destination_handle);
		if (l2 < l1) {
			if (ferror(destination_handle)) {
				/* General error */
				result = -1;
				pb_log_fn("failed: unknown fault\n");
			}
			else {
				/* No space on destination device */
				result = -1;
				pb_log("%s: failed: temporary storage full\n",
					__func__);
			}
			break;
		}
	}

	if (result) {
		*destination_file = NULL;
		goto out;
	}

	snprintf(readlink_buffer, MAX_FILENAME_SIZE, "/proc/self/fd/%d",
		destination_fd);
	r = readlink(readlink_buffer, dest_filename, MAX_FILENAME_SIZE);
	if (r < 0) {
		/* readlink failed */
		result = -1;
		r = 0;
		pb_log("%s: failed: unable to obtain temporary filename\n",
			__func__);
	}
	dest_filename[r] = '\0';

	*destination_file = talloc_strdup(ctx, dest_filename);
out:
	talloc_free(buffer);
	fclose(source_handle);
	fclose(destination_handle);
	close(destination_fd);
	return result;
}

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

	talloc_free(tempfile);

	fchmod(fd, 0644);

	close(fd);
	return rc;
}
