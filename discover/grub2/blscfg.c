
#define _GNU_SOURCE

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

#include <log/log.h>
#include <file/file.h>
#include <talloc/talloc.h>
#include <i18n/i18n.h>

#include "grub2.h"
#include "discover/parser-conf.h"
#include "discover/parser.h"

#define BLS_DIR "/loader/entries"

struct bls_state {
	struct discover_boot_option *opt;
	struct grub2_script *script;
	const char *filename;
	const char *title;
	const char *version;
	const char *machine_id;
	const char *image;
	const char *initrd;
	const char *dtb;
};

static void bls_process_pair(struct conf_context *conf, const char *name,
			     char *value)
{
	struct bls_state *state = conf->parser_info;
	struct discover_boot_option *opt = state->opt;
	struct boot_option *option = opt->option;
	const char *boot_args;

	if (streq(name, "title")) {
		state->title = talloc_strdup(state, value);
		return;
	}

	if (streq(name, "version")) {
		state->version = talloc_strdup(state, value);
		return;
	}

	if (streq(name, "machine-id")) {
		state->machine_id = talloc_strdup(state, value);
		return;
	}

	if (streq(name, "linux")) {
		state->image = talloc_strdup(state, value);
		return;
	}

	if (streq(name, "initrd")) {
		state->initrd = talloc_strdup(state, value);
		return;
	}

	if (streq(name, "devicetree")) {
		state->dtb = talloc_strdup(state, value);
		return;
	}

	if (streq(name, "options")) {
		if (value[0] == '$') {
			boot_args = script_env_get(state->script, value + 1);
			if (!boot_args)
				return;

			option->boot_args = talloc_strdup(opt, boot_args);
		} else {
			option->boot_args = talloc_strdup(opt, value);
		}
		return;
	}
}

static bool option_is_default(struct grub2_script *script,
			      struct boot_option *option)
{
	const char *var;

	var = script_env_get(script, "default");
	if (!var)
		return false;

	if (!strcmp(var, option->id))
		return true;

	return !strcmp(var, option->name);
}

static void bls_finish(struct conf_context *conf)
{
	struct bls_state *state = conf->parser_info;
	struct discover_context *dc = conf->dc;
	struct discover_boot_option *opt = state->opt;
	struct boot_option *option = opt->option;
	const char *root;
	char *filename;

	if (!state->image) {
		device_handler_status_dev_info(dc->handler, dc->device,
					       _("linux field not found in %s"),
					       state->filename);
		return;
	}

	filename = basename(state->filename);
	filename[strlen(filename) - strlen(".conf")] = '\0';

	option->id = talloc_strdup(option, filename);

	if (state->title)
		option->name = talloc_strdup(option, state->title);
	else if (state->machine_id && state->version)
		option->name = talloc_asprintf(option, "%s %s",
					       state->machine_id,
					       state->version);
	else if (state->version)
		option->name = talloc_strdup(option, state->version);
	else
		option->name = talloc_strdup(option, state->image);

	root = script_env_get(state->script, "root");

	opt->boot_image = create_grub2_resource(opt, conf->dc->device,
						root, state->image);

	if (state->initrd)
		opt->initrd = create_grub2_resource(opt, conf->dc->device,
						    root, state->initrd);

	if (state->dtb)
		opt->dtb = create_grub2_resource(opt, conf->dc->device,
						 root, state->dtb);

	option->is_default = option_is_default(state->script, option);

	discover_context_add_boot_option(dc, opt);

	device_handler_status_dev_info(dc->handler, dc->device,
				       _("Created menu entry from BLS file %s"),
				       state->filename);
}

static int bls_filter(const struct dirent *ent)
{
	int offset = strlen(ent->d_name) - strlen(".conf");

	if (offset < 0)
		return 0;

	return strncmp(ent->d_name + offset, ".conf", strlen(".conf")) == 0;
}

static int bls_sort(const struct dirent **ent_a, const struct dirent **ent_b)
{
	return strverscmp((*ent_b)->d_name, (*ent_a)->d_name);
}

int builtin_blscfg(struct grub2_script *script,
		void *data __attribute__((unused)),
		int argc __attribute__((unused)),
		char *argv[] __attribute__((unused)));

int builtin_blscfg(struct grub2_script *script,
		void *data __attribute__((unused)),
		int argc __attribute__((unused)),
		char *argv[] __attribute__((unused)))
{
	struct discover_context *dc = script->ctx;
	struct dirent **bls_entries;
	struct conf_context *conf;
	struct bls_state *state;
	char *buf, *filename;
	int n, len, rc = -1;

	conf = talloc_zero(dc, struct conf_context);
	if (!conf)
		return rc;

	conf->dc = dc;
	conf->get_pair = conf_get_pair_space;
	conf->process_pair = bls_process_pair;
	conf->finish = bls_finish;

	n = parser_scandir(dc, BLS_DIR, &bls_entries, bls_filter, bls_sort);
	if (n <= 0)
		goto err;

	while (n--) {
		filename = talloc_asprintf(dc, BLS_DIR"/%s",
					   bls_entries[n]->d_name);
		if (!filename)
			break;

		state = talloc_zero(conf, struct bls_state);
		if (!state)
			break;

		state->opt = discover_boot_option_create(dc, dc->device);
		if (!state->opt)
			break;

		state->script = script;
		state->filename = filename;
		conf->parser_info = state;

		rc = parser_request_file(dc, dc->device, filename, &buf, &len);
		if (rc)
			break;

		conf_parse_buf(conf, buf, len);

		talloc_free(buf);
		talloc_free(state);
		talloc_free(filename);
		free(bls_entries[n]);
	}

	if (n > 0) {
		device_handler_status_dev_info(dc->handler, dc->device,
					       _("Scanning %s failed"),
					       BLS_DIR);
		do {
			free(bls_entries[n]);
		} while (n-- > 0);
	}

	free(bls_entries);
err:
	talloc_free(conf);
	return rc;
}
