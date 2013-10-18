
#include <assert.h>

#include <talloc/talloc.h>
#include <types/types.h>

#include "device-handler.h"

typedef void (*boot_status_fn)(void *arg, struct boot_status *);

void discover_server_notify_device_add(struct discover_server *server,
		struct device *device)
{
	(void)server;
	(void)device;
}

void discover_server_notify_boot_option_add(struct discover_server *server,
		struct boot_option *option)
{
	(void)server;
	(void)option;
}

void discover_server_notify_device_remove(struct discover_server *server,
		struct device *device)
{
	(void)server;
	(void)device;
}

void discover_server_notify_boot_status(struct discover_server *server,
		struct boot_status *status)
{
	(void)server;
	(void)status;
}

void discover_server_notify_config(struct discover_server *server,
		struct config *config)
{
	(void)server;
	(void)config;
}

void parser_init(void)
{
}

void iterate_parsers(struct discover_context *ctx)
{
	(void)ctx;
	assert(false);
}

struct boot_task *boot(void *ctx, struct discover_boot_option *opt,
		struct boot_command *cmd, int dry_run,
		boot_status_fn status_fn, void *status_arg)
{
	(void)ctx;
	(void)opt;
	(void)cmd;
	(void)dry_run;
	(void)status_fn;
	(void)status_arg;
	assert(false);
}

void boot_cancel(struct boot_task *task)
{
	(void)task;
}
