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

#if !defined(_PB_COMMON_PS3_H)
#define _PB_COMMON_PS3_H

#include <stdint.h>

int ps3_get_video_mode(unsigned int *mode_id);
int ps3_set_video_mode(unsigned int mode_id);

/**
 * enum ps3_flash_flags
 * @ps3_flag_telnet: Allow telnet connections. System use.
 */

enum ps3_flash_flags {
	ps3_flag_telnet = 1,
};

enum ps3_timeouts {
	ps3_timeout_forever = 255,
};

/**
 * struct ps3_flash_values - Values from PS3 flash memory.
 * @default_item: The default menu item.
 * @timeout: The timeout in seconds.
 * @video_mode: The default video_mode.
 * @flags: Logical OR of enum ps3_flash_flags.
 */

struct ps3_flash_values {
	uint32_t default_item;
	uint16_t video_mode;
	/* uint16_t flags; */
	uint8_t timeout;
};

static const struct ps3_flash_values ps3_flash_defaults = {
	.default_item = 0,
	.video_mode = 1,
	.timeout = ps3_timeout_forever,
};

int ps3_flash_get_values(struct ps3_flash_values *values);
int ps3_flash_set_values(const struct ps3_flash_values *values);

#endif
