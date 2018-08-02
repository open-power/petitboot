/*
 *  Copyright (C) 2009 Sony Computer Entertainment Inc.
 *  Copyright 2009 Sony Corp.
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

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <ps3-flash.h>
#include <ps3-av.h>

#include "log/log.h"
#include "ui-system.h"
#include "ps3.h"

static const char flash_dev[] = "/dev/ps3flash";
static const char fb_dev[] = "/dev/fb0";

static const struct os_area_db_id id_default_item =
{
	.owner = OS_AREA_DB_OWNER_PETITBOOT, /* 3 */
	.key = 1,
};
static const struct os_area_db_id id_video_mode =
{
	.owner = OS_AREA_DB_OWNER_PETITBOOT, /* 3 */
	.key = OS_AREA_DB_KEY_VIDEO_MODE,    /* 2 */
};
static const struct os_area_db_id id_flags =
{
	.owner = OS_AREA_DB_OWNER_PETITBOOT, /* 3 */
	.key = 3,
};
static const struct os_area_db_id id_timeout =
{
	.owner = OS_AREA_DB_OWNER_PETITBOOT, /* 3 */
	.key = 4,
};

struct ps3_flash_ctx {
	FILE *dev;
	struct os_area_header header;
	struct os_area_params params;
	struct os_area_db db;
};

static void ps3_flash_close(struct ps3_flash_ctx *fc)
{
	assert(fc->dev);

	fclose(fc->dev);
	fc->dev = NULL;
}

static int ps3_flash_open(struct ps3_flash_ctx *fc, const char *mode)
{
	int result;

	fc->dev = fopen(flash_dev, mode);

	if (!fc->dev) {
		pb_log_fn("fopen failed: %s: %s\n", strerror(errno),
			flash_dev);
		return -1;
	}

	os_area_set_log_stream(pb_log_get_stream());

	result = os_area_fixed_read(&fc->header, &fc->params, fc->dev);

	if (result) {
		pb_log_fn("os_area_fixed_read failed\n");
		goto fail;
	}

	return 0;

fail:
	ps3_flash_close(fc);
	return -1;
}

/**
 * ps3_flash_get_values - Read values from the PS3 flash memory database.
 *
 * Returns zero on success.
 */

int ps3_flash_get_values(struct ps3_flash_values *values)
{
	int result;
	int sum;
	struct ps3_flash_ctx fc;
	uint64_t tmp;

	result = ps3_flash_open(&fc, "r");

	if (result)
		goto fail;

	result = os_area_db_read(&fc.db, &fc.header, fc.dev);

	ps3_flash_close(&fc);

	if (result) {
		pb_log_fn("os_area_db_read failed: %s\n",
			strerror(errno));
		goto fail;
	}

	sum = result = os_area_db_get(&fc.db, &id_default_item, &tmp);

	if (!result)
		values->default_item = (uint32_t)tmp;

	result = os_area_db_get(&fc.db, &id_timeout, &tmp);

	if (!result)
		values->timeout = (uint8_t)tmp;

	sum += result = os_area_db_get(&fc.db, &id_video_mode, &tmp);

	if (!result)
		values->video_mode = (uint16_t)tmp;

	pb_debug("%s: default_item: %x\n", __func__,
		(unsigned int)values->default_item);
	pb_debug("%s: timeout: %u\n", __func__,
		(unsigned int)values->timeout);
	pb_debug("%s: video_mode:   %u\n", __func__,
		(unsigned int)values->video_mode);
fail:
	return (result || sum) ? -1 : 0;
}

/**
 * ps3_flash_set_values - Writes values from the PS3 flash memory database.
 *
 * Formats the flash database before writing if a valid database if not found.
 * Returns zero on success.
 */

int ps3_flash_set_values(const struct ps3_flash_values *values)
{
	int result;
	struct ps3_flash_ctx fc;

	pb_debug("%s: default_item: %u\n", __func__, values->default_item);
	pb_debug("%s: video_mode:   %u\n", __func__, values->video_mode);

	result = ps3_flash_open(&fc, "r+");

	if (result)
		return result;

	result = os_area_db_read(&fc.db, &fc.header, fc.dev);

	if (result) {
		pb_log_fn("os_area_db_read failed: %s\n",
			strerror(errno));
		pb_log_fn("formating db\n");

		result = os_area_db_format(&fc.db, &fc.header, fc.dev);

		if (result) {
			pb_log_fn("db_format failed: %s\n",
				strerror(errno));
			goto fail;
		}
	}

	/* timeout is currently read-only, set with ps3-bl-option */

	result = os_area_db_set_32(&fc.db, &id_default_item,
		values->default_item);
	result += os_area_db_set_16(&fc.db, &id_video_mode,
		values->video_mode);

	result += os_area_db_write(&fc.db, &fc.header, fc.dev);

	ps3_flash_close(&fc);
	return result;

fail:
	ps3_flash_close(&fc);
	return -1;
}

/**
 * ps3_video_ioctl - Low level ioctl helper.
 *
 * Use ps3_get_video_mode or ps3_set_video_mode().
 */

static int ps3_video_ioctl(int request, unsigned int *mode_id)
{
	int result;
	int fd;

	fd = open(fb_dev, O_RDWR);

	if (fd < 0) {
		pb_log_fn("open failed: %s: %s\n", strerror(errno),
			fb_dev);
		return -1;
	}

	result = ioctl(fd, request, (unsigned long)mode_id);

	close(fd);

	if (result < 0) {
		pb_log_fn("ioctl failed: %s: %s\n", strerror(errno),
			fb_dev);
		return -1;
	}

	return 0;
}

/**
 * ps3_set_video_mode - Set the PS3 video mode.
 * @mode_id: The PS3 video mode_id as documented in the ps3-video-mode man page.
 *
 * Returns zero on success.
 */

int ps3_set_video_mode(unsigned int mode_id)
{
	pb_debug("%s: %u\n", __func__, mode_id);
	return ps3_video_ioctl(PS3FB_IOCTL_SETMODE, &mode_id);
}

/**
 * ps3_set_video_mode - Get the current PS3 video mode.
 * @mode_id: The PS3 video mode_id as documented in the ps3-video-mode man page.
 *
 * Returns zero on success.
 */

int ps3_get_video_mode(unsigned int *mode_id)
{
	int result;

	*mode_id = 0;

	result =  ps3_video_ioctl(PS3FB_IOCTL_GETMODE, mode_id);

	pb_log_fn("%u\n", *mode_id);
	return result;
}
