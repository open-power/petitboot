#include <assert.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <mntent.h>
#include <locale.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/mount.h>

#include <talloc/talloc.h>
#include <list/list.h>
#include <log/log.h>
#include <types/types.h>
#include <system/system.h>
#include <process/process.h>
#include <url/url.h>
#include <i18n/i18n.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>

#include "device-handler.h"
#include "discover-server.h"
#include "user-event.h"
#include "platform.h"
#include "event.h"
#include "parser.h"
#include "resource.h"
#include "paths.h"
#include "sysinfo.h"
#include "boot.h"
#include "udev.h"
#include "network.h"

struct device_handler {
	struct discover_server	*server;
	int			dry_run;

	struct pb_udev		*udev;
	struct network		*network;
	struct user_event	*user_event;

	struct discover_device	**devices;
	unsigned int		n_devices;

	struct waitset		*waitset;
	struct waiter		*timeout_waiter;
	bool			autoboot_enabled;
	unsigned int		sec_to_boot;

	struct discover_boot_option *default_boot_option;
	struct list		unresolved_boot_options;

	struct boot_task	*pending_boot;
	bool			pending_boot_is_default;
};

static int mount_device(struct discover_device *dev);
static int umount_device(struct discover_device *dev);

static int device_handler_init_sources(struct device_handler *handler);
static void device_handler_reinit_sources(struct device_handler *handler);

static void device_handler_update_lang(const char *lang);

void discover_context_add_boot_option(struct discover_context *ctx,
		struct discover_boot_option *boot_option)
{
	boot_option->source = ctx->parser;
	list_add_tail(&ctx->boot_options, &boot_option->list);
	talloc_steal(ctx, boot_option);
}

/**
 * device_handler_get_device_count - Get the count of current handler devices.
 */

int device_handler_get_device_count(const struct device_handler *handler)
{
	return handler->n_devices;
}

/**
 * device_handler_get_device - Get a handler device by index.
 */

const struct discover_device *device_handler_get_device(
	const struct device_handler *handler, unsigned int index)
{
	if (index >= handler->n_devices) {
		assert(0 && "bad index");
		return NULL;
	}

	return handler->devices[index];
}

struct discover_boot_option *discover_boot_option_create(
		struct discover_context *ctx,
		struct discover_device *device)
{
	struct discover_boot_option *opt;

	opt = talloc_zero(ctx, struct discover_boot_option);
	opt->option = talloc_zero(opt, struct boot_option);
	opt->device = device;

	return opt;
}

static int device_match_uuid(struct discover_device *dev, const char *uuid)
{
	return dev->uuid && !strcmp(dev->uuid, uuid);
}

static int device_match_label(struct discover_device *dev, const char *label)
{
	return dev->label && !strcmp(dev->label, label);
}

static int device_match_id(struct discover_device *dev, const char *id)
{
	return !strcmp(dev->device->id, id);
}

static int device_match_serial(struct discover_device *dev, const char *serial)
{
	const char *val = discover_device_get_param(dev, "ID_SERIAL");
	return val && !strcmp(val, serial);
}

static struct discover_device *device_lookup(
		struct device_handler *device_handler,
		int (match_fn)(struct discover_device *, const char *),
		const char *str)
{
	struct discover_device *dev;
	unsigned int i;

	if (!str)
		return NULL;

	for (i = 0; i < device_handler->n_devices; i++) {
		dev = device_handler->devices[i];

		if (match_fn(dev, str))
			return dev;
	}

	return NULL;
}

struct discover_device *device_lookup_by_name(struct device_handler *handler,
		const char *name)
{
	if (!strncmp(name, "/dev/", strlen("/dev/")))
		name += strlen("/dev/");

	return device_lookup_by_id(handler, name);
}

struct discover_device *device_lookup_by_uuid(
		struct device_handler *device_handler,
		const char *uuid)
{
	return device_lookup(device_handler, device_match_uuid, uuid);
}

struct discover_device *device_lookup_by_label(
		struct device_handler *device_handler,
		const char *label)
{
	return device_lookup(device_handler, device_match_label, label);
}

struct discover_device *device_lookup_by_id(
		struct device_handler *device_handler,
		const char *id)
{
	return device_lookup(device_handler, device_match_id, id);
}

struct discover_device *device_lookup_by_serial(
		struct device_handler *device_handler,
		const char *serial)
{
	return device_lookup(device_handler, device_match_serial, serial);
}

void device_handler_destroy(struct device_handler *handler)
{
	talloc_free(handler);
}

static int destroy_device(void *arg)
{
	struct discover_device *dev = arg;

	umount_device(dev);

	return 0;
}

struct discover_device *discover_device_create(struct device_handler *handler,
		const char *id)
{
	struct discover_device *dev;

	dev = device_lookup_by_id(handler, id);
	if (dev)
		return dev;

	dev = talloc_zero(handler, struct discover_device);
	dev->device = talloc_zero(dev, struct device);
	dev->device->id = talloc_strdup(dev->device, id);
	list_init(&dev->params);
	list_init(&dev->boot_options);

	talloc_set_destructor(dev, destroy_device);

	return dev;
}

struct discover_device_param {
	char			*name;
	char			*value;
	struct list_item	list;
};

void discover_device_set_param(struct discover_device *device,
		const char *name, const char *value)
{
	struct discover_device_param *param;
	bool found = false;

	list_for_each_entry(&device->params, param, list) {
		if (!strcmp(param->name, name)) {
			found = true;
			break;
		}
	}

	if (!found) {
		if (!value)
			return;
		param = talloc(device, struct discover_device_param);
		param->name = talloc_strdup(param, name);
		list_add(&device->params, &param->list);
	} else {
		if (!value) {
			list_remove(&param->list);
			talloc_free(param);
			return;
		}
		talloc_free(param->value);
	}

	param->value = talloc_strdup(param, value);
}

const char *discover_device_get_param(struct discover_device *device,
		const char *name)
{
	struct discover_device_param *param;

	list_for_each_entry(&device->params, param, list) {
		if (!strcmp(param->name, name))
			return param->value;
	}
	return NULL;
}

struct device_handler *device_handler_init(struct discover_server *server,
		struct waitset *waitset, int dry_run)
{
	struct device_handler *handler;
	int rc;

	handler = talloc_zero(NULL, struct device_handler);
	handler->server = server;
	handler->waitset = waitset;
	handler->dry_run = dry_run;
	handler->autoboot_enabled = config_get()->autoboot_enabled;

	list_init(&handler->unresolved_boot_options);

	/* set up our mount point base */
	pb_mkdir_recursive(mount_base());

	parser_init();

	if (config_get()->safe_mode)
		return handler;

	rc = device_handler_init_sources(handler);
	if (rc) {
		talloc_free(handler);
		return NULL;
	}

	return handler;
}

void device_handler_reinit(struct device_handler *handler)
{
	struct discover_boot_option *opt, *tmp;
	unsigned int i;

	device_handler_cancel_default(handler);

	/* free unresolved boot options */
	list_for_each_entry_safe(&handler->unresolved_boot_options,
			opt, tmp, list)
		talloc_free(opt);
	list_init(&handler->unresolved_boot_options);

	/* drop all devices */
	for (i = 0; i < handler->n_devices; i++)
		discover_server_notify_device_remove(handler->server,
				handler->devices[i]->device);

	talloc_free(handler->devices);
	handler->devices = NULL;
	handler->n_devices = 0;

	device_handler_reinit_sources(handler);
}

void device_handler_remove(struct device_handler *handler,
		struct discover_device *device)
{
	struct discover_boot_option *opt, *tmp;
	unsigned int i;

	for (i = 0; i < handler->n_devices; i++)
		if (handler->devices[i] == device)
			break;

	if (i == handler->n_devices) {
		talloc_free(device);
		return;
	}

	/* Free any unresolved options, as they're currently allocated
	 * against the handler */
	list_for_each_entry_safe(&handler->unresolved_boot_options,
			opt, tmp, list) {
		if (opt->device != device)
			continue;
		list_remove(&opt->list);
		talloc_free(opt);
	}

	/* if this is a network device, we have to unregister it from the
	 * network code */
	if (device->device->type == DEVICE_TYPE_NETWORK)
		network_unregister_device(handler->network, device);

	handler->n_devices--;
	memmove(&handler->devices[i], &handler->devices[i + 1],
		(handler->n_devices - i) * sizeof(handler->devices[0]));
	handler->devices = talloc_realloc(handler, handler->devices,
		struct discover_device *, handler->n_devices);

	if (device->notified)
		discover_server_notify_device_remove(handler->server,
							device->device);

	talloc_free(device);
}

static void boot_status(void *arg, struct boot_status *status)
{
	struct device_handler *handler = arg;

	discover_server_notify_boot_status(handler->server, status);
}

static void countdown_status(struct device_handler *handler,
		struct discover_boot_option *opt, unsigned int sec)
{
	struct boot_status status;

	status.type = BOOT_STATUS_INFO;
	status.progress = -1;
	status.detail = NULL;
	status.message = talloc_asprintf(handler,
			_("Booting in %d sec: %s"), sec, opt->option->name);

	discover_server_notify_boot_status(handler->server, &status);

	talloc_free(status.message);
}

static int default_timeout(void *arg)
{
	struct device_handler *handler = arg;
	struct discover_boot_option *opt;

	if (!handler->default_boot_option)
		return 0;

	if (handler->pending_boot)
		return 0;

	opt = handler->default_boot_option;

	if (handler->sec_to_boot) {
		countdown_status(handler, opt, handler->sec_to_boot);
		handler->sec_to_boot--;
		handler->timeout_waiter = waiter_register_timeout(
						handler->waitset, 1000,
						default_timeout, handler);
		return 0;
	}

	handler->timeout_waiter = NULL;

	pb_log("Timeout expired, booting default option %s\n", opt->option->id);

	handler->pending_boot = boot(handler, handler->default_boot_option,
			NULL, handler->dry_run, boot_status, handler);
	handler->pending_boot_is_default = true;
	return 0;
}

static bool priority_match(struct boot_priority *prio,
		struct discover_boot_option *opt)
{
	return prio->type == opt->device->device->type ||
		prio->type == DEVICE_TYPE_ANY;
}

static int default_option_priority(struct discover_boot_option *opt)
{
	const struct config *config;
	struct boot_priority *prio;
	unsigned int i;

	config = config_get();

	for (i = 0; i < config->n_boot_priorities; i++) {
		prio = &config->boot_priorities[i];
		if (priority_match(prio, opt))
			return prio->priority;
	}

	return 0;
}

static bool device_allows_default(struct discover_device *dev)
{
	const char *dev_str;

	dev_str = config_get()->boot_device;

	if (!dev_str || !strlen(dev_str))
		return true;

	/* default devices are specified by UUIDs at present */
	if (strcmp(dev->uuid, dev_str))
		return false;

	return true;
}

static void set_default(struct device_handler *handler,
		struct discover_boot_option *opt)
{
	int new_prio;

	if (!handler->autoboot_enabled)
		return;

	/* do we allow default-booting from this device? */
	if (!device_allows_default(opt->device))
		return;

	new_prio = default_option_priority(opt);

	/* A negative priority indicates that we don't want to boot this device
	 * by default */
	if (new_prio < 0)
		return;

	/* Resolve any conflicts: if we have a new default option, it only
	 * replaces the current if it has a higher priority. */
	if (handler->default_boot_option) {
		int cur_prio;

		cur_prio = default_option_priority(
					handler->default_boot_option);

		if (new_prio > cur_prio) {
			handler->default_boot_option = opt;
			/* extend the timeout a little, so the user sees some
			 * indication of the change */
			handler->sec_to_boot += 2;
		}

		return;
	}

	handler->sec_to_boot = config_get()->autoboot_timeout_sec;
	handler->default_boot_option = opt;

	pb_log("Boot option %s set as default, timeout %u sec.\n",
	       opt->option->id, handler->sec_to_boot);

	default_timeout(handler);
}

static bool resource_is_resolved(struct resource *res)
{
	return !res || res->resolved;
}

/* We only use this in an assert, which will disappear if we're compiling
 * with NDEBUG, so we need the 'used' attribute for these builds */
static bool __attribute__((used)) boot_option_is_resolved(
		struct discover_boot_option *opt)
{
	return resource_is_resolved(opt->boot_image) &&
		resource_is_resolved(opt->initrd) &&
		resource_is_resolved(opt->dtb) &&
		resource_is_resolved(opt->icon);
}

static bool resource_resolve(struct resource *res, const char *name,
		struct discover_boot_option *opt,
		struct device_handler *handler)
{
	struct parser *parser = opt->source;

	if (resource_is_resolved(res))
		return true;

	pb_debug("Attempting to resolve resource %s->%s with parser %s\n",
			opt->option->id, name, parser->name);
	parser->resolve_resource(handler, res);

	return res->resolved;
}

static bool boot_option_resolve(struct discover_boot_option *opt,
		struct device_handler *handler)
{
	return resource_resolve(opt->boot_image, "boot_image", opt, handler) &&
		resource_resolve(opt->initrd, "initrd", opt, handler) &&
		resource_resolve(opt->dtb, "dtb", opt, handler) &&
		resource_resolve(opt->icon, "icon", opt, handler);
}

static void boot_option_finalise(struct device_handler *handler,
		struct discover_boot_option *opt)
{
	assert(boot_option_is_resolved(opt));

	/* check that the parsers haven't set any of the final data */
	assert(!opt->option->boot_image_file);
	assert(!opt->option->initrd_file);
	assert(!opt->option->dtb_file);
	assert(!opt->option->icon_file);
	assert(!opt->option->device_id);

	if (opt->boot_image)
		opt->option->boot_image_file = opt->boot_image->url->full;
	if (opt->initrd)
		opt->option->initrd_file = opt->initrd->url->full;
	if (opt->dtb)
		opt->option->dtb_file = opt->dtb->url->full;
	if (opt->icon)
		opt->option->icon_file = opt->icon->url->full;

	opt->option->device_id = opt->device->device->id;

	if (opt->option->is_default)
		set_default(handler, opt);
}

static void notify_boot_option(struct device_handler *handler,
		struct discover_boot_option *opt)
{
	struct discover_device *dev = opt->device;

	if (!dev->notified)
		discover_server_notify_device_add(handler->server,
						  opt->device->device);
	dev->notified = true;
	discover_server_notify_boot_option_add(handler->server, opt->option);
}

static void process_boot_option_queue(struct device_handler *handler)
{
	struct discover_boot_option *opt, *tmp;

	list_for_each_entry_safe(&handler->unresolved_boot_options,
			opt, tmp, list) {

		pb_debug("queue: attempting resolution for %s\n",
				opt->option->id);

		if (!boot_option_resolve(opt, handler))
			continue;

		pb_debug("\tresolved!\n");

		list_remove(&opt->list);
		list_add_tail(&opt->device->boot_options, &opt->list);
		talloc_steal(opt->device, opt);
		boot_option_finalise(handler, opt);
		notify_boot_option(handler, opt);
	}
}

struct discover_context *device_handler_discover_context_create(
		struct device_handler *handler,
		struct discover_device *device)
{
	struct discover_context *ctx;

	ctx = talloc_zero(handler, struct discover_context);
	ctx->device = device;
	ctx->network = handler->network;
	list_init(&ctx->boot_options);

	return ctx;
}

/**
 * context_commit - Commit a temporary discovery context to the handler,
 * and notify the clients about any new options / devices
 */
void device_handler_discover_context_commit(struct device_handler *handler,
		struct discover_context *ctx)
{
	struct discover_device *dev = ctx->device;
	struct discover_boot_option *opt, *tmp;

	if (!device_lookup_by_id(handler, dev->device->id))
		device_handler_add_device(handler, dev);

	/* move boot options from the context to the device */
	list_for_each_entry_safe(&ctx->boot_options, opt, tmp, list) {
		list_remove(&opt->list);

		if (boot_option_resolve(opt, handler)) {
			pb_log("boot option %s is resolved, "
					"sending to clients\n",
					opt->option->id);
			list_add_tail(&dev->boot_options, &opt->list);
			talloc_steal(dev, opt);
			boot_option_finalise(handler, opt);
			notify_boot_option(handler, opt);
		} else {
			if (!opt->source->resolve_resource) {
				pb_log("parser %s gave us an unresolved "
					"resource (%s), but no way to "
					"resolve it\n",
					opt->source->name, opt->option->id);
				talloc_free(opt);
			} else {
				pb_log("boot option %s is unresolved, "
						"adding to queue\n",
						opt->option->id);
				list_add(&handler->unresolved_boot_options,
						&opt->list);
				talloc_steal(handler, opt);
			}
		}
	}
}

void device_handler_add_device(struct device_handler *handler,
		struct discover_device *device)
{
	handler->n_devices++;
	handler->devices = talloc_realloc(handler, handler->devices,
				struct discover_device *, handler->n_devices);
	handler->devices[handler->n_devices - 1] = device;

	if (device->device->type == DEVICE_TYPE_NETWORK)
		network_register_device(handler->network, device);
}

/* Start discovery on a hotplugged device. The device will be in our devices
 * array, but has only just been initialised by the hotplug source.
 */
int device_handler_discover(struct device_handler *handler,
		struct discover_device *dev)
{
	struct discover_context *ctx;
	int rc;

	process_boot_option_queue(handler);

	/* create our context */
	ctx = device_handler_discover_context_create(handler, dev);

	rc = mount_device(dev);
	if (rc)
		goto out;

	/* add this device to our system info */
	system_info_register_blockdev(dev->device->id, dev->uuid,
			dev->mount_path);

	/* run the parsers. This will populate the ctx's boot_option list. */
	iterate_parsers(ctx);

	/* add discovered stuff to the handler */
	device_handler_discover_context_commit(handler, ctx);

out:
	talloc_free(ctx);

	return 0;
}

/* Incoming dhcp event */
int device_handler_dhcp(struct device_handler *handler,
		struct discover_device *dev, struct event *event)
{
	struct discover_context *ctx;

	/* create our context */
	ctx = device_handler_discover_context_create(handler, dev);
	ctx->event = event;

	iterate_parsers(ctx);

	device_handler_discover_context_commit(handler, ctx);

	talloc_free(ctx);

	return 0;
}

/* incoming conf event */
int device_handler_conf(struct device_handler *handler,
		struct discover_device *dev, struct pb_url *url)
{
        struct discover_context *ctx;

        /* create our context */
        ctx = device_handler_discover_context_create(handler, dev);
        ctx->conf_url = url;

        iterate_parsers(ctx);

        device_handler_discover_context_commit(handler, ctx);

        talloc_free(ctx);

        return 0;
}

static struct discover_boot_option *find_boot_option_by_id(
		struct device_handler *handler, const char *id)
{
	unsigned int i;

	for (i = 0; i < handler->n_devices; i++) {
		struct discover_device *dev = handler->devices[i];
		struct discover_boot_option *opt;

		list_for_each_entry(&dev->boot_options, opt, list)
			if (!strcmp(opt->option->id, id))
				return opt;
	}

	return NULL;
}

void device_handler_boot(struct device_handler *handler,
		struct boot_command *cmd)
{
	struct discover_boot_option *opt = NULL;

	if (cmd->option_id && strlen(cmd->option_id))
		opt = find_boot_option_by_id(handler, cmd->option_id);

	if (handler->pending_boot)
		boot_cancel(handler->pending_boot);

	platform_finalise_config();

	handler->pending_boot = boot(handler, opt, cmd, handler->dry_run,
			boot_status, handler);
	handler->pending_boot_is_default = false;
}

void device_handler_cancel_default(struct device_handler *handler)
{
	struct boot_status status;

	if (handler->timeout_waiter)
		waiter_remove(handler->timeout_waiter);

	handler->timeout_waiter = NULL;
	handler->autoboot_enabled = false;

	/* we only send status if we had a default boot option queued */
	if (!handler->default_boot_option)
		return;

	pb_log("Cancelling default boot option\n");

	if (handler->pending_boot && handler->pending_boot_is_default) {
		boot_cancel(handler->pending_boot);
		handler->pending_boot = NULL;
		handler->pending_boot_is_default = false;
	}

	handler->default_boot_option = NULL;

	status.type = BOOT_STATUS_INFO;
	status.progress = -1;
	status.detail = NULL;
	status.message = _("Default boot cancelled");

	discover_server_notify_boot_status(handler->server, &status);
}

void device_handler_update_config(struct device_handler *handler,
		struct config *config)
{
	int rc;

	rc = config_set(config);
	if (rc)
		return;

	discover_server_notify_config(handler->server, config);
	device_handler_update_lang(config->lang);
	device_handler_reinit(handler);
}

static char *device_from_addr(void *ctx, struct pb_url *url)
{
	char *ipaddr, *buf, *tok, *dev = NULL;
	const char *delim = " ";
	struct sockaddr_in *ip;
	struct sockaddr_in si;
	struct addrinfo *res;
	struct process *p;
	int rc;

	/* Note: IPv4 only */
	rc = inet_pton(AF_INET, url->host, &(si.sin_addr));
	if (rc > 0) {
		ipaddr = url->host;
	} else {
		/* need to turn hostname into a valid IP */
		rc = getaddrinfo(url->host, NULL, NULL, &res);
		if (rc) {
			pb_debug("%s: Invalid URL\n",__func__);
			return NULL;
		}
		ipaddr = talloc_array(ctx,char,INET_ADDRSTRLEN);
		ip = (struct sockaddr_in *) res->ai_addr;
		inet_ntop(AF_INET, &(ip->sin_addr), ipaddr, INET_ADDRSTRLEN);
		freeaddrinfo(res);
	}

	const char *argv[] = {
		pb_system_apps.ip,
		"route", "show", "to", "match",
		ipaddr,
		NULL
	};

	p = process_create(ctx);

	p->path = pb_system_apps.ip;
	p->argv = argv;
	p->keep_stdout = true;

	rc = process_run_sync(p);

	if (rc) {
		/* ip has complained for some reason; most likely
		 * there is no route to the host - bail out */
		pb_debug("%s: No route to %s\n",__func__,url->host);
		return NULL;
	}

	buf = p->stdout_buf;
	/* If a route is found, ip-route output will be of the form
	 * "... dev DEVNAME ... " */
	tok = strtok(buf, delim);
	while (tok) {
		if (!strcmp(tok, "dev")) {
			tok = strtok(NULL, delim);
			dev = talloc_strdup(ctx, tok);
			break;
		}
		tok = strtok(NULL, delim);
	}

	process_release(p);
	if (dev)
		pb_debug("%s: Found interface '%s'\n", __func__,dev);
	return dev;
}


void device_handler_process_url(struct device_handler *handler,
		const char *url)
{
	struct discover_context *ctx;
	struct discover_device *dev;
	struct boot_status *status;
	struct pb_url *pb_url;
	struct event *event;
	struct param *param;

	status = talloc(handler, struct boot_status);

	status->type = BOOT_STATUS_ERROR;
	status->progress = 0;
	status->detail = talloc_asprintf(status,
			_("Received config URL %s"), url);

	if (!handler->network) {
		status->message = talloc_asprintf(handler,
					_("No network configured"));
		goto msg;
	}

	event = talloc(handler, struct event);
	event->type = EVENT_TYPE_USER;
	event->action = EVENT_ACTION_CONF;

	event->params = talloc_array(event, struct param, 1);
	param = &event->params[0];
	param->name = talloc_strdup(event, "pxeconffile");
	param->value = talloc_strdup(event, url);
	event->n_params = 1;

	pb_url = pb_url_parse(event, event->params->value);
	if (!pb_url || !pb_url->host) {
		status->message = talloc_asprintf(handler,
					_("Invalid config URL!"));
		goto msg;
	}

	event->device = device_from_addr(event, pb_url);
	if (!event->device) {
		status->message = talloc_asprintf(status,
					_("Unable to route to host %s"),
					pb_url->host);
		goto msg;
	}

	dev = discover_device_create(handler, event->device);
	ctx = device_handler_discover_context_create(handler, dev);
	ctx->event = event;

	iterate_parsers(ctx);

	device_handler_discover_context_commit(handler, ctx);

	talloc_free(ctx);

	status->type = BOOT_STATUS_INFO;
	status->message = talloc_asprintf(status, _("Config file %s parsed"),
					pb_url->file);
msg:
	boot_status(handler, status);
	talloc_free(status);
}

#ifndef PETITBOOT_TEST

static void device_handler_update_lang(const char *lang)
{
	const char *cur_lang;

	if (!lang)
		return;

	cur_lang = setlocale(LC_ALL, NULL);
	if (cur_lang && !strcmp(cur_lang, lang))
		return;

	setlocale(LC_ALL, lang);
}

static int device_handler_init_sources(struct device_handler *handler)
{
	/* init our device sources: udev, network and user events */
	handler->udev = udev_init(handler, handler->waitset);
	if (!handler->udev)
		return -1;

	handler->network = network_init(handler, handler->waitset,
			handler->dry_run);
	if (!handler->network)
		return -1;

	handler->user_event = user_event_init(handler, handler->waitset);
	if (!handler->user_event)
		return -1;

	return 0;
}

static void device_handler_reinit_sources(struct device_handler *handler)
{
	/* if we haven't initialised sources previously (becuase we started in
	 * safe mode), then init once here. */
	if (!(handler->udev || handler->network || handler->user_event)) {
		device_handler_init_sources(handler);
		return;
	}

	udev_reinit(handler->udev);

	network_shutdown(handler->network);
	handler->network = network_init(handler, handler->waitset,
			handler->dry_run);
}

static bool check_existing_mount(struct discover_device *dev)
{
	struct stat devstat, mntstat;
	struct mntent *mnt;
	FILE *fp;
	int rc;

	rc = stat(dev->device_path, &devstat);
	if (rc) {
		pb_debug("%s: stat failed: %s\n", __func__, strerror(errno));
		return false;
	}

	if (!S_ISBLK(devstat.st_mode)) {
		pb_debug("%s: %s isn't a block device?\n", __func__,
				dev->device_path);
		return false;
	}

	fp = fopen("/proc/self/mounts", "r");

	for (;;) {
		mnt = getmntent(fp);
		if (!mnt)
			break;

		if (!mnt->mnt_fsname || mnt->mnt_fsname[0] != '/')
			continue;

		rc = stat(mnt->mnt_fsname, &mntstat);
		if (rc)
			continue;

		if (!S_ISBLK(mntstat.st_mode))
			continue;

		if (mntstat.st_rdev == devstat.st_rdev) {
			dev->mount_path = talloc_strdup(dev, mnt->mnt_dir);
			dev->mounted_rw = !!hasmntopt(mnt, "rw");
			dev->mounted = true;
			dev->unmount = false;

			pb_debug("%s: %s is already mounted (r%c) at %s\n",
					__func__, dev->device_path,
					dev->mounted_rw ? 'w' : 'o',
					mnt->mnt_dir);
			break;
		}
	}

	fclose(fp);

	return mnt != NULL;
}

static int mount_device(struct discover_device *dev)
{
	const char *fstype;
	int rc;

	if (!dev->device_path)
		return -1;

	if (dev->mounted)
		return 0;

	if (check_existing_mount(dev))
		return 0;

	fstype = discover_device_get_param(dev, "ID_FS_TYPE");
	if (!fstype)
		return 0;

	dev->mount_path = join_paths(dev, mount_base(),
					dev->device_path);

	if (pb_mkdir_recursive(dev->mount_path)) {
		pb_log("couldn't create mount directory %s: %s\n",
				dev->mount_path, strerror(errno));
		goto err_free;
	}

	pb_log("mounting device %s read-only\n", dev->device_path);
	errno = 0;
	rc = mount(dev->device_path, dev->mount_path, fstype,
			MS_RDONLY | MS_SILENT, "");
	if (!rc) {
		dev->mounted = true;
		dev->mounted_rw = false;
		dev->unmount = true;
		return 0;
	}

	pb_log("couldn't mount device %s: mount failed: %s\n",
			dev->device_path, strerror(errno));

	pb_rmdir_recursive(mount_base(), dev->mount_path);
err_free:
	talloc_free(dev->mount_path);
	dev->mount_path = NULL;
	return -1;
}

static int umount_device(struct discover_device *dev)
{
	int rc;

	if (!dev->mounted || !dev->unmount)
		return 0;

	pb_log("unmounting device %s\n", dev->device_path);
	rc = umount(dev->mount_path);
	if (rc)
		return -1;

	dev->mounted = false;

	pb_rmdir_recursive(mount_base(), dev->mount_path);

	talloc_free(dev->mount_path);
	dev->mount_path = NULL;

	return 0;
}

int device_request_write(struct discover_device *dev, bool *release)
{
	int rc;

	*release = false;

	if (!dev->mounted)
		return -1;

	if (dev->mounted_rw)
		return 0;

	pb_log("remounting device %s read-write\n", dev->device_path);
	rc = mount(dev->device_path, dev->mount_path, "",
			MS_REMOUNT | MS_SILENT, "");
	if (rc)
		return -1;

	dev->mounted_rw = true;
	*release = true;
	return 0;
}

void device_release_write(struct discover_device *dev, bool release)
{
	if (!release)
		return;

	pb_log("remounting device %s read-only\n", dev->device_path);
	mount(dev->device_path, dev->mount_path, "",
			MS_REMOUNT | MS_RDONLY | MS_SILENT, "");
	dev->mounted_rw = false;
}

#else

static void device_handler_update_lang(const char *lang __attribute__((unused)))
{
}

static int device_handler_init_sources(
		struct device_handler *handler __attribute__((unused)))
{
	return 0;
}

static void device_handler_reinit_sources(
		struct device_handler *handler __attribute__((unused)))
{
}

static int umount_device(struct discover_device *dev __attribute__((unused)))
{
	return 0;
}

static int __attribute__((unused)) mount_device(
		struct discover_device *dev __attribute__((unused)))
{
	return 0;
}

int device_request_write(struct discover_device *dev __attribute__((unused)),
		bool *release)
{
	*release = true;
	return 0;
}

void device_release_write(struct discover_device *dev __attribute__((unused)),
	bool release __attribute__((unused)))
{
}

#endif

