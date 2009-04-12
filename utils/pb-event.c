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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>

#include "discover/user-event.h"

#if defined(DEBUG)
#define DBG(_args...) do {fprintf(stderr, _args); fflush(stderr); } while (0)
#else
static inline int __attribute__ ((format (printf, 1, 2))) DBG(
	__attribute__((unused)) const char *fmt, ...) {return 0; }
#endif

int main(void)
{
	int result;
	struct sockaddr_un addr;
	char buf[PBOOT_USER_EVENT_SIZE];
	ssize_t len;
	int s;
	int i;

	s = socket(PF_UNIX, SOCK_DGRAM, 0);

	if (s < 0) {
		fprintf(stderr, "pb-event: socket: %s\n", strerror(errno));
		return EXIT_FAILURE;
	}

	result = EXIT_SUCCESS;

	len = fread(buf, 1, sizeof(buf), stdin);

	if (!feof(stdin)) {
		fprintf(stderr, "pb-event: msg too big (%u byte max)\n",
			sizeof(buf));
		result = EXIT_FAILURE;
		/* continue on and try to write msg */
	}

	if (!len)
		return result;

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, PBOOT_USER_EVENT_SOCKET);

	for (i = 10; i; i--) {
		ssize_t sent = sendto(s, buf, len, 0, (struct sockaddr *)&addr,
			SUN_LEN(&addr));

		if (sent == len)
			break;

		DBG("pb-event: waiting for server %d\n", i);
		sleep(1);
	}

	if (!i) {
		fprintf(stderr, "pb-event: send: %s\n", strerror(errno));
		return EXIT_FAILURE;
	}

	DBG("pb-event: wrote %u bytes\n", len);
	return result;
}
