
#include <stdio.h>
#include <string.h>

#include <log/log.h>
#include <types/types.h>
#include <talloc/talloc.h>
#include <array-size/array-size.h>

#include "grub2.h"


static int builtin_set(struct grub2_script *script,
		void *data __attribute__((unused)),
		int argc, char *argv[])
{
	char *name, *value, *p;
	int i;

	if (argc < 2)
		return -1;

	p = strchr(argv[1], '=');
	if (!p)
		return -1;

	name = talloc_strndup(script, argv[1], p - argv[1]);
	value = talloc_strdup(script, p+1);

	for (i = 2; i < argc; i++)
		value = talloc_asprintf_append(value, " %s", argv[i]);

	script_env_set(script, name, value);

	return 0;
}

static int builtin_linux(struct grub2_script *script,
		void *data __attribute__((unused)),
		int argc, char *argv[])
{
	struct discover_boot_option *opt = script->opt;
	const char *root;
	int i;

	if (!opt) {
		pb_log("grub2 syntax error: 'linux' statement outside "
				"a menuentry.\n");
		return -1;
	}

	if (argc < 2) {
		pb_log("grub2 syntax error: no filename provided to "
				"linux statement\n");
		return -1;
	}

	root = script_env_get(script, "root");

	opt->boot_image = create_grub2_resource(opt, script->ctx->device,
						root, argv[1]);
	opt->option->boot_args = NULL;

	if (argc > 2)
		opt->option->boot_args = talloc_strdup(opt, argv[2]);

	for (i = 3; i < argc; i++)
		opt->option->boot_args = talloc_asprintf_append(
						opt->option->boot_args,
						" %s", argv[i]);
	return 0;
}

static int builtin_initrd(struct grub2_script *script,
		void *data __attribute__((unused)),
		int argc, char *argv[])
{
	struct discover_boot_option *opt = script->opt;
	const char *root;

	if (!opt) {
		pb_log("grub2 syntax error: 'initrd' statement outside "
				"a menuentry.\n");
		return -1;
	}

	if (argc < 2) {
		pb_log("grub2 syntax error: no filename provided to "
				"initrd statement\n");
		return -1;
	}

	root = script_env_get(script, "root");
	opt->initrd = create_grub2_resource(opt, script->ctx->device,
						root, argv[1]);

	return 0;
}

static int builtin_search(struct grub2_script *script,
		void *data __attribute__((unused)),
		int argc, char *argv[])
{
	const char *env_var, *spec;
	int i;

	env_var = NULL;

	for (i = 1; i < argc - 1; i++) {
		if (!strncmp(argv[i], "--set=", strlen("--set="))) {
			env_var = argv[i] + strlen("--set=");
			break;
		}
	}

	if (!env_var)
		return 0;

	spec = argv[argc - 1];

	script_env_set(script, env_var, spec);

	return 0;
}

static struct {
	const char *name;
	grub2_function fn;
} builtins[] = {
	{
		.name = "set",
		.fn = builtin_set,
	},
	{
		.name = "linux",
		.fn = builtin_linux,
	},
	{
		.name = "initrd",
		.fn = builtin_initrd,
	},
	{
		.name = "search",
		.fn = builtin_search,
	}
};

void register_builtins(struct grub2_script *script)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(builtins); i++)
		script_register_function(script, builtins[i].name,
				builtins[i].fn, NULL);
}
