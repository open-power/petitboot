
#include <assert.h>
#include <string.h>
#include <i18n/i18n.h>

#include <talloc/talloc.h>
#include <url/url.h>

#include <discover/resource.h>
#include <discover/parser.h>
#include <discover/parser-utils.h>

#include "grub2.h"

static const char *const grub2_conf_files[] = {
	"/grub.cfg",
	"/menu.lst",
	"/grub/grub.cfg",
	"/grub2/grub.cfg",
	"/grub/menu.lst",
	"/boot/grub/grub.cfg",
	"/boot/grub2/grub.cfg",
	"/boot/grub/menu.lst",
	"/efi/boot/grub.cfg",
	"/GRUB.CFG",
	"/MENU.LST",
	"/GRUB/GRUB.CFG",
	"/GRUB2/GRUB.CFG",
	"/GRUB/MENU.LST",
	"/BOOT/GRUB/GRUB.CFG",
	"/BOOT/GRUB/MENU.LST",
	"/EFI/BOOT/GRUB.CFG",
	NULL
};

static struct discover_device *grub2_lookup_device(
		struct device_handler *handler, const char *desc)
{
	struct discover_device *dev;

	if (!desc || !*desc)
		return NULL;

	dev = device_lookup_by_id(handler, desc);
	if (dev)
		return dev;

	/* for now, only lookup by UUID */
	dev = device_lookup_by_uuid(handler, desc);
	if (dev)
		return dev;

	return NULL;
}

/* we use slightly different resources for grub2 */
struct resource *create_grub2_resource(struct grub2_script *script,
		struct discover_boot_option *opt,
		const char *path)
{
	struct discover_device *dev;
	struct grub2_file *file;
	struct resource *res;
	const char *root;

	if (strstr(path, "://")) {
		struct pb_url *url = pb_url_parse(opt, path);
		if (url)
			return create_url_resource(opt, url);
	}

	file = grub2_parse_file(script, path);
	if (!file)
		return NULL;

	res = talloc(opt, struct resource);
	root = script_env_get(script, "root");

	if (!file->dev && root && strlen(root))
		file->dev = talloc_strdup(file, root);

	/* if we don't have a device specified, or the lookup succeeds now,
	 * then we can resolve the resource right away */
	if (file->dev)
		dev = grub2_lookup_device(script->ctx->handler, file->dev);
	else
		dev = script->ctx->device;

	if (dev) {
		resolve_resource_against_device(res, dev, file->path);
	} else {
		res->resolved = false;
		res->info = talloc_steal(opt, file);
	}

	return res;
}

bool resolve_grub2_resource(struct device_handler *handler,
		struct resource *res)
{
	struct grub2_file *file = res->info;
	struct discover_device *dev;

	assert(!res->resolved);

	dev = grub2_lookup_device(handler, file->dev);
	if (!dev)
		return false;

	resolve_resource_against_device(res, dev, file->path);
	talloc_free(file);

	return true;
}

struct grub2_file *grub2_parse_file(struct grub2_script *script,
		const char *str)
{
	struct grub2_file *file;
	size_t dev_len;
	char *pos;

	if (!str)
		return NULL;

	file = talloc_zero(script, struct grub2_file);

	if (*str != '(') {
		/* just a path - no device, return path as-is */
		file->path = talloc_strdup(file, str);

	} else {
		/* device plus path - split into components */

		pos = strchr(str, ')');

		/* no closing bracket, or zero-length path? */
		if (!pos || *(pos+1) == '\0') {
			talloc_free(file);
			return NULL;
		}

		file->path = talloc_strdup(file, pos + 1);

		dev_len = pos - str - 1;
		if (dev_len)
			file->dev = talloc_strndup(file, str + 1, dev_len);
	}

	return file;
}


static int grub2_parse(struct discover_context *dc)
{
	const char * const *filename;
	struct grub2_parser *parser;
	int len, rc;
	char *buf;

	/* Support block device boot only at present */
	if (dc->event)
		return -1;

	for (filename = grub2_conf_files; *filename; filename++) {
		rc = parser_request_file(dc, dc->device, *filename, &buf, &len);
		if (rc)
			continue;

		parser = grub2_parser_create(dc);
		grub2_parser_parse(parser, *filename, buf, len);
		device_handler_status_dev_info(dc->handler, dc->device,
				_("Parsed GRUB configuration from %s"),
				*filename);
		talloc_free(buf);
		talloc_free(parser);
		break;
	}


	return 0;
}

static struct parser grub2_parser = {
	.name			= "grub2",
	.parse			= grub2_parse,
	.resolve_resource	= resolve_grub2_resource,
};

register_parser(grub2_parser);
