
#include <stdlib.h>
#include <signal.h>

#include "udev.h"
#include "discover-server.h"
#include "waiter.h"
#include "log.h"


int main(void)
{
	struct discover_server *server;
	struct udev *udev;

	/* we look for closed sockets when we write, so ignore SIGPIPE */
	signal(SIGPIPE, SIG_IGN);

	udev = udev_init();
	if (!udev)
		return EXIT_FAILURE;

	server = discover_server_init();
	if (!server)
		return EXIT_FAILURE;

	for (;;) {
		if (waiter_poll())
			return EXIT_FAILURE;
	}


	return EXIT_SUCCESS;
}
