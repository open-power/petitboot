#include <assert.h>
#include <string.h>
#include <i18n/i18n.h>

#include <talloc/talloc.h>
#include <url/url.h>

#include <discover/resource.h>
#include <discover/parser.h>
#include <discover/parser-utils.h>

#include "native.h"

static const char *const native_conf_files[] = {
	"/boot/petitboot.conf",
	"/petitboot.conf",
	NULL
};

static int native_parse(struct discover_context *dc)
{
	const char * const *filename;
	struct native_parser *parser;
	int len, rc;
	char *buf;

	/* Support block device boot only at present */
	if (dc->event)
		return -1;

	for (filename = native_conf_files; *filename; filename++) {
		rc = parser_request_file(dc, dc->device, *filename, &buf, &len);
		if (rc)
			continue;

		parser = native_parser_create(dc);
		native_parser_parse(parser, *filename, buf, len);
		device_handler_status_dev_info(dc->handler, dc->device,
				_("Parsed native configuration from %s"),
				*filename);
		talloc_free(buf);
		talloc_free(parser);
		break;
	}

	return 0;
}

static struct parser native_parser = {
	.name			= "native",
	.parse			= native_parse,
	.resolve_resource	= resolve_devpath_resource,
};

register_parser(native_parser);
