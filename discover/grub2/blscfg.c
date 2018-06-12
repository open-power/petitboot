
#define _GNU_SOURCE

#include <assert.h>
#include <ctype.h>
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

static const char *const bls_dirs[] = {
	"/loader/entries",
	"/boot/loader/entries",
	NULL
};

struct bls_state {
	struct discover_boot_option *opt;
	struct grub2_script *script;
	unsigned int idx;
	const char *filename;
	const char *title;
	const char *version;
	const char *machine_id;
	const char *image;
	const char *initrd;
	const char *dtb;
};

static char *field_append(struct bls_state *state, int type, char *buffer,
			  char *start, char *end)
{
	char *temp = talloc_strndup(state, start, end - start + 1);
	const char *field = temp;

	if (type == GRUB2_WORD_VAR) {
		field = script_env_get(state->script, temp);
		if (!field)
			return buffer;
	}

	if (!buffer)
		buffer = talloc_strdup(state->opt, field);
	else
		buffer = talloc_asprintf_append(buffer, "%s", field);

	return buffer;
}

static char *expand_field(struct bls_state *state, char *value)
{
	char *buffer = NULL;
	char *start = value;
	char *end = value;
	int type = GRUB2_WORD_TEXT;

	while (*value) {
		if (*value == '$') {
			if (start != end) {
				buffer = field_append(state, type, buffer,
						      start, end);
				if (!buffer)
					return NULL;
			}

			type = GRUB2_WORD_VAR;
			start = value + 1;
		} else if (type == GRUB2_WORD_VAR) {
			if (!isalnum(*value) && *value != '_') {
				buffer = field_append(state, type, buffer,
						      start, end);
				type = GRUB2_WORD_TEXT;
				start = value;
			}
		}

		end = value;
		value++;
	}

	if (start != end) {
		buffer = field_append(state, type, buffer,
				      start, end);
		if (!buffer)
			return NULL;
	}

	return buffer;
}

static void bls_process_pair(struct conf_context *conf, const char *name,
			     char *value)
{
	struct bls_state *state = conf->parser_info;
	struct discover_boot_option *opt = state->opt;
	struct boot_option *option = opt->option;

	if (streq(name, "title")) {
		state->title = expand_field(state, value);
		return;
	}

	if (streq(name, "version")) {
		state->version = expand_field(state, value);
		return;
	}

	if (streq(name, "machine-id")) {
		state->machine_id = expand_field(state, value);
		return;
	}

	if (streq(name, "linux")) {
		state->image = expand_field(state, value);
		return;
	}

	if (streq(name, "initrd")) {
		state->initrd = expand_field(state, value);
		return;
	}

	if (streq(name, "devicetree")) {
		state->dtb = expand_field(state, value);
		return;
	}

	if (streq(name, "options")) {
		option->boot_args = expand_field(state, value);
		return;
	}
}

static bool option_is_default(struct bls_state *state,
			      struct boot_option *option)
{
	unsigned int idx;
	const char *var;
	char *end;

	var = script_env_get(state->script, "default");
	if (!var)
		return false;

	if (!strcmp(var, option->id))
		return true;

	if (!strcmp(var, option->name))
		return true;

	idx = strtoul(var, &end, 10);
	return end != var && *end == '\0' && idx == state->idx;
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

	char* args_sigfile_default = talloc_asprintf(opt,
		"%s.cmdline.sig", state->image);
	opt->args_sig_file = create_grub2_resource(opt, conf->dc->device,
						root, args_sigfile_default);
	talloc_free(args_sigfile_default);

	option->is_default = option_is_default(state, option);

	list_add_tail(&state->script->options, &opt->list);
	state->script->n_options++;

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
	return strverscmp((*ent_a)->d_name, (*ent_b)->d_name);
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
	unsigned int current_idx = script->n_options;
	struct discover_context *dc = script->ctx;
	struct dirent **bls_entries;
	struct conf_context *conf;
	struct bls_state *state;
	char *buf, *filename;
	const char * const *dir;
	const char *blsdir;
	int n, len, rc = -1;
	struct stat statbuf;

	conf = talloc_zero(dc, struct conf_context);
	if (!conf)
		return rc;

	conf->dc = dc;
	conf->get_pair = conf_get_pair_space;
	conf->process_pair = bls_process_pair;
	conf->finish = bls_finish;

	blsdir = script_env_get(script, "blsdir");
	if (!blsdir)
		for (dir = bls_dirs; *dir; dir++)
			if (!parser_stat_path(dc, dc->device, *dir, &statbuf)) {
				blsdir = *dir;
				break;
			}

	if (!blsdir) {
		device_handler_status_dev_info(dc->handler, dc->device,
					       _("BLS directory wasn't found"));
		goto err;
	}

	n = parser_scandir(dc, blsdir, &bls_entries, bls_filter, bls_sort);
	if (n <= 0)
		goto err;

	while (n--) {
		filename = talloc_asprintf(dc, "%s/%s", blsdir,
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
		state->idx = current_idx++;
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
					       blsdir);
		do {
			free(bls_entries[n]);
		} while (n-- > 0);
	}

	free(bls_entries);
err:
	talloc_free(conf);
	return rc;
}
