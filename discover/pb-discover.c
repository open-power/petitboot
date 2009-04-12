
#include <assert.h>
#include <stdlib.h>
#include <signal.h>

#include <waiter/waiter.h>
#include <log/log.h>

#include "udev.h"
#include "user-event.h"
#include "discover-server.h"
#include "device-handler.h"

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
	struct user_event *uev;
	FILE *log;

	log = fopen("pb-discover.log", "a");
	assert(log);
	pb_log_set_stream(log);

#if defined(DEBUG)
	pb_log_always_flush(1);
#endif
	pb_log("--- pb-discover ---\n");

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

	uev = user_event_init(handler);
	if (!uev)
		return EXIT_FAILURE;

	udev_trigger(udev);
	user_event_trigger(uev);

	for (running = 1; running;) {
		if (waiter_poll())
			break;
	}

	device_handler_destroy(handler);

	pb_log("--- end ---\n");

	return EXIT_SUCCESS;
}
