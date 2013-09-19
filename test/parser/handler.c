
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

void parser_init(void)
{
}

void iterate_parsers(struct discover_context *ctx, enum conf_method method)
{
	(void)ctx;
	(void)method;
	assert(false);
}

int boot(void *ctx, struct discover_boot_option *opt, struct boot_command *cmd,
		int dry_run, boot_status_fn status_fn, void *status_arg)
{
	(void)ctx;
	(void)opt;
	(void)cmd;
	(void)dry_run;
	(void)status_fn;
	(void)status_arg;
	assert(false);
}
