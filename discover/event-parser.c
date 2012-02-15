#define _GNU_SOURCE

#include <assert.h>

#include "log/log.h"
#include "talloc/talloc.h"
#include "event.h"
#include "parser-utils.h"

/**
 * parse_user_event - Parse a user event.
 *
 * Understands params: name, image, args.
 */

int parse_user_event(struct device *device, struct event *event)
{
	struct boot_option *opt;
	const char *p;

	opt = talloc_zero(device, struct boot_option);

	if (!opt)
		goto fail;

	p = event_get_param(event, "name");

	if (!p) {
		pb_log("%s: no name found\n", __func__);
		goto fail;
	}

	opt->id = talloc_asprintf(opt, "%s#%s", device->id, p);
	opt->name = talloc_strdup(opt, p);

	p = event_get_param(event, "image");
	assert(p);

	if (!p) {
		pb_log("%s: no image found\n", __func__);
		goto fail;
	}

	opt->boot_image_file = talloc_strdup(opt, p);

	p = event_get_param(event, "args");
	assert(p);

	opt->boot_args = talloc_strdup(opt, p);

	opt->description = talloc_asprintf(opt, "%s %s", opt->boot_image_file,
		opt->boot_args);

	device_add_boot_option(device, opt);

	return 0;

fail:
	talloc_free(opt);
	return -1;
}
