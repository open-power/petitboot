
#include <stdio.h>
#include <string.h>

#include <log/log.h>
#include <types/types.h>
#include <talloc/talloc.h>
#include <util/util.h>
#include <url/url.h>

#include "discover/resource.h"
#include "discover/parser.h"
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

	char* args_sigfile_default = talloc_asprintf(opt,
		"%s.cmdline.sig", argv[1]);
	opt->args_sig_file = create_grub2_resource(opt, script->ctx->device,
						root, args_sigfile_default);
	talloc_free(args_sigfile_default);
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

/* Note that GRUB does not follow symlinks in evaluating all file
 * tests but -s, unlike below. However, it seems like a bad idea to
 * emulate GRUB's behavior (e.g., it would take extra work), so we
 * implement the behavior that coreutils' test binary has. */
static bool builtin_test_op_file(struct grub2_script *script, char op,
		const char *file)
{
	bool result;
	int rc;
	struct stat statbuf;

	rc = parser_stat_path(script->ctx, script->ctx->device,
			file, &statbuf);
	if (rc)
		return false;

	switch (op) {
	case 's':
		/* -s: return true if file exists and has non-zero size */
		result = statbuf.st_size > 0;
		break;
	case 'f':
		/* -f: return true if file exists and is not a directory. This is
		 * different than the behavior of "test", but is what GRUB does
		 * (though note as above that we follow symlinks unlike GRUB). */
		result = !S_ISDIR(statbuf.st_mode);
		break;
	default:
		result = false;

	}

	return result;
}

/* See comment at builtin_test_op_file for differences between how
 * GRUB implements file tests versus Petitboot's GRUB parser. */
static bool builtin_test_op_dir(struct grub2_script *script, char op,
		const char *dir)
{
	int rc;
	struct stat statbuf;

	if (op != 'd')
		return false;

	rc = parser_stat_path(script->ctx, script->ctx->device, dir, &statbuf);
	if (rc) {
		return false;
	}

	return S_ISDIR(statbuf.st_mode);
}

static bool builtin_test_op(struct grub2_script *script,
		int argc, char **argv, int *consumed)
{
	char *op;

	if (argc >= 3) {
		const char *a1, *a2;

		a1 = argv[0];
		op = argv[1];
		a2 = argv[2];

		if (!strcmp(op, "=") || !strcmp(op, "==")) {
			*consumed = 3;
			return !strcmp(a1, a2);
		}

		if (!strcmp(op, "!=")) {
			*consumed = 3;
			return strcmp(a1, a2);
		}

		if (!strcmp(op, "<")) {
			*consumed = 3;
			return strcmp(a1, a2) < 0;
		}

		if (!strcmp(op, ">")) {
			*consumed = 3;
			return strcmp(a1, a2) > 0;
		}
	}

	if (argc >= 2) {
		const char *a1;

		op = argv[0];
		a1 = argv[1];

		if (!strcmp(op, "-z")) {
			*consumed = 2;
			return strlen(a1) == 0;
		}

		if (!strcmp(op, "-n")) {
			*consumed = 2;
			return strlen(a1) != 0;
		}

		if (!strcmp(op, "-s") || !strcmp(op, "-f")) {
			*consumed = 2;
			return builtin_test_op_file(script, op[1], a1);
		}

		if (!strcmp(op, "-d")) {
			*consumed = 2;
			return builtin_test_op_dir(script, op[1], a1);
		}
	}

	op = argv[0];
	*consumed = 1;
	return strlen(op) > 0;
}

static int builtin_test(struct grub2_script *script,
		void *data __attribute__((unused)),
		int argc, char *argv[])
{
	int consumed;
	bool not, rc;

	if (!strcmp(argv[0], "[") && !strcmp(argv[argc - 1], "]"))
		argc--;

	/* skip command name */
	argc--;
	argv++;

	not = false;
	rc = false;

	for (consumed = 0; argc > 0; argv += consumed, argc -= consumed) {

		if (!strcmp(argv[0], "!")) {
			not = true;
			consumed = 1;
			continue;
		}

		if (!strcmp(argv[0], "-a")) {
			if (!rc)
				return 1;
			consumed = 1;
			continue;
		}

		if (!strcmp(argv[0], "-o")) {
			if (rc)
				return 0;
			consumed = 1;
			continue;
		}

		rc = builtin_test_op(script, argc, argv, &consumed);
		if (not) {
			rc = !rc;
			not = false;
		}
	}

	return rc ? 0 : 1;
}

static int builtin_true(struct grub2_script *script __attribute__((unused)),
		void *data __attribute__((unused)),
		int argc __attribute__((unused)),
		char *argv[] __attribute__((unused)))
{
	return 0;
}

static int builtin_false(struct grub2_script *script __attribute__((unused)),
		void *data __attribute__((unused)),
		int argc __attribute__((unused)),
		char *argv[] __attribute__((unused)))
{
	return 1;
}

static int builtin_nop(struct grub2_script *script __attribute__((unused)),
		void *data __attribute__((unused)),
		int argc __attribute__((unused)),
		char *argv[] __attribute__((unused)))
{
	return 0;
}

extern int builtin_load_env(struct grub2_script *script,
		void *data __attribute__((unused)),
		int argc, char *argv[]);
int builtin_save_env(struct grub2_script *script,
		void *data __attribute__((unused)),
		int argc, char *argv[]);
int builtin_blscfg(struct grub2_script *script,
		void *data __attribute__((unused)),
		int argc __attribute__((unused)),
		char *argv[] __attribute__((unused)));

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
		.name = "linux16",
		.fn = builtin_linux,
	},
	{
		.name = "initrd",
		.fn = builtin_initrd,
	},
	{
		.name = "search",
		.fn = builtin_search,
	},
	{
		.name = "[",
		.fn = builtin_test,
	},
	{
		.name = "test",
		.fn = builtin_test,
	},
	{
		.name = "true",
		.fn = builtin_true,
	},
	{
		.name = "false",
		.fn = builtin_false,
	},
	{
		.name = "load_env",
		.fn = builtin_load_env,
	},
	{
		.name = "save_env",
		.fn = builtin_save_env,
	},
	{
		.name = "blscfg",
		.fn = builtin_blscfg,
	}
};

static const char *nops[] = {
	"echo", "export", "insmod", "loadfont", "terminfo",
};

void register_builtins(struct grub2_script *script)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(builtins); i++)
		script_register_function(script, builtins[i].name,
				builtins[i].fn, NULL);

	for (i = 0; i < ARRAY_SIZE(nops); i++)
		script_register_function(script, nops[i], builtin_nop, NULL);
}
