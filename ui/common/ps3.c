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

#define _GNU_SOURCE

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

struct ps3_flash_ctx {
	FILE *dev;
	struct os_area_header header;
	struct os_area_params params;
	struct os_area_db db;
};

static void ps3_flash_close(struct ps3_flash_ctx *fc)
{
	fclose(fc->dev);
	fc->dev = NULL;
}

static int ps3_flash_open(struct ps3_flash_ctx *fc, const char *mode)
{
	int result;

	fc->dev = fopen(flash_dev, mode);

	if (!fc->dev) {
		pb_log("%s: fopen failed: %s: %s\n", __func__, strerror(errno),
			flash_dev);
		return -1;
	}

	os_area_set_log_stream(pb_log_get_stream());

	result = os_area_fixed_read(&fc->header, &fc->params, fc->dev);

	if (result) {
		pb_log("%s: os_area_fixed_read failed: %s\n", __func__);
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

	memset(values, 0, sizeof(*values));

	result = ps3_flash_open(&fc, "r");

	if (result)
		return -1;

	result = os_area_db_read(&fc.db, &fc.header, fc.dev);

	if (result) {
		pb_log("%s: os_area_db_read failed: %s\n", __func__,
			strerror(errno));
		goto fail;
	}

	sum = result = os_area_db_get(&fc.db, &id_default_item, &tmp);

	if (!result)
		values->default_item = (uint32_t)tmp;

	sum += result = os_area_db_get(&fc.db, &id_video_mode, &tmp);

	if (!result)
		values->video_mode = (uint16_t)tmp;


	pb_log("%s: default_item: %u\n", __func__, values->default_item);
	pb_log("%s: video_mode:   %u\n", __func__, values->video_mode);

	ps3_flash_close(&fc);
	return !!sum;

fail:
	ps3_flash_close(&fc);
	return -1;
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

	pb_log("%s: default_item: %u\n", __func__, values->default_item);
	pb_log("%s: video_mode:   %u\n", __func__, values->video_mode);

	result = ps3_flash_open(&fc, "r+");

	if (result)
		return result;

	result = os_area_db_read(&fc.db, &fc.header, fc.dev);

	if (result) {
		pb_log("%s: os_area_db_read failed: %s\n", __func__,
			strerror(errno));
		pb_log("%s: formating db\n", __func__);

		result = os_area_db_format(&fc.db, &fc.header, fc.dev);

		if (result) {
			pb_log("%s: db_format failed: %s\n", __func__,
				strerror(errno));
			goto fail;
		}
	}

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
		pb_log("%s: open failed: %s: %s\n", __func__, strerror(errno),
			fb_dev);
		return -1;
	}

	result = ioctl(fd, request, (unsigned long)mode_id);

	close(fd);

	if (result < 0) {
		pb_log("%s: ioctl failed: %s: %s\n", __func__, strerror(errno),
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
	pb_log("%s: %u\n", __func__, mode_id);
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

	pb_log("%s: %u\n", __func__, *mode_id);
	return result;
}
