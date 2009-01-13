
#include <stdlib.h>
#include <signal.h>

#include "udev.h"
#include "discover-server.h"
#include "device-handler.h"
#include "waiter.h"
#include "log.h"

static int running;

static void sigint_handler(int __attribute__((unused)) signum)
{
	running = 0;
}

int main(void)
{
	struct device_handler *handler;
	struct discover_server *server;
	struct udev *udev;

	/* we look for closed sockets when we write, so ignore SIGPIPE */
	signal(SIGPIPE, SIG_IGN);

	signal(SIGINT, sigint_handler);

	server = discover_server_init();
	if (!server)
		return EXIT_FAILURE;

	handler = device_handler_init(server);
	if (!handler)
		return EXIT_FAILURE;

	discover_server_set_device_source(server, handler);

	udev = udev_init(handler);
	if (!udev)
		return EXIT_FAILURE;

	for (running = 1; running;) {
		if (waiter_poll())
			break;
	}

	device_handler_destroy(handler);


	return EXIT_SUCCESS;
}
