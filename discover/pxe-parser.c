
#define _GNU_SOURCE
#include <string.h>

#include <talloc/talloc.h>
#include <url/url.h>

#include "parser.h"
#include "parser-conf.h"
#include "parser-utils.h"
#include "resource.h"
#include "paths.h"
#include "user-event.h"

struct pxe_parser_info {
	struct discover_boot_option *opt;
	const char *default_name;
};

static void pxe_finish(struct conf_context *conf)
{
	struct pxe_parser_info *info = conf->parser_info;
	if (info->opt)
		discover_context_add_boot_option(conf->dc, info->opt);
}

static void pxe_process_pair(struct conf_context *ctx,
		const char *name, char *value)
{
	struct pxe_parser_info *parser_info = ctx->parser_info;
	struct discover_boot_option *opt = parser_info->opt;
	struct pb_url *url;

	/* quirk in the syslinux config format: initrd can be separated
	 * by an '=' */
	if (!name && !strncasecmp(value, "initrd=", strlen("initrd="))) {
		name = "initrd";
		value += strlen("initrd=");
	}

	if (!name)
		return;

	if (streq(name, "DEFAULT")) {
		parser_info->default_name = talloc_strdup(parser_info, value);
		return;
	}

	if (streq(name, "LABEL")) {
		if (opt)
			pxe_finish(ctx);

		opt = discover_boot_option_create(ctx->dc, ctx->dc->device);

		opt->option->name = talloc_strdup(opt, value);
		opt->option->id = talloc_asprintf(opt, "%s@%p",
				ctx->dc->device->device->id, opt);

		opt->option->is_default = parser_info->default_name &&
					streq(parser_info->default_name, value);

		parser_info->opt = opt;
		return;
	}

	/* all other parameters need an option */
	if (!opt)
		return;

	if (streq(name, "KERNEL")) {
		url = pb_url_join(ctx->dc, ctx->dc->conf_url, value);
		opt->boot_image = create_url_resource(opt, url);

	} else if (streq(name, "INITRD")) {
		url = pb_url_join(ctx->dc, ctx->dc->conf_url, value);
		opt->initrd = create_url_resource(opt, url);

	} else if (streq(name, "APPEND")) {
		char *str, *end;

		opt->option->boot_args = talloc_strdup(opt->option, value);

		str = strcasestr(value, "INITRD=");
		if (str) {
			str += strlen("INITRD=");
			end = strchrnul(str, ' ');
			*end = '\0';

			url = pb_url_join(ctx->dc, ctx->dc->conf_url, str);
			opt->initrd = create_url_resource(opt, url);
		}
	}

}

static int pxe_parse(struct discover_context *dc)
{
	struct pxe_parser_info *parser_info;
	char **pxe_conf_files, **filename;
	struct pb_url *conf_url, *url;
	struct conf_context *conf;
	int len, rc;
	char *buf;

	/* Expects dhcp event parameters to support network boot */
	if (!dc->event)
		return -1;

	conf = talloc_zero(dc, struct conf_context);

	if (!conf)
		goto out;

	conf->dc = dc;
	conf->get_pair = conf_get_pair_space;
	conf->process_pair = pxe_process_pair;
	conf->finish = pxe_finish;

	parser_info = talloc_zero(conf, struct pxe_parser_info);
	conf->parser_info = parser_info;

	conf_url = user_event_parse_conf_url(dc, dc->event);
	if (!conf_url)
		goto out_conf;

	if (dc->conf_url) {
		rc = parser_request_url(dc, dc->conf_url, &buf, &len);
		if (rc)
			goto out_conf;
	} else {
		pxe_conf_files = user_event_parse_conf_filenames(dc, dc->event);
		if (!pxe_conf_files)
			goto out_conf;

		rc = -1;

		for (filename = pxe_conf_files; *filename; filename++) {
			url = pb_url_join(dc, conf_url, *filename);
			if (!url)
				goto out_pxe_conf;

			rc = parser_request_url(dc, url, &buf, &len);
			if (!rc) /* found one, just break */
				break;

			talloc_free(url);
		}

		/* No configuration file found on the boot server */
		if (rc)
			goto out_pxe_conf;

		dc->conf_url = url;

		talloc_free(conf_url);
		talloc_free(pxe_conf_files);
	}

	/* Call the config file parser with the data read from the file */
	conf_parse_buf(conf, buf, len);

	talloc_free(buf);
	talloc_free(conf);

	return 0;

out_pxe_conf:
	talloc_free(pxe_conf_files);
out_conf:
	talloc_free(conf);
out:
	return -1;
}

static struct parser pxe_parser = {
	.name			= "pxe",
	.parse			= pxe_parse,
};

register_parser(pxe_parser);
