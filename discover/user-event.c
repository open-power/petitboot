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

#include <assert.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>

#include <log/log.h>
#include <url/url.h>
#include <types/types.h>
#include <talloc/talloc.h>
#include <waiter/waiter.h>

#include "device-handler.h"
#include "resource.h"
#include "event.h"
#include "user-event.h"
#include "sysinfo.h"


#define MAC_ADDR_SIZE	6
#define IP_ADDR_SIZE	4

struct user_event {
	struct device_handler *handler;
	int socket;
};

static const char *event_action_name(enum event_action action)
{
	switch (action) {
	case EVENT_ACTION_ADD:
		return "add";
	case EVENT_ACTION_REMOVE:
		return "remove";
	case EVENT_ACTION_URL:
		return "url";
	case EVENT_ACTION_DHCP:
		return "dhcp";
	case EVENT_ACTION_BOOT:
		return "boot";
	case EVENT_ACTION_SYNC:
		return "sync";
	case EVENT_ACTION_PLUGIN:
		return "plugin";
	default:
		break;
	}

	return "unknown";
}

static void user_event_print_event(struct event __attribute__((unused)) *event)
{
	int i;

	pb_debug("user_event %s event:\n", event_action_name(event->action));
	pb_debug("\tdevice: %s\n", event->device);

	for (i = 0; i < event->n_params; i++)
		pb_debug("\t%-12s => %s\n",
			event->params[i].name, event->params[i].value);
}

static struct resource *user_event_resource(struct discover_boot_option *opt,
		struct event *event, bool gen_boot_args_sigfile)
{
	const char *siaddr, *boot_file;
	struct resource *res;
	struct pb_url *url;
	char *url_str;

	siaddr = event_get_param(event, "siaddr");
	if (!siaddr) {
		pb_log("%s: next server option not found\n", __func__);
		return NULL;
	}

	boot_file = event_get_param(event, "bootfile");
	if (!boot_file) {
		pb_log("%s: bootfile not found\n", __func__);
		return NULL;
	}

	if (gen_boot_args_sigfile) {
		char* args_sigfile_default = talloc_asprintf(opt,
			"%s.cmdline.sig", boot_file);
		url_str = talloc_asprintf(opt, "%s%s/%s", "tftp://", siaddr,
			args_sigfile_default);
		talloc_free(args_sigfile_default);
	}
	else
		url_str = talloc_asprintf(opt, "%s%s/%s", "tftp://", siaddr,
			boot_file);
	url = pb_url_parse(opt, url_str);
	talloc_free(url_str);

	if (!url)
		return NULL;

	res = create_url_resource(opt, url);
	if (!res) {
		talloc_free(url);
		return NULL;
	}

	return res;
}

static int parse_user_event(struct discover_context *ctx, struct event *event)
{
	struct discover_boot_option *d_opt;
	char *server_ip, *root_dir, *p;
	struct boot_option *opt;
	struct device *dev;
	const char *val;

	dev = ctx->device->device;

	d_opt = discover_boot_option_create(ctx, ctx->device);
	if (!d_opt)
		goto fail;

	opt = d_opt->option;

	val = event_get_param(event, "name");

	if (!val) {
		pb_log("%s: no name found\n", __func__);
		goto fail_opt;
	}

	opt->id = talloc_asprintf(opt, "%s#%s", dev->id, val);
	opt->name = talloc_strdup(opt, val);

	d_opt->boot_image = user_event_resource(d_opt, event, false);
	if (!d_opt->boot_image) {
		pb_log("%s: no boot image found for %s!\n", __func__,
				opt->name);
		goto fail_opt;
	}
	d_opt->args_sig_file = user_event_resource(d_opt, event, true);

	val = event_get_param(event, "rootpath");
	if (val) {
		server_ip = talloc_strdup(opt, val);
		p = strchr(server_ip, ':');
		if (p) {
			root_dir = talloc_strdup(opt, p + 1);
			*p = '\0';
			opt->boot_args = talloc_asprintf(opt, "%s%s:%s",
					"root=/dev/nfs ip=any nfsroot=",
					server_ip, root_dir);

			talloc_free(root_dir);
		} else {
			opt->boot_args = talloc_asprintf(opt, "%s",
					"root=/dev/nfs ip=any nfsroot=");
		}

		talloc_free(server_ip);
	}

	opt->description = talloc_asprintf(opt, "%s %s", opt->boot_image_file,
		opt->boot_args ? : "");

	if (event_get_param(event, "default"))
		opt->is_default = true;

	discover_context_add_boot_option(ctx, d_opt);

	return 0;

fail_opt:
	talloc_free(d_opt);
fail:
	return -1;
}

static const char *parse_host_addr(struct event *event)
{
	const char *val;

	val = event_get_param(event, "tftp");
	if (val)
		return val;

	val = event_get_param(event, "siaddr");
	if (val)
		return val;

	val = event_get_param(event, "serverid");
	if (val)
		return val;

	return NULL;
}

static char *parse_mac_addr(struct discover_context *ctx, const char *mac)
{
	unsigned int mac_addr_arr[MAC_ADDR_SIZE];
	char *mac_addr;

	sscanf(mac, "%X:%X:%X:%X:%X:%X", mac_addr_arr, mac_addr_arr + 1,
			mac_addr_arr + 2, mac_addr_arr + 3, mac_addr_arr + 4,
			mac_addr_arr + 5);

	mac_addr = talloc_asprintf(ctx, "01-%02x-%02x-%02x-%02x-%02x-%02x",
			mac_addr_arr[0], mac_addr_arr[1], mac_addr_arr[2],
			mac_addr_arr[3], mac_addr_arr[4], mac_addr_arr[5]);

	return mac_addr;
}

static char *parse_ip_addr(struct discover_context *ctx, const char *ip)
{
	unsigned int ip_addr_arr[IP_ADDR_SIZE];
	char *ip_hex;

	sscanf(ip, "%u.%u.%u.%u", ip_addr_arr, ip_addr_arr + 1,
			ip_addr_arr + 2, ip_addr_arr + 3);

	ip_hex = talloc_asprintf(ctx, "%02X%02X%02X%02X", ip_addr_arr[0],
			ip_addr_arr[1], ip_addr_arr[2], ip_addr_arr[3]);

	return ip_hex;
}

struct pb_url *user_event_parse_conf_url(struct discover_context *ctx,
		struct event *event, bool *is_complete)
{
	const char *conffile, *pathprefix, *host, *bootfile;
	char *p, *basedir, *url_str;
	struct pb_url *url;

	conffile = event_get_param(event, "pxeconffile");
	pathprefix = event_get_param(event, "pxepathprefix");
	bootfile = event_get_param(event, "bootfile");

	/* If we're given a conf file, we're able to generate a complete URL to
	 * the configuration file, and the parser doesn't need to do any
	 * further autodiscovery */
	*is_complete = !!conffile;

	/* if conffile is a URL, that's all we need */
	if (conffile && is_url(conffile)) {
		url = pb_url_parse(ctx, conffile);
		return url;
	}

	/* If we can create a URL from pathprefix (optionally with
	 * conffile appended to create a complete URL), use that */
	if (pathprefix && is_url(pathprefix)) {
		if (conffile) {
			url_str = talloc_asprintf(ctx, "%s%s",
					pathprefix, conffile);
			url = pb_url_parse(ctx, url_str);
			talloc_free(url_str);
		} else {
			url = pb_url_parse(ctx, pathprefix);
		}

		return url;
	}

	host = parse_host_addr(event);
	if (!host) {
		pb_log("%s: host address not found\n", __func__);
		return NULL;
	}

	url_str = talloc_asprintf(ctx, "tftp://%s/", host);

	/* if we have a pathprefix, use that directly.. */
	if (pathprefix) {
		/* strip leading slashes */
		while (pathprefix[0] == '/')
			pathprefix++;
		url_str = talloc_asprintf_append(url_str, "%s", pathprefix);

	/* ... otherwise, add a path based on the bootfile name, but only
	 * if conffile isn't an absolute path itself */
	} else if (bootfile && !(conffile && conffile[0] == '/')) {

		basedir = talloc_strdup(ctx, bootfile);

		/* strip filename from the bootfile path, leaving only a
		 * directory */
		p = strrchr(basedir, '/');
		if (!p)
			p = basedir;
		*p = '\0';

		if (strlen(basedir))
			url_str = talloc_asprintf_append(url_str, "%s/",
					basedir);

		talloc_free(basedir);
	}

	/* finally, append conffile */
	if (conffile)
		url_str = talloc_asprintf_append(url_str, "%s", conffile);

	url = pb_url_parse(ctx, url_str);

	talloc_free(url_str);

	return url;
}

char **user_event_parse_conf_filenames(
		struct discover_context *ctx, struct event *event)
{
	char *mac_addr, *ip_hex;
	const char *mac, *ip;
	char **filenames;
	int index, len;

	mac = event_get_param(event, "mac");
	if (mac)
		mac_addr = parse_mac_addr(ctx, mac);
	else
		mac_addr = NULL;

	ip = event_get_param(event, "ip");
	if (ip) {
		ip_hex = parse_ip_addr(ctx, ip);
		len = strlen(ip_hex);
	} else {
		ip_hex = NULL;
		len = 0;
	}

	if (!mac_addr && !ip_hex) {
		pb_log("%s: neither mac nor ip parameter found\n", __func__);
		return NULL;
	}

	/* Filenames as fallback IP's + mac + default */
	filenames = talloc_array(ctx, char *, len + 3);

	index = 0;
	if (mac_addr)
		filenames[index++] = talloc_strdup(filenames, mac_addr);

	while (len) {
		filenames[index++] = talloc_strdup(filenames, ip_hex);
		ip_hex[--len] = '\0';
	}

	filenames[index++] = talloc_strdup(filenames, "default");
	filenames[index++] = NULL;

	if (mac_addr)
		talloc_free(mac_addr);

	if (ip_hex)
		talloc_free(ip_hex);

	return filenames;
}

static int user_event_dhcp(struct user_event *uev, struct event *event)
{
	struct device_handler *handler = uev->handler;
	struct discover_device *dev;

	uint8_t hwaddr[MAC_ADDR_SIZE];

	sscanf(event_get_param(event, "mac"),
	       "%hhX:%hhX:%hhX:%hhX:%hhX:%hhX",
	       hwaddr, hwaddr + 1, hwaddr + 2,
	       hwaddr + 3, hwaddr + 4, hwaddr + 5);

	system_info_set_interface_address(sizeof(hwaddr), hwaddr,
					  event_get_param(event, "ip"));

	dev = discover_device_create(handler, event_get_param(event, "mac"),
					event->device);

	device_handler_dhcp(handler, dev, event);

	return 0;
}

static int user_event_add(struct user_event *uev, struct event *event)
{
	struct device_handler *handler = uev->handler;
	struct discover_context *ctx;
	struct discover_device *dev;

	/* In case this is a network interface, try to refer to it by UUID */
	dev = discover_device_create(handler, event_get_param(event, "mac"),
					event->device);
	dev->device->id = talloc_strdup(dev, event->device);
	ctx = device_handler_discover_context_create(handler, dev);

	parse_user_event(ctx, event);

	device_handler_discover_context_commit(handler, ctx);

	talloc_free(ctx);

	return 0;
}

static int user_event_remove(struct user_event *uev, struct event *event)
{
	struct device_handler *handler = uev->handler;
	struct discover_device *dev;
	const char *mac = event_get_param(event, "mac");

	if (mac)
		dev = device_lookup_by_uuid(handler, event_get_param(event, "mac"));
	else
		dev = device_lookup_by_id(handler, event->device);

	if (!dev)
		return 0;

	device_handler_remove(handler, dev);

	return 0;
}

static int user_event_url(struct user_event *uev, struct event *event)
{
	struct device_handler *handler = uev->handler;
	const char *url;

	url = event_get_param(event, "url");
	if (url)
		device_handler_process_url(handler, url, NULL, NULL);

	return 0;
}

static int user_event_boot(struct user_event *uev, struct event *event)
{
	struct device_handler *handler = uev->handler;
	struct boot_command *cmd = talloc(handler, struct boot_command);

	cmd->option_id = talloc_strdup(cmd, event_get_param(event, "id"));
	cmd->boot_image_file = talloc_strdup(cmd, event_get_param(event, "image"));
	cmd->initrd_file = talloc_strdup(cmd, event_get_param(event, "initrd"));
	cmd->dtb_file = talloc_strdup(cmd, event_get_param(event, "dtb"));
	cmd->boot_args = talloc_strdup(cmd, event_get_param(event, "args"));

	device_handler_boot(handler, cmd);

	talloc_free(cmd);

	return 0;
}

static int user_event_sync(struct user_event *uev, struct event *event)
{
	struct device_handler *handler = uev->handler;

	if (strncasecmp(event->device, "all", strlen("all")) != 0)
		device_sync_snapshots(handler, event->device);
	else
		device_sync_snapshots(handler, NULL);

	return 0;
}

static int process_uninstalled_plugin(struct user_event *uev,
		struct event *event)
{
	struct device_handler *handler = uev->handler;
	struct discover_boot_option *file_opt;
	struct discover_device *device;
	struct discover_context *ctx;
	const char *path;
	struct resource *res;

	if (!event_get_param(event, "path")) {
		pb_log("Uninstalled pb-plugin event missing path param\n");
		return -1;
	}

	device = device_lookup_by_name(handler, event->device);
	if (!device) {
		pb_log("Couldn't find device matching %s for plugin\n",
				event->device);
		return -1;
	}

	ctx = device_handler_discover_context_create(handler, device);
	file_opt = discover_boot_option_create(ctx, device);
	file_opt->option->name = talloc_strdup(file_opt,
			event_get_param(event, "name"));
	file_opt->option->id = talloc_asprintf(file_opt, "%s@%p",
			device->device->id, file_opt);
	file_opt->option->type = DISCOVER_PLUGIN_OPTION;


	path = event_get_param(event, "path");
	/* path may be relative to root */
	if (strncmp(device->mount_path, path, strlen(device->mount_path)) == 0) {
		path += strlen(device->mount_path) + 1;
	}

	res = talloc(file_opt, struct resource);
	resolve_resource_against_device(res, device, path);
	file_opt->boot_image = res;

	discover_context_add_boot_option(ctx, file_opt);
	device_handler_discover_context_commit(handler, ctx);

	return 0;
}

/*
 * Notification of a plugin event. This can either be for an uninstalled plugin
 * that pb-plugin has scanned, or the result of a plugin that pb-plugin has
 * installed.
 */
static int user_event_plugin(struct user_event *uev, struct event *event)
{
	struct device_handler *handler = uev->handler;
	char *executable, *executables, *saveptr;
	struct plugin_option *opt;
	const char *installed;

	installed = event_get_param(event, "installed");
	if (!installed || strncmp(installed, "no", strlen("no")) == 0)
		return process_uninstalled_plugin(uev, event);

	opt = talloc_zero(handler, struct plugin_option);
	if (!opt)
		return -1;
	opt->name = talloc_strdup(opt, event_get_param(event, "name"));
	opt->id = talloc_strdup(opt, event_get_param(event, "id"));
	opt->version = talloc_strdup(opt, event_get_param(event, "version"));
	opt->vendor = talloc_strdup(opt, event_get_param(event, "vendor"));
	opt->vendor_id = talloc_strdup(opt, event_get_param(event, "vendor_id"));
	opt->date = talloc_strdup(opt, event_get_param(event, "date"));
	opt->plugin_file = talloc_strdup(opt,
			event_get_param(event, "source_file"));

	executables = talloc_strdup(opt, event_get_param(event, "executables"));
	if (!executables) {
		talloc_free(opt);
		return -1;
	}

	/*
	 * The 'executables' parameter is a space-delimited list of installed
	 * executables
	 */
	executable = strtok_r(executables, " ", &saveptr);
	while (executable) {
		opt->executables = talloc_realloc(opt, opt->executables,
						  char *, opt->n_executables + 1);
		if (!opt->executables) {
			talloc_free(opt);
			return -1;
		}
		opt->executables[opt->n_executables++] = talloc_strdup(opt,
								executable);
		executable = strtok_r(NULL, " ", &saveptr);
	}

	device_handler_add_plugin_option(handler, opt);

	talloc_free(executables);

	return 0;
}

static void user_event_handle_message(struct user_event *uev, char *buf,
	int len)
{
	int result;
	struct event *event;

	event = talloc(uev, struct event);
	event->type = EVENT_TYPE_USER;

	result = event_parse_ad_message(event, buf, len);

	if (result)
		return;

	user_event_print_event(event);

	switch (event->action) {
	case EVENT_ACTION_ADD:
		result = user_event_add(uev, event);
		break;
	case EVENT_ACTION_REMOVE:
		result = user_event_remove(uev, event);
		break;
	case EVENT_ACTION_URL:
		result = user_event_url(uev, event);
		goto out;
	case EVENT_ACTION_DHCP:
		result = user_event_dhcp(uev, event);
		goto out;
	case EVENT_ACTION_BOOT:
		result = user_event_boot(uev, event);
		break;
	case EVENT_ACTION_SYNC:
		result = user_event_sync(uev, event);
		break;
	case EVENT_ACTION_PLUGIN:
		result = user_event_plugin(uev, event);
		break;
	default:
		break;
	}

	/* user_event_url() and user_event_dhcp() will steal the event context,
	 * but all others still need to free */
	talloc_free(event);
out:
	return;
}

static int user_event_process(void *arg)
{
	struct user_event *uev = arg;
	char buf[PBOOT_USER_EVENT_SIZE + 1];
	int len;

	len = recvfrom(uev->socket, buf, PBOOT_USER_EVENT_SIZE, 0, NULL, NULL);

	if (len < 0) {
		pb_log("%s: socket read failed: %s", __func__, strerror(errno));
		return 0;
	}

	if (len == 0) {
		pb_log("%s: empty", __func__);
		return 0;
	}

	buf[len] = '\0';

	pb_debug("%s: %u bytes\n", __func__, len);

	user_event_handle_message(uev, buf, len);

	return 0;
}

static int user_event_destructor(void *arg)
{
	struct user_event *uev = arg;

	pb_debug("%s\n", __func__);

	if (uev->socket >= 0)
		close(uev->socket);

	return 0;
}

struct user_event *user_event_init(struct device_handler *handler,
		struct waitset *waitset)
{
	struct sockaddr_un addr;
	struct user_event *uev;

	unlink(PBOOT_USER_EVENT_SOCKET);

	uev = talloc(handler, struct user_event);

	uev->handler = handler;

	uev->socket = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (uev->socket < 0) {
		pb_log("%s: Error creating event handler socket: %s\n",
			__func__, strerror(errno));
		goto out_err;
	}

	talloc_set_destructor(uev, user_event_destructor);

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, PBOOT_USER_EVENT_SOCKET);

	if (bind(uev->socket, (struct sockaddr *)&addr, sizeof(addr))) {
		pb_log("Error binding event handler socket: %s\n",
			strerror(errno));
	}

	waiter_register_io(waitset, uev->socket, WAIT_IN,
			user_event_process, uev);

	pb_debug("%s: waiting on %s\n", __func__, PBOOT_USER_EVENT_SOCKET);

	return uev;

out_err:
	talloc_free(uev);
	return NULL;
}
