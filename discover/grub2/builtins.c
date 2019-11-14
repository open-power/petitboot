
#define _GNU_SOURCE

#include <getopt.h>
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

	opt->boot_image = create_grub2_resource(script, opt, argv[1]);
	opt->option->boot_args = NULL;

	if (argc > 2)
		opt->option->boot_args = talloc_strdup(opt, argv[2]);

	for (i = 3; i < argc; i++)
		opt->option->boot_args = talloc_asprintf_append(
						opt->option->boot_args,
						" %s", argv[i]);

	char* args_sigfile_default = talloc_asprintf(opt,
		"%s.cmdline.sig", argv[1]);
	opt->args_sig_file = create_grub2_resource(script, opt,
						args_sigfile_default);
	talloc_free(args_sigfile_default);
	return 0;
}

static int builtin_initrd(struct grub2_script *script,
		void *data __attribute__((unused)),
		int argc, char *argv[])
{
	struct discover_boot_option *opt = script->opt;

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

	opt->initrd = create_grub2_resource(script, opt, argv[1]);

	return 0;
}

static const struct option search_options[] = {
	{
		.name = "set",
		.has_arg = required_argument,
		.val = 's',
	},
	{
		.name = "file",
		.has_arg = no_argument,
		.val = 'f',
	},
	{
		.name = "label",
		.has_arg = no_argument,
		.val = 'l',
	},
	{
		.name = "fs-uuid",
		.has_arg = no_argument,
		.val = 'u',
	},
	{ 0 },
};

static int builtin_search(struct grub2_script *script,
		void *data __attribute__((unused)),
		int argc, char *argv[])
{
	const char *env_var, *spec, *res;
	struct discover_device *dev;
	enum {
		LOOKUP_UUID = 'u',
		LOOKUP_LABEL = 'l',
		LOOKUP_FILE = 'f',
	} lookup_type;

	env_var = "root";
	optind = 0;

	/* Default to UUID, for backwards compat with earlier petitboot
	 * versions. This argument is non-optional in GRUB. */
	lookup_type = LOOKUP_UUID;

	for (;;) {
		int c = getopt_long(argc, argv, ":flu", search_options, NULL);
		if (c == -1)
			break;

		switch (c) {
		case 's':
			env_var = optarg;
			break;
		case LOOKUP_UUID:
		case LOOKUP_LABEL:
		case LOOKUP_FILE:
			lookup_type = c;
			break;
		case '?':
		case ':':
			break;
		}
	}

	if (!strlen(env_var))
		return 0;

	if (optind >= argc)
		return -1;

	spec = argv[optind];
	res = NULL;

	switch (lookup_type) {
	case LOOKUP_UUID:
		dev = device_lookup_by_uuid(script->ctx->handler,
				spec);
		res = dev ? dev->device->id : spec;
		break;
	case LOOKUP_LABEL:
		dev = device_lookup_by_label(script->ctx->handler,
				spec);
		if (dev)
			res = dev->device->id;
		break;
	case LOOKUP_FILE:
		/* not yet implemented */
		break;
	}

	if (res)
		script_env_set(script, env_var, res);

	return 0;
}

static int parse_to_device_path(struct grub2_script *script,
		const char *desc, struct discover_device **devp,
		char **pathp)
{
	struct discover_device *dev;
	struct grub2_file *file;

	file = grub2_parse_file(script, desc);
	if (!file)
		return -1;

	dev = script->ctx->device;
	if (file->dev)
		dev = grub2_lookup_device(script->ctx->handler, file->dev);

	if (!dev)
		return -1;

	*devp = dev;
	*pathp = talloc_strdup(script, file->path);

	talloc_free(file);

	return 0;
}

/* Note that GRUB does not follow symlinks in evaluating all file
 * tests but -s, unlike below. However, it seems like a bad idea to
 * emulate GRUB's behavior (e.g., it would take extra work), so we
 * implement the behavior that coreutils' test binary has. */
static bool builtin_test_op_file(struct grub2_script *script, char op,
		const char *file)
{
	struct discover_device *dev;
	struct stat statbuf;
	bool result;
	char *path;
	int rc;

	rc = parse_to_device_path(script, file, &dev, &path);
	if (rc)
		return false;

	rc = parser_stat_path(script->ctx, dev, path, &statbuf);
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
	struct discover_device *dev;
	struct stat statbuf;
	char *path;
	int rc;

	if (op != 'd')
		return false;

	rc = parse_to_device_path(script, dir, &dev, &path);
	if (rc)
		return false;

	rc = parser_stat_path(script->ctx, dev, path, &statbuf);
	if (rc)
		return false;

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

static int builtin_source(struct grub2_script *script,
		void *data __attribute__((unused)),
		int argc, char *argv[])
{
	struct grub2_statements *statements;
	struct discover_device *dev;
	const char *filename;
	char *path, *buf;
	int rc, len;

	if (argc != 2)
		return false;

	/* limit script recursion */
	if (script->include_depth >= 10)
		return false;

	rc = parse_to_device_path(script, argv[1], &dev, &path);
	if (rc)
		return false;

	rc = parser_request_file(script->ctx, dev, path, &buf, &len);
	if (rc)
		return false;

	/* save current script state */
	statements = script->statements;
	filename = script->filename;
	script->include_depth++;

	rc = grub2_parser_parse(script->parser, argv[1], buf, len);

	if (!rc)
		statements_execute(script, script->statements);

	talloc_free(script->statements);

	/* restore state */
	script->statements = statements;
	script->filename = filename;
	script->include_depth--;

	return !rc;
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
		.name = "initrd16",
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
	},
	{
		.name = "source",
		.fn = builtin_source,
	},
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
