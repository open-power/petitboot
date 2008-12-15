
#include <stdlib.h>
#include <signal.h>

#include "udev.h"
#include "discover-server.h"
#include "device-handler.h"
#include "waiter.h"
#include "log.h"

int main(void)
{
	struct device_handler *handler;
	struct discover_server *server;
	struct udev *udev;

	/* we look for closed sockets when we write, so ignore SIGPIPE */
	signal(SIGPIPE, SIG_IGN);

	server = discover_server_init();
	if (!server)
		return EXIT_FAILURE;

	handler = device_handler_init(server);
	if (!handler)
		return EXIT_FAILURE;

	discover_server_set_device_source(server, handler);

	udev = udev_init();
	if (!udev)
		return EXIT_FAILURE;

	for (;;) {
		if (waiter_poll())
			return EXIT_FAILURE;
	}


	return EXIT_SUCCESS;
}
