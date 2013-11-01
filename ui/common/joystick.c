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

#define _GNU_SOURCE
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "log/log.h"
#include "talloc/talloc.h"
#include "joystick.h"

/**
 * pjs_process_event - Read joystick event and map to UI key code.
 *
 * Returns a map routine UI key code or zero.
 */

int pjs_process_event(const struct pjs *pjs)
{
	int result;
	struct js_event e;

	assert(pjs->fd);

	result = read(pjs->fd, &e, sizeof(e));

	if (result != sizeof(e)) {
		pb_log("%s: read failed: %s\n", __func__, strerror(errno));
		return 0;
	}

	return pjs->map(&e);
}

/**
 * pjs_destructor - The talloc destructor for a joystick handler.
 */

static int pjs_destructor(void *arg)
{
	struct pjs *pjs = pjs_from_arg(arg);

	close(pjs->fd);
	pjs->fd = 0;

	return 0;
}

/**
 * pjs_init - Initialize the joystick event handler.
 */

struct pjs *pjs_init(void *ctx, int (*map)(const struct js_event *))
{
	static const char dev_name[] = "/dev/input/js0";
	struct pjs *pjs;

	pjs = talloc_zero(ctx, struct pjs);

	if (!pjs)
		return NULL;

	pjs->map = map;
	pjs->fd = open(dev_name, O_RDONLY | O_NONBLOCK);

	if (pjs->fd < 0) {
		pb_log("%s: open %s failed: %s\n", __func__, dev_name,
			strerror(errno));
		goto out_err;
	}

	talloc_set_destructor(pjs, pjs_destructor);

	pb_debug("%s: using %s\n", __func__, dev_name);

	return pjs;

out_err:
	close(pjs->fd);
	pjs->fd = 0;
	talloc_free(pjs);
	return NULL;
}
