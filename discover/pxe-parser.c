
#include <string.h>

#include <talloc/talloc.h>
#include <url/url.h>

#include "parser.h"
#include "parser-conf.h"
#include "parser-utils.h"
#include "resource.h"

static void pxe_finish(struct conf_context *conf)
{
	printf("%s\n", __func__);
	discover_context_add_boot_option(conf->dc, conf->parser_info);
}

static void pxe_process_pair(struct conf_context *ctx,
		const char *name, char *value)
{
	struct discover_boot_option *opt = ctx->parser_info;
	struct pb_url *url;

	if (streq(name, "LABEL")) {
		if (opt)
			pxe_finish(ctx);

		opt = discover_boot_option_create(ctx->dc, ctx->dc->device);
		ctx->parser_info = opt;

		opt->option->device_id = ctx->dc->device->device->id;
		opt->option->name = talloc_strdup(opt, value);
		opt->option->id = talloc_asprintf(opt, "%s@%p",
				opt->option->device_id, opt);
		return;
	}

	/* all other parameters need an option */
	if (!opt)
		return;

	if (streq(name, "KERNEL")) {
		url = pb_url_join(ctx->dc, ctx->dc->conf_url, value);
		opt->boot_image = create_url_resource(opt, url);

	} else if (streq(name, "APPEND")) {
		opt->option->boot_args = talloc_strdup(opt->option, value);
		/* todo: initrd extraction */
	}
}

static int pxe_parse(struct discover_context *dc, char *buf, int len)
{
	struct conf_context *conf;

	conf = talloc_zero(dc, struct conf_context);

	if (!conf)
		return 0;

	conf->dc = dc;
	conf->get_pair = conf_get_pair_space;
	conf->process_pair = pxe_process_pair;
	conf->finish = pxe_finish;

	conf_parse_buf(conf, buf, len);

	talloc_free(conf);
	return 1;
}

static struct parser pxe_parser = {
	.name			= "pxe",
	.parse			= pxe_parse,
	.method			= CONF_METHOD_DHCP,
};

register_parser(pxe_parser);
