#define _GNU_SOURCE

#include <assert.h>

#include "log/log.h"
#include "talloc/talloc.h"
#include "url/url.h"

#include "resource.h"
#include "event.h"
#include "parser-utils.h"
#include "device-handler.h"

static struct resource *user_event_resource(struct discover_boot_option *opt,
		struct event *event, const char *param_name)
{
	struct resource *res;
	struct pb_url *url;
	const char *val;

	val = event_get_param(event, param_name);
	if (!val)
		return NULL;

	url = pb_url_parse(opt, val);
	if (!url)
		return NULL;

	res = create_url_resource(opt, url);
	if (!res) {
		talloc_free(url);
		return NULL;
	}

	return res;
}

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
	opt->name = talloc_strdup(opt, p);

	d_opt->boot_image = user_event_resource(d_opt, event, "image");
	if (!d_opt->boot_image) {
		pb_log("%s: no boot image found for %s!\n", __func__,
				opt->name);
		goto fail;
	}

	d_opt->initrd = user_event_resource(d_opt, event, "initrd");

	p = event_get_param(event, "args");

	if (p)
		opt->boot_args = talloc_strdup(opt, p);

	opt->description = talloc_asprintf(opt, "%s %s", opt->boot_image_file,
		opt->boot_args ? : "");

	if (event_get_param(event, "default"))
		opt->is_default = true;

	discover_context_add_boot_option(ctx, d_opt);

	return 0;

fail:
	talloc_free(d_opt);
	return -1;
}
