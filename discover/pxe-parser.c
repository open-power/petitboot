
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdlib.h>
#include <string.h>

#include <talloc/talloc.h>
#include <url/url.h>
#include <log/log.h>
#include <file/file.h>
#include <i18n/i18n.h>

#include "device-handler.h"
#include "parser.h"
#include "parser-conf.h"
#include "parser-utils.h"
#include "resource.h"
#include "paths.h"
#include "event.h"
#include "user-event.h"
#include "network.h"

static const char *pxelinux_prefix = "pxelinux.cfg/";

static void pxe_conf_parse_cb(struct load_url_result *result, void *data);

struct pxe_parser_info {
	struct discover_boot_option	*opt;
	const char			*default_name;
	char				**pxe_conf_files;
	struct pb_url			*pxe_base_url;
	int				current;
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
		uint8_t *mac = find_mac_by_name(ctx,
				device_handler_get_network(ctx->handler),
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

		char* args_sigfile_default = talloc_asprintf(opt,
			"%s.cmdline.sig", value);
		url = pxe_url_join(ctx->dc, ctx->dc->conf_url,
			args_sigfile_default);
		opt->args_sig_file = create_url_resource(opt, url);
		talloc_free(args_sigfile_default);

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

	} else if (streq(name, "DTB") || streq(name, "FDT")) {
		url = pxe_url_join(ctx->dc, ctx->dc->conf_url, value);
		opt->dtb = create_url_resource(opt, url);
	}

}

static void pxe_load_next_filename(struct conf_context *conf)
{
	struct pxe_parser_info *info = conf->parser_info;
	struct pb_url *url;

	if (!info->pxe_conf_files)
		return;

	for (; info->pxe_conf_files[info->current]; info->current++) {
		url = pb_url_join(conf->dc, info->pxe_base_url,
				  info->pxe_conf_files[info->current]);
		if (!url)
			continue;

		if (load_url_async(conf, url, pxe_conf_parse_cb, conf,
				   NULL, NULL))
			break;
	}

	return;
}

/*
 * Callback for asynchronous loads from pxe_parse()
 * @param result Result of load_url_async()
 * @param data   Pointer to associated conf_context
 */
static void pxe_conf_parse_cb(struct load_url_result *result, void *data)
{
	struct conf_context *conf = data;
	struct device_handler *handler;
	struct pxe_parser_info *info;
	char *buf = NULL;
	int len, rc = 0;

	if (!data)
		return;

	handler = talloc_parent(conf);

	if (result && result->status == LOAD_OK)
		rc = read_file(conf, result->local, &buf, &len);
	if (!result || result->status != LOAD_OK || rc) {
		/* This load failed so try the next available filename */
		info = conf->parser_info;
		if (!info->pxe_conf_files) {
			device_handler_status_dev_err(handler,
					conf->dc->device,
					_("Failed to download %s"),
					pb_url_to_string(result->url));

			return;
		}

		info->current++;
		pxe_load_next_filename(conf);
		if (info->pxe_conf_files[info->current] == NULL) {
			/* Nothing left to try */
			device_handler_status_dev_err(handler,
					conf->dc->device,
					_("PXE autoconfiguration failed"));
			goto out_clean;
		}
		return;
	}

	/*
	 * Parse the first successfully downloaded file. We only want to parse
	 * the first because otherwise we could parse options from both a
	 * machine-specific config and a 'fallback' default config
	 */

	conf_parse_buf(conf, buf, len);

	/* We may be called well after the original caller of iterate_parsers(),
	 * commit any new boot options ourselves */
	device_handler_discover_context_commit(handler, conf->dc);

	/*
	 * TRANSLATORS: the format specifier in this string is a URL
	 * eg. tftp://192.168.1.1/pxelinux.cfg
	 */
	device_handler_status_dev_info(handler, conf->dc->device,
			_("Parsed PXE config from %s"),
			pb_url_to_string(result->url));

	talloc_free(buf);
out_clean:
	if (result->cleanup_local)
		unlink(result->local);
	talloc_free(conf);
}

/**
 * Return a new conf_context and increment the talloc reference count on
 * the discover_context struct.
 * @param  ctx  Parent talloc context
 * @param  orig Original discover_context
 * @return      Pointer to new conf_context
 */
static struct conf_context *copy_context(void *ctx, struct discover_context *dc)
{
	struct pxe_parser_info *info;
	struct conf_context *conf;

	conf = talloc_zero(ctx, struct conf_context);

	if (!conf)
		return NULL;

	conf->get_pair = conf_get_pair_space;
	conf->process_pair = pxe_process_pair;
	conf->finish = pxe_finish;
	info = talloc_zero(conf, struct pxe_parser_info);
	if (!info) {
		talloc_free(conf);
		return NULL;
	}
	conf->parser_info = info;

	/*
	 * The discover_context may be freed once pxe_parse() returns, but the
	 * callback will still need it. Take a reference so that that it will
	 * persist until the last callback completes.
	 */
	conf->dc = talloc_reference(conf, dc);

	return conf;
}

static int pxe_parse(struct discover_context *dc)
{
	struct pb_url *pxe_base_url;
	struct conf_context *conf = NULL;
	struct load_url_result *result;
	void *ctx = talloc_parent(dc);
	struct pxe_parser_info *info;
	char **pxe_conf_files;
	bool complete_url;

	/* Expects dhcp event parameters to support network boot */
	if (!dc->event)
		return -1;

	dc->conf_url = user_event_parse_conf_url(dc, dc->event, &complete_url);
	if (!dc->conf_url)
		return -1;

	/*
	 * Retrieving PXE configs over the network can take some time depending
	 * on factors such as slow network, malformed paths, bad DNS, and
	 * overzealous firewalls. Instead of blocking the discover server while
	 * we wait for these, spawn an asynchronous job that will attempt to
	 * retrieve each possible URL until it successfully finds one, and
	 * parse and process the resulting file in a callback.
	 */
	conf = copy_context(ctx, dc);
	if (!conf)
		return -1;

	if (complete_url) {
		device_handler_status_dev_info(conf->dc->handler,
			dc->device,
			_("Requesting config %s"),
			pb_url_to_string(conf->dc->conf_url));

		/* we have a complete URL; use this and we're done. */
		result = load_url_async(conf->dc, conf->dc->conf_url,
					pxe_conf_parse_cb, conf, NULL, ctx);
		if (!result) {
			pb_log("load_url_async fails for %s\n",
					dc->conf_url->path);
			goto out_conf;
		}
	} else {
		pxe_conf_files = user_event_parse_conf_filenames(dc, dc->event);
		if (!pxe_conf_files)
			goto out_conf;

		pxe_base_url = pb_url_join(dc, dc->conf_url, pxelinux_prefix);
		if (!pxe_base_url)
			goto out_pxe_conf;

		info = conf->parser_info;
		info->pxe_conf_files = pxe_conf_files;
		info->pxe_base_url = pxe_base_url;

		device_handler_status_dev_info(conf->dc->handler,
			conf->dc->device,
			_("Probing from base %s"),
			pb_url_to_string(pxe_base_url));

		pxe_load_next_filename(conf);
	}

	return 0;

out_pxe_conf:
	talloc_free(pxe_conf_files);
out_conf:
	talloc_free(conf);
	return -1;
}

static struct parser pxe_parser = {
	.name			= "pxe",
	.parse			= pxe_parse,
};

register_parser(pxe_parser);
