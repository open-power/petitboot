
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdlib.h>
#include <string.h>

#include <talloc/talloc.h>
#include <url/url.h>
#include <log/log.h>

#include "parser.h"
#include "parser-conf.h"
#include "parser-utils.h"
#include "resource.h"
#include "paths.h"
#include "event.h"
#include "user-event.h"
#include "network.h"

static const char *pxelinux_prefix = "pxelinux.cfg/";

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

/* We need a slightly modified version of pb_url_join, to allow for the
 * pxelinux "::filename" syntax for absolute URLs
 */
static struct pb_url *pxe_url_join(void *ctx, const struct pb_url *url,
		const char *s)
{
	struct pb_url *new_url;
	int len;

	len = strlen(s);

	if (len > 2 && s[0] == ':' && s[1] == ':') {
		char *tmp;

		if (s[2] == '/') {
			/* ::/path -> /path */
			tmp = talloc_strdup(ctx, s+2);
		} else {
			/* ::path -> /path */
			tmp = talloc_strdup(ctx, s+1);
			tmp[0] = '/';
		}

		new_url = pb_url_join(ctx, url, tmp);

		talloc_free(tmp);

	} else {
		const char *tmp;
		/* strip leading slashes */
		for (tmp = s; *tmp == '/'; tmp++)
			;
		new_url = pb_url_join(ctx, url, tmp);
	}

	return new_url;
}

static void pxe_append_string(struct discover_boot_option *opt,
		const char *str)
{
	if (opt->option->boot_args)
		opt->option->boot_args = talloc_asprintf_append(
				opt->option->boot_args, " %s", str);
	else
		opt->option->boot_args = talloc_strdup(opt->option, str);
}

static char *pxe_sysappend_mac(void *ctx, uint8_t *mac)
{
	if (!mac)
		return NULL;

	return talloc_asprintf(ctx,
		"BOOTIF=01-%02x-%02x-%02x-%02x-%02x-%02x",
		mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static void pxe_process_sysappend(struct discover_context *ctx,
		struct discover_boot_option *opt,
		unsigned long val)
{
	struct event *event = ctx->event;
	char *str = NULL;

	if (!event)
		return;

	if (val & 0x2) {
		uint8_t *mac = find_mac_by_name(ctx, ctx->network,
					event->device);
		str = pxe_sysappend_mac(ctx, mac);
		if (str) {
			pxe_append_string(opt, str);
			talloc_free(str);
		}
		val &= ~0x2;
	}

	if (val)
		pb_log("pxe: unsupported features requested in "
				"ipappend/sysappend: 0x%04lx", val);

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
		url = pxe_url_join(ctx->dc, ctx->dc->conf_url, value);
		opt->boot_image = create_url_resource(opt, url);

	} else if (streq(name, "INITRD")) {
		url = pxe_url_join(ctx->dc, ctx->dc->conf_url, value);
		opt->initrd = create_url_resource(opt, url);

	} else if (streq(name, "APPEND")) {
		char *str, *end;

		pxe_append_string(opt, value);

		str = strcasestr(value, "INITRD=");
		if (str) {
			str += strlen("INITRD=");
			end = strchrnul(str, ' ');
			*end = '\0';

			url = pxe_url_join(ctx->dc, ctx->dc->conf_url, str);
			opt->initrd = create_url_resource(opt, url);
		}
	} else if (streq(name, "SYSAPPEND") || streq(name, "IPAPPEND")) {
		unsigned long type;
		char *end;

		type = strtoul(value, &end, 10);
		if (end != value && !(*end))
			pxe_process_sysappend(ctx->dc, opt, type);
	}

}

static int pxe_parse(struct discover_context *dc)
{
	struct pb_url *pxe_base_url, *url;
	struct pxe_parser_info *parser_info;
	char **pxe_conf_files, **filename;
	struct conf_context *conf;
	bool complete_url;
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

	dc->conf_url = user_event_parse_conf_url(dc, dc->event, &complete_url);
	if (!dc->conf_url)
		goto out_conf;

	if (complete_url) {
		/* we have a complete URL; use this and we're done. */
		rc = parser_request_url(dc, dc->conf_url, &buf, &len);
		if (rc)
			goto out_conf;
	} else {
		pxe_conf_files = user_event_parse_conf_filenames(dc, dc->event);
		if (!pxe_conf_files)
			goto out_conf;

		rc = -1;

		pxe_base_url = pb_url_join(dc, dc->conf_url, pxelinux_prefix);
		if (!pxe_base_url)
			goto out_pxe_conf;

		for (filename = pxe_conf_files; *filename; filename++) {
			url = pb_url_join(dc, pxe_base_url, *filename);
			if (!url)
				continue;

			rc = parser_request_url(dc, url, &buf, &len);
			if (!rc) /* found one, just break */
				break;

			talloc_free(url);
		}

		talloc_free(pxe_base_url);

		/* No configuration file found on the boot server */
		if (rc)
			goto out_pxe_conf;

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
