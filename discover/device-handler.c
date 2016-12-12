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
#include "devmapper.h"
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
#include "ipmi.h"

enum default_priority {
	DEFAULT_PRIORITY_REMOTE		= 1,
	DEFAULT_PRIORITY_LOCAL_FIRST	= 2,
	DEFAULT_PRIORITY_LOCAL_LAST	= 0xfe,
	DEFAULT_PRIORITY_DISABLED	= 0xff,
};

struct device_handler {
	struct discover_server	*server;
	int			dry_run;

	struct pb_udev		*udev;
	struct network		*network;
	struct user_event	*user_event;

	struct discover_device	**devices;
	unsigned int		n_devices;

	struct ramdisk_device	**ramdisks;
	unsigned int		n_ramdisks;

	struct waitset		*waitset;
	struct waiter		*timeout_waiter;
	bool			autoboot_enabled;
	unsigned int		sec_to_boot;

	struct discover_boot_option *default_boot_option;
	int			default_boot_option_priority;

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
		const char *uuid, const char *id)
{
	struct discover_device *dev;

	if (uuid)
		dev = device_lookup_by_uuid(handler, uuid);
	else
		dev = device_lookup_by_id(handler, id);

	if (dev)
		return dev;

	dev = talloc_zero(handler, struct discover_device);
	dev->device = talloc_zero(dev, struct device);
	dev->device->id = talloc_strdup(dev->device, id);
	dev->uuid = talloc_strdup(dev, uuid);
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
	struct ramdisk_device *ramdisk;
	unsigned int i;

	device_handler_cancel_default(handler);

	/* free unresolved boot options */
	list_for_each_entry_safe(&handler->unresolved_boot_options,
			opt, tmp, list)
		talloc_free(opt);
	list_init(&handler->unresolved_boot_options);

	/* drop all devices */
	for (i = 0; i < handler->n_devices; i++) {
		discover_server_notify_device_remove(handler->server,
				handler->devices[i]->device);
		ramdisk = handler->devices[i]->ramdisk;
		talloc_free(handler->devices[i]);
		talloc_free(ramdisk);
	}

	talloc_free(handler->devices);
	handler->devices = NULL;
	handler->n_devices = 0;
	talloc_free(handler->ramdisks);
	handler->ramdisks = NULL;
	handler->n_ramdisks = 0;

	device_handler_reinit_sources(handler);
}

void device_handler_remove(struct device_handler *handler,
		struct discover_device *device)
{
	struct discover_boot_option *opt, *tmp;
	unsigned int i;

	list_for_each_entry_safe(&device->boot_options, opt, tmp, list) {
		if (opt == handler->default_boot_option) {
			pb_log("Default option %s cancelled since device removed",
					opt->option->name);
			device_handler_cancel_default(handler);
			break;
		}
	}

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

void device_handler_status(struct device_handler *handler,
		struct status *status)
{
	discover_server_notify_boot_status(handler->server, status);
}

static void _device_handler_vstatus(struct device_handler *handler,
		enum status_type type, const char *fmt, va_list ap)
{
	struct status status;

	status.type = type;
	status.message = talloc_vasprintf(handler, fmt, ap);

	device_handler_status(handler, &status);

	talloc_free(status.message);
}

void device_handler_status_info(struct device_handler *handler,
		const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	_device_handler_vstatus(handler, STATUS_INFO, fmt, ap);
	va_end(ap);
}

void device_handler_status_err(struct device_handler *handler,
		const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	_device_handler_vstatus(handler, STATUS_ERROR, fmt, ap);
	va_end(ap);
}

static void device_handler_boot_status_cb(void *arg, struct status *status)
{
	device_handler_status(arg, status);
}

static void countdown_status(struct device_handler *handler,
		struct discover_boot_option *opt, unsigned int sec)
{
	struct status status;

	status.type = STATUS_INFO;
	status.message = talloc_asprintf(handler,
			_("Booting in %d sec: %s"), sec, opt->option->name);

	device_handler_status(handler, &status);

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

	platform_pre_boot();

	handler->pending_boot = boot(handler, handler->default_boot_option,
			NULL, handler->dry_run, device_handler_boot_status_cb,
			handler);
	handler->pending_boot_is_default = true;
	return 0;
}

struct {
	enum ipmi_bootdev	ipmi_type;
	enum device_type	device_type;
} device_type_map[] = {
	{ IPMI_BOOTDEV_NETWORK, DEVICE_TYPE_NETWORK },
	{ IPMI_BOOTDEV_DISK, DEVICE_TYPE_DISK },
	{ IPMI_BOOTDEV_DISK, DEVICE_TYPE_USB },
	{ IPMI_BOOTDEV_CDROM, DEVICE_TYPE_OPTICAL },
};

static bool ipmi_device_type_matches(enum ipmi_bootdev ipmi_type,
		enum device_type device_type)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(device_type_map); i++) {
		if (device_type_map[i].device_type == device_type)
			return device_type_map[i].ipmi_type == ipmi_type;
	}

	return false;
}

static int autoboot_option_priority(const struct config *config,
				struct discover_boot_option *opt)
{
	enum device_type type = opt->device->device->type;
	const char *uuid = opt->device->uuid;
	struct autoboot_option *auto_opt;
	unsigned int i;

	for (i = 0; i < config->n_autoboot_opts; i++) {
		auto_opt = &config->autoboot_opts[i];
		if (auto_opt->boot_type == BOOT_DEVICE_UUID)
			if (!strcmp(auto_opt->uuid, uuid))
				return DEFAULT_PRIORITY_LOCAL_FIRST + i;

		if (auto_opt->boot_type == BOOT_DEVICE_TYPE)
			if (auto_opt->type == type ||
			    auto_opt->type == DEVICE_TYPE_ANY)
				return DEFAULT_PRIORITY_LOCAL_FIRST + i;
	}

	return -1;
}

/*
 * We have different priorities to resolve conflicts between boot options that
 * report to be the default for their device. This function assigns a priority
 * for these options.
 */
static enum default_priority default_option_priority(
		struct discover_boot_option *opt)
{
	const struct config *config;

	config = config_get();

	/* We give highest priority to IPMI-configured boot options. If
	 * we have an IPMI bootdev configuration set, then we don't allow
	 * any other defaults */
	if (config->ipmi_bootdev) {
		bool ipmi_match = ipmi_device_type_matches(config->ipmi_bootdev,
				opt->device->device->type);
		if (ipmi_match)
			return DEFAULT_PRIORITY_REMOTE;

		pb_debug("handler: disabled default priority due to "
				"non-matching IPMI type %x\n",
				config->ipmi_bootdev);
		return DEFAULT_PRIORITY_DISABLED;
	}

	/* Next, try to match the option against the user-defined autoboot
	 * options, either by device UUID or type. */
	if (config->n_autoboot_opts) {
		int boot_match = autoboot_option_priority(config, opt);
		if (boot_match > 0)
			return boot_match;
	}

	/* If the option didn't match any entry in the array, it is disabled */
	pb_debug("handler: disabled default priority due to "
			"non-matching UUID or type\n");
	return DEFAULT_PRIORITY_DISABLED;
}

static void set_default(struct device_handler *handler,
		struct discover_boot_option *opt)
{
	enum default_priority cur_prio, new_prio;

	if (!handler->autoboot_enabled)
		return;

	pb_debug("handler: new default option: %s\n", opt->option->id);

	new_prio = default_option_priority(opt);

	/* Anything outside our range prevents a default boot */
	if (new_prio >= DEFAULT_PRIORITY_DISABLED)
		return;

	pb_debug("handler: calculated priority %d\n", new_prio);

	/* Resolve any conflicts: if we have a new default option, it only
	 * replaces the current if it has a higher priority. */
	if (handler->default_boot_option) {

		cur_prio = handler->default_boot_option_priority;

		if (new_prio < cur_prio) {
			pb_log("handler: new prio %d beats "
					"old prio %d for %s\n",
					new_prio, cur_prio,
					handler->default_boot_option
						->option->id);
			handler->default_boot_option = opt;
			handler->default_boot_option_priority = new_prio;
			/* extend the timeout a little, so the user sees some
			 * indication of the change */
			handler->sec_to_boot += 2;
		}

		return;
	}

	handler->sec_to_boot = config_get()->autoboot_timeout_sec;
	handler->default_boot_option = opt;
	handler->default_boot_option_priority = new_prio;

	pb_log("handler: boot option %s set as default, timeout %u sec.\n",
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
		resource_is_resolved(opt->args_sig_file) &&
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
		resource_resolve(opt->args_sig_file, "args_sig_file", opt,
			handler) &&
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
	assert(!opt->option->args_sig_file);

	if (opt->boot_image)
		opt->option->boot_image_file = opt->boot_image->url->full;
	if (opt->initrd)
		opt->option->initrd_file = opt->initrd->url->full;
	if (opt->dtb)
		opt->option->dtb_file = opt->dtb->url->full;
	if (opt->icon)
		opt->option->icon_file = opt->icon->url->full;
	if (opt->args_sig_file)
		opt->option->args_sig_file = opt->args_sig_file->url->full;

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

void device_handler_add_ramdisk(struct device_handler *handler,
		const char *path)
{
	struct ramdisk_device *dev;
	unsigned int i;

	if (!path)
		return;

	for (i = 0; i < handler->n_ramdisks; i++)
		if (!strcmp(handler->ramdisks[i]->path, path))
			return;

	dev = talloc_zero(handler, struct ramdisk_device);
	if (!dev) {
		pb_log("Failed to allocate memory to track %s\n", path);
		return;
	}

	dev->path = talloc_strdup(handler, path);

	handler->ramdisks = talloc_realloc(handler, handler->ramdisks,
				struct ramdisk_device *,
				handler->n_ramdisks + 1);
	if (!handler->ramdisks) {
		pb_log("Failed to reallocate memory"
		       "- ramdisk tracking inconsistent!\n");
		return;
	}

	handler->ramdisks[i] = dev;
	i = handler->n_ramdisks++;
}

struct ramdisk_device *device_handler_get_ramdisk(
		struct device_handler *handler)
{
	unsigned int i;
	char *name;
	dev_t id;

	/* Check if free ramdisk exists */
	for (i = 0; i < handler->n_ramdisks; i++)
		if (!handler->ramdisks[i]->snapshot &&
		    !handler->ramdisks[i]->origin &&
		    !handler->ramdisks[i]->base)
			return handler->ramdisks[i];

	/* Otherwise create a new one */
	name = talloc_asprintf(handler, "/dev/ram%d",
			handler->n_ramdisks);
	if (!name) {
		pb_debug("Failed to allocate memory to name /dev/ram%d",
			handler->n_ramdisks);
		return NULL;
	}

	id = makedev(1, handler->n_ramdisks);
	if (mknod(name, S_IFBLK, id)) {
		if (errno == EEXIST) {
			/* We haven't yet received updates for existing
			 * ramdisks - add and use this one */
			pb_debug("Using untracked ramdisk %s\n", name);
		} else {
			pb_log("Failed to create new ramdisk %s: %s\n",
			       name, strerror(errno));
			return NULL;
		}
	}
	device_handler_add_ramdisk(handler, name);
	talloc_free(name);

	return handler->ramdisks[i];
}

void device_handler_release_ramdisk(struct discover_device *device)
{
	struct ramdisk_device *ramdisk = device->ramdisk;

	talloc_free(ramdisk->snapshot);
	talloc_free(ramdisk->origin);
	talloc_free(ramdisk->base);

	ramdisk->snapshot = ramdisk->origin = ramdisk->base = NULL;
	ramdisk->sectors = 0;

	device->ramdisk = NULL;
}

/* Start discovery on a hotplugged device. The device will be in our devices
 * array, but has only just been initialised by the hotplug source.
 */
int device_handler_discover(struct device_handler *handler,
		struct discover_device *dev)
{
	struct discover_context *ctx;
	int rc;

	/*
	 * TRANSLATORS: this string will be passed the type and identifier
	 * of the device. For example, the first parameter could be "Disk",
	 * (which will be translated accordingly) and the second a Linux device
	 * identifier like 'sda1' (which will not be translated)
	 */
	device_handler_status_info(handler, _("Processing %s device %s"),
				device_type_display_name(dev->device->type),
				dev->device->id);

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
	/*
	 * TRANSLATORS: the format specifier in this string is a Linux
	 * device identifier, like 'sda1'
	 */
	device_handler_status_info(handler, _("Processing %s complete"),
				dev->device->id);

	talloc_unlink(handler, ctx);

	return 0;
}

/* Incoming dhcp event */
int device_handler_dhcp(struct device_handler *handler,
		struct discover_device *dev, struct event *event)
{
	struct discover_context *ctx;

	/*
	 * TRANSLATORS: this format specifier will be the name of a network
	 * device, like 'eth0'.
	 */
	device_handler_status_info(handler, _("Processing dhcp event on %s"),
				dev->device->id);

	/* create our context */
	ctx = device_handler_discover_context_create(handler, dev);
	talloc_steal(ctx, event);
	ctx->event = event;

	iterate_parsers(ctx);

	device_handler_discover_context_commit(handler, ctx);

	/*
	 * TRANSLATORS: this format specifier will be the name of a network
	 * device, like 'eth0'.
	 */
	device_handler_status_info(handler, _("Processing %s complete"),
				dev->device->id);

	talloc_unlink(handler, ctx);

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

	platform_pre_boot();

	handler->pending_boot = boot(handler, opt, cmd, handler->dry_run,
			device_handler_boot_status_cb, handler);
	handler->pending_boot_is_default = false;
}

void device_handler_cancel_default(struct device_handler *handler)
{
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

	device_handler_status_info(handler, _("Default boot cancelled"));
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

	if (rc || p->exit_status) {
		/* ip has complained for some reason; most likely
		 * there is no route to the host - bail out */
		pb_debug("%s: `ip` returns non-zero exit status\n", __func__);
		pb_debug("ip buf: %s\n", p->stdout_buf);
		process_release(p);
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
		const char *url, const char *mac, const char *ip)
{
	struct discover_context *ctx;
	struct discover_device *dev;
	struct pb_url *pb_url;
	struct event *event;
	struct param *param;

	if (!handler->network) {
		device_handler_status_err(handler, _("No network configured"));
		return;
	}

	event = talloc(handler, struct event);
	event->type = EVENT_TYPE_USER;
	event->action = EVENT_ACTION_URL;

	if (url[strlen(url) - 1] == '/') {
		event->params = talloc_array(event, struct param, 3);
		param = &event->params[0];
		param->name = talloc_strdup(event, "pxepathprefix");
		param->value = talloc_strdup(event, url);
		param = &event->params[1];
		param->name = talloc_strdup(event, "mac");
		param->value = talloc_strdup(event, mac);
		param = &event->params[2];
		param->name = talloc_strdup(event, "ip");
		param->value = talloc_strdup(event, ip);
		event->n_params = 3;
	} else {
		event->params = talloc_array(event, struct param, 1);
		param = &event->params[0];
		param->name = talloc_strdup(event, "pxeconffile");
		param->value = talloc_strdup(event, url);
		event->n_params = 1;
	}

	pb_url = pb_url_parse(event, event->params->value);
	if (!pb_url || (pb_url->scheme != pb_url_file && !pb_url->host)) {
		device_handler_status_err(handler, _("Invalid config URL!"));
		return;
	}

	if (pb_url->scheme == pb_url_file)
		event->device = talloc_asprintf(event, "local");
	else
		event->device = device_from_addr(event, pb_url);

	if (!event->device) {
		device_handler_status_err(handler,
					_("Unable to route to host %s"),
					pb_url->host);
		return;
	}

	dev = discover_device_create(handler, mac, event->device);
	if (pb_url->scheme == pb_url_file)
		dev->device->type = DEVICE_TYPE_ANY;
	ctx = device_handler_discover_context_create(handler, dev);
	talloc_steal(ctx, event);
	ctx->event = event;

	iterate_parsers(ctx);

	device_handler_discover_context_commit(handler, ctx);

	talloc_unlink(handler, ctx);
}

#ifndef PETITBOOT_TEST

/**
 * context_commit - Commit a temporary discovery context to the handler,
 * and notify the clients about any new options / devices
 */
void device_handler_discover_context_commit(struct device_handler *handler,
		struct discover_context *ctx)
{
	struct discover_device *dev = ctx->device;
	struct discover_boot_option *opt, *tmp;

	if (!device_lookup_by_uuid(handler, dev->uuid))
		device_handler_add_device(handler, dev);

	/* move boot options from the context to the device */
	list_for_each_entry_safe(&ctx->boot_options, opt, tmp, list) {
		list_remove(&opt->list);

		/* All boot options need at least a kernel image */
		if (!opt->boot_image || !opt->boot_image->url) {
			pb_log("boot option %s is missing boot image, ignoring\n",
				opt->option->id);
			talloc_free(opt);
			continue;
		}

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

static inline const char *get_device_path(struct discover_device *dev)
{
	return dev->ramdisk ? dev->ramdisk->snapshot : dev->device_path;
}

static char *check_subvols(struct discover_device *dev)
{
	const char *fstype = discover_device_get_param(dev, "ID_FS_TYPE");
	struct stat sb;
	char *path;
	int rc;

	if (strncmp(fstype, "btrfs", strlen("btrfs")))
		return dev->mount_path;

	/* On btrfs a device's root may be under a subvolume path */
	path = join_paths(dev, dev->mount_path, "@");
	rc = stat(path, &sb);
	if (!rc && S_ISDIR(sb.st_mode)) {
		pb_debug("Using '%s' for btrfs root path\n", path);
		return path;
	}

	talloc_free(path);
	return dev->mount_path;
}

static bool check_existing_mount(struct discover_device *dev)
{
	struct stat devstat, mntstat;
	const char *device_path;
	struct mntent *mnt;
	FILE *fp;
	int rc;

	device_path = get_device_path(dev);

	rc = stat(device_path, &devstat);
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
			dev->root_path = check_subvols(dev);
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

/*
 * Attempt to mount a filesystem safely, while handling certain filesytem-
 * specific options
 */
static int try_mount(const char *device_path, const char *mount_path,
			     const char *fstype, unsigned long flags,
			     bool have_snapshot)
{
	const char *fs, *safe_opts;
	int rc;

	/* Mount ext3 as ext4 instead so 'norecovery' can be used */
	if (strncmp(fstype, "ext3", strlen("ext3")) == 0) {
		pb_debug("Mounting ext3 filesystem as ext4\n");
		fs = "ext4";
	} else
		fs = fstype;

	if (strncmp(fs, "xfs", strlen("xfs")) == 0 ||
	    strncmp(fs, "ext4", strlen("ext4")) == 0)
		safe_opts = "norecovery";
	else
		safe_opts = NULL;

	errno = 0;
	/* If no snapshot is available don't attempt recovery */
	if (!have_snapshot)
		return mount(device_path, mount_path, fs, flags, safe_opts);

	rc = mount(device_path, mount_path, fs, flags, NULL);

	if (!rc)
		return rc;

	/* Mounting failed; some filesystems will fail to mount if a recovery
	 * journal exists (eg. cross-endian XFS), so try again with norecovery
	 * where that option is available.
	 * If mounting read-write just return the error as norecovery is not a
	 * valid option */
	if ((flags & MS_RDONLY) != MS_RDONLY || !safe_opts)
		return rc;

	errno = 0;
	return mount(device_path, mount_path, fs, flags, safe_opts);
}

static int mount_device(struct discover_device *dev)
{
	const char *fstype, *device_path;
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

	device_path = get_device_path(dev);

	pb_log("mounting device %s read-only\n", dev->device_path);
	rc = try_mount(device_path, dev->mount_path, fstype,
		       MS_RDONLY | MS_SILENT, dev->ramdisk);

	if (!rc) {
		dev->mounted = true;
		dev->mounted_rw = false;
		dev->unmount = true;
		dev->root_path = check_subvols(dev);
		return 0;
	}

	pb_log("couldn't mount device %s: mount failed: %s\n",
			device_path, strerror(errno));

	/* If mount fails clean up any snapshot */
	devmapper_destroy_snapshot(dev);

	pb_rmdir_recursive(mount_base(), dev->mount_path);
err_free:
	talloc_free(dev->mount_path);
	dev->mount_path = NULL;
	return -1;
}

static int umount_device(struct discover_device *dev)
{
	const char *device_path;
	int rc;

	if (!dev->mounted || !dev->unmount)
		return 0;

	device_path = get_device_path(dev);

	pb_log("unmounting device %s\n", device_path);
	rc = umount(dev->mount_path);
	if (rc)
		return -1;

	dev->mounted = false;
	devmapper_destroy_snapshot(dev);

	pb_rmdir_recursive(mount_base(), dev->mount_path);

	talloc_free(dev->mount_path);
	dev->mount_path = NULL;
	dev->root_path = NULL;

	return 0;
}

int device_request_write(struct discover_device *dev, bool *release)
{
	const char *fstype, *device_path;
	const struct config *config;
	int rc;

	*release = false;

	config = config_get();
	if (!config->allow_writes)
		return -1;

	if (!dev->mounted)
		return -1;

	if (dev->mounted_rw)
		return 0;

	fstype = discover_device_get_param(dev, "ID_FS_TYPE");

	device_path = get_device_path(dev);

	pb_log("remounting device %s read-write\n", device_path);

	rc = umount(dev->mount_path);
	if (rc) {
		pb_log("Failed to unmount %s: %s\n",
		       dev->mount_path, strerror(errno));
		return -1;
	}

	rc = try_mount(device_path, dev->mount_path, fstype,
		       MS_SILENT, dev->ramdisk);
	if (rc)
		goto mount_ro;

	dev->mounted_rw = true;
	*release = true;
	return 0;

mount_ro:
	pb_log("Unable to remount device %s read-write: %s\n",
	       device_path, strerror(errno));
	rc = try_mount(device_path, dev->mount_path, fstype,
		       MS_RDONLY | MS_SILENT, dev->ramdisk);
	if (rc)
		pb_log("Unable to recover mount for %s: %s\n",
		       device_path, strerror(errno));
	return -1;
}

void device_release_write(struct discover_device *dev, bool release)
{
	const char *fstype, *device_path;

	if (!release)
		return;

	device_path = get_device_path(dev);

	fstype = discover_device_get_param(dev, "ID_FS_TYPE");

	pb_log("remounting device %s read-only\n", device_path);

	if (umount(dev->mount_path)) {
		pb_log("Failed to unmount %s\n", dev->mount_path);
		return;
	}
	dev->mounted_rw = dev->mounted = false;

	if (dev->ramdisk) {
		devmapper_merge_snapshot(dev);
		/* device_path becomes stale after merge */
		device_path = get_device_path(dev);
	}

	if (try_mount(device_path, dev->mount_path, fstype,
		       MS_RDONLY | MS_SILENT, dev->ramdisk))
		pb_log("Failed to remount %s read-only: %s\n",
		       device_path, strerror(errno));
	else
		dev->mounted = true;
}

void device_sync_snapshots(struct device_handler *handler, const char *device)
{
	struct discover_device *dev = NULL;
	unsigned int i;

	if (device) {
		/* Find matching device and sync */
		dev = device_lookup_by_name(handler, device);
		if (!dev) {
			pb_log("%s: device name '%s' unrecognised\n",
				__func__, device);
			return;
		}
		if (dev->ramdisk)
			device_release_write(dev, true);
		else
			pb_log("%s has no snapshot to merge, skipping\n",
				dev->device->id);
		return;
	}

	/* Otherwise sync all relevant devices */
	for (i = 0; i < handler->n_devices; i++) {
		dev = handler->devices[i];
		if (dev->device->type != DEVICE_TYPE_DISK &&
			dev->device->type != DEVICE_TYPE_USB)
			continue;
		if (dev->ramdisk)
			device_release_write(dev, true);
		else
			pb_log("%s has no snapshot to merge, skipping\n",
				dev->device->id);
	}
}

#else

void device_handler_discover_context_commit(
		struct device_handler *handler __attribute__((unused)),
		struct discover_context *ctx __attribute__((unused)))
{
	pb_log("%s stubbed out for test cases\n", __func__);
}

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

void device_sync_snapshots(
		struct device_handler *handler __attribute__((unused)),
		const char *device __attribute__((unused)))
{
}

#endif
