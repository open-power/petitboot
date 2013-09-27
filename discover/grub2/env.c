
#include <stdio.h>
#include <string.h>

#include <log/log.h>
#include <types/types.h>
#include <talloc/talloc.h>
#include <array-size/array-size.h>

#include <discover/parser.h>
#include <discover/file.h>

#include "grub2.h"

static const char *default_envfile = "grubenv";
static const char *signature = "# GRUB Environment Block\n";

static int parse_buf_to_env(struct grub2_script *script, void *buf, int len)
{
	char *tmp, *line, *sep;
	int siglen;

	siglen = strlen(signature);

	if (len < siglen) {
		pb_log("grub environment block too small\n");
		return -1;
	}

	if (memcmp(buf, signature, siglen)) {
		pb_log("grub environment block has invalid signature\n");
		return -1;
	}

	buf += siglen;

	for (line = strtok_r(buf, "\n", &tmp); line;
				line = strtok_r(NULL, "\n", &tmp)) {

		if (*line == '#')
			continue;

		sep = strchr(line, '=');
		if (!sep)
			continue;
		if (sep == line)
			continue;

		*sep = '\0';
		script_env_set(script, line, sep + 1);
	}

	return 0;
}

int builtin_load_env(struct grub2_script *script,
		void *data __attribute__((unused)),
		int argc, char *argv[]);

int builtin_load_env(struct grub2_script *script,
		void *data __attribute__((unused)),
		int argc, char *argv[])
{
	struct discover_device *dev = script->ctx->device;
	const char *envfile;
	char *buf, *envpath;
	int rc, len;

	/* we only support local filesystems */
	if (!dev->mounted) {
		pb_log("load_env: can't load from a non-mounted device (%s)\n",
				dev->device->id);
		return -1;
	}

	if (argc == 3 && !strcmp(argv[1], "-f"))
		envfile = argv[2];
	else
		envfile = default_envfile;

	envpath = talloc_asprintf(script, "%s/%s",
				script_env_get(script, "prefix") ? : "",
				envfile);

	rc = parser_request_file(script->ctx, dev, envpath, &buf, &len);

	if (!rc)
		rc = parse_buf_to_env(script, buf, len);

	talloc_free(buf);

	return 0;
}

int builtin_save_env(struct grub2_script *script,
		void *data __attribute__((unused)),
		int argc, char *argv[]);

int builtin_save_env(struct grub2_script *script __attribute__((unused)),
		void *data __attribute__((unused)),
		int argc __attribute__((unused)),
		char *argv[] __attribute__((unused)))
{
	/* todo: save */
	return 0;
}

