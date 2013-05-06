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
#include <err.h>
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

static void print_version(void)
{
	printf("pb-event (" PACKAGE_NAME ") " PACKAGE_VERSION "\n");
}

static void print_usage(void)
{
	print_version();
	printf(
"Usage: pb-event [-h] [event...]\n"
"\n"
"       Send a single petitboot user event to the petitboot discover server.\n"
"       Events can be read from stdin, or provided on the command line.\n"
"       User events must have the following format:\n"
"\n"
"         (add|remove)@device-id [name=value] [image=value] [args=value]\n"
"\n"
"       When read from stdin, components are separated by NUL chars\n"
"\n"
"Examples:\n"
"\n"
"    args:\n"
"       pb-event add@/net/eth0 name=netboot image=tftp://192.168.1.10/vmlinux\n"
"       pb-event remove@/net/eth0\n"
"\n"
"    stdin:\n"
"       printf 'add@/net/eth0\\0name=netboot\\0image=tftp://10.0.0.2/vmlinux\\0' \\\n"
"           | pb-event\n"
"       printf 'remove@/net/eth0\\0' | pb-event\n"
"\n");
}

static const char *err_max_size = "pb-event: message too large "
					"(%zu byte max)\n";

static ssize_t parse_event_args(int n, char * const * args,
		char *buf, size_t max_len)
{
	ssize_t arg_len, total_len;
	const char *arg;
	int i;

	total_len = 0;

	for (i = 0; i < n; i++) {
		arg = args[i];
		arg_len = strlen(arg);

		if (total_len + (size_t)i + 1 > max_len) {
			fprintf(stderr, err_max_size, max_len);
			return -1;
		}

		memcpy(buf + total_len, arg, arg_len);
		total_len += arg_len;

		buf[total_len] = '\0';
		total_len++;
	}

	return total_len;

}

static ssize_t parse_event_file(FILE *filp, char *buf, size_t max_len)
{
	int len;

	len = fread(buf, 1, max_len, filp);

	if (!feof(filp)) {
		fprintf(stderr, err_max_size, max_len);
		return -1;
	}

	if (!len)
		return -1;

	return len;
}

static int send_event(char *buf, ssize_t len)
{
	struct sockaddr_un addr;
	int sd, i;

	sd = socket(PF_UNIX, SOCK_DGRAM, 0);
	if (sd < 0)
		err(EXIT_FAILURE, "socket");

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, PBOOT_USER_EVENT_SOCKET);

	for (i = 10; i; i--) {
		ssize_t sent = sendto(sd, buf, len, 0,
				(struct sockaddr *)&addr, SUN_LEN(&addr));

		if (sent == len)
			break;

		DBG("pb-event: waiting for server %d\n", i);
		sleep(1);
	}

	close(sd);

	if (!i)
		err(EXIT_FAILURE, "send");

	DBG("pb-event: wrote %zu bytes\n", len);

	return 0;
}

int main(int argc, char *argv[])
{
	char buf[PBOOT_USER_EVENT_SIZE];
	ssize_t len;

	if (argc >= 2 && !strcmp(argv[1], "-h")) {
		print_usage();
		return EXIT_SUCCESS;
	}

	if (argc > 1) {
		len = parse_event_args(argc - 1, argv + 1,
					buf, sizeof(buf));
	} else {
		len = parse_event_file(stdin, buf, sizeof(buf));
	}

	if (len < 0)
		return EXIT_FAILURE;

	send_event(buf, len);

	return EXIT_SUCCESS;
}
