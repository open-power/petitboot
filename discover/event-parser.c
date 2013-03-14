#define _GNU_SOURCE

#include <assert.h>

#include "log/log.h"
#include "talloc/talloc.h"
#include "event.h"
#include "parser-utils.h"
#include "device-handler.h"

/**
 * parse_user_event - Parse a user event.
 *
 * Understands params: name, image, args.
 */

int parse_user_event(struct discover_context *ctx, struct event *event)
{
	struct discover_boot_option *d_opt;
	struct boot_option *opt;
	struct device *dev;
	const char *p;

	dev = ctx->device->device;

	d_opt = discover_boot_option_create(ctx, ctx->device);
	opt = d_opt->option;

	if (!d_opt)
		goto fail;

	p = event_get_param(event, "name");

	if (!p) {
		pb_log("%s: no name found\n", __func__);
		goto fail;
	}

	opt->id = talloc_asprintf(opt, "%s#%s", dev->id, p);
	opt->device_id = talloc_strdup(opt, dev->id);
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

	discover_context_add_boot_option(ctx, d_opt);

	return 0;

fail:
	talloc_free(d_opt);
	return -1;
}
