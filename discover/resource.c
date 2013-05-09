
#define _GNU_SOURCE

#include <assert.h>
#include <stdbool.h>
#include <string.h>

#include <url/url.h>
#include <log/log.h>
#include <talloc/talloc.h>

#include "device-handler.h"
#include "resource.h"
#include "paths.h"

static int is_prefix_ignorecase(const char *str, const char *prefix)
{
	return !strncasecmp(str, prefix, strlen(prefix));
}

struct devpath_resource_info {
	char	*dev, *path;
};

static struct discover_device *parse_device_string(
		struct device_handler *handler, const char *devstr)
{
	if (is_prefix_ignorecase(devstr, "uuid="))
		return device_lookup_by_uuid(handler, devstr + strlen("uuid"));

	if (is_prefix_ignorecase(devstr, "label="))
		return device_lookup_by_label(handler,
					devstr + strlen("label="));

	return device_lookup_by_name(handler, devstr);
}
static void resolve_devpath_against_device(struct resource *res,
	struct discover_device *dev, const char *path)
{
	char *resolved_path = join_paths(res, dev->mount_path, path);
	res->url = pb_url_parse(res, resolved_path);
	res->resolved = true;
}

struct resource *create_devpath_resource(struct discover_boot_option *opt,
	struct discover_device *orig_device,
	const char *devpath)
{
	struct devpath_resource_info *info;
	char *pos, *devstr, *path;
	struct resource *res;
	struct pb_url *url;

	res = talloc(opt, struct resource);

	pos = strchr(devpath, ':');

	/* do we have a "://" scheme separator? */
	if (pos && pos[1] && pos[1] == '/' && pos[2] && pos[2] == '/') {
		url = pb_url_parse(res, devpath);

		if (url->scheme != pb_url_file) {
			/* not a file? we're ready to go */
			res->resolved = true;
			res->url = url;
		} else {
			/* we've been passed a file:// URL, which has no device
			 * specifier. We can resolve against the original
			 * device */
			resolve_devpath_against_device(res, orig_device,
					url->path);
			talloc_free(url);
		}
		return res;
	}

	/* if there was no device specified, we can resolve now */
	if (!pos) {
		resolve_devpath_against_device(res, orig_device, devpath);
		return res;
	}

	devstr = talloc_strndup(res, devpath, pos - devpath);
	path = talloc_strdup(res, pos + 1);

	pb_log("%s: resource depends on device %s\n", __func__, devstr);

	/* defer resolution until we can find a suitable matching device */
	info = talloc(res, struct devpath_resource_info);
	info->dev = devstr;
	info->path = path;

	res->resolved = false;
	res->info = info;

	return res;
}

bool resolve_devpath_resource(struct device_handler *handler,
		struct resource *res)
{
	struct devpath_resource_info *info = res->info;
	struct discover_device *dev;

	assert(!res->resolved);

	dev = parse_device_string(handler, info->dev);

	if (!dev)
		return false;

	resolve_devpath_against_device(res, dev, info->path);
	talloc_free(info);

	return true;
}

struct resource *create_url_resource(struct discover_boot_option *opt,
		struct pb_url *url)
{
	struct resource *res;

	res = talloc(opt, struct resource);
	res->url = url;
	res->resolved = true;

	return res;
}
