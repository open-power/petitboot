
#include <string.h>

#include <util/util.h>
#include <talloc/talloc.h>

#include "parser-test.h"

static const char *envsig = "# GRUB Environment Block\n";

struct env_test {
	const char *name;
	const char *env_before;
	const char *script;
	const char *env_after;
} tests[] = {
	{
		"init",
		"######",
		"a=xxx\nsave_env a\n",
		"a=xxx\n"
	},
	{
		"append",
		"q=q\nr=r\n######",
		"a=xxx\nsave_env a\n",
		"q=q\nr=r\na=xxx\n"
	},
	{
		"expand",
		"q=q\na=x\nr=r\n##",
		"a=xxx\nsave_env a\n",
		"q=q\na=xxx\nr=r\n",
	},
	{
		"reduce",
		"q=q\na=xxx\nr=r\n",
		"a=x\nsave_env a\n",
		"q=q\na=x\nr=r\n##",
	},
	{
		"invalid-insert",
		"q=q\n---\nr=r\n",
		"a=x\nsave_env a\n",
		"q=q\na=x\nr=r\n",
	},
	{
		"invalid-shift",
		"q=q\n--\nr=r\n#",
		"a=x\nsave_env a\n",
		"q=q\na=x\nr=r\n",
	},
	{
		"invalid-reduce",
		"q=q\n----\nr=r\n",
		"a=x\nsave_env a\n",
		"q=q\na=x\nr=r\n#",
	},
	{
		"dup-replace-first",
		"q=q\na=y\nr=r\na=z",
		"a=x\nsave_env a\n",
		"q=q\na=x\nr=r\na=z",
	},
	{
		"nospace-add",
		"q=q\nr=r\n###",
		"a=x\nsave_env a\n",
		"q=q\nr=r\n###",
	},
	{
		"nospace-replace",
		"q=q\na=x\nr=r\n#",
		"a=xxx\nsave_env a\n",
		"q=q\na=x\nr=r\n#",
	},
	{
		"unset-var",
		"##############",
		"save_env an_unset_var\n",
		"an_unset_var=\n"
	}
};

static void run_env_test(struct parser_test *test, struct env_test *envtest)
{
	const char *env_before, *env_after;

	env_before = talloc_asprintf(test, "%s%s", envsig, envtest->env_before);
	env_after  = talloc_asprintf(test, "%s%s", envsig, envtest->env_after);

	test_add_file_data(test, test->ctx->device, "/boot/grub/grubenv",
			env_before, strlen(env_before));

	__test_read_conf_data(test, test->ctx->device,
			"/boot/grub/grub.cfg", envtest->script,
			strlen(envtest->script));

	test_run_parser(test, "grub2");

	check_file_contents(test, test->ctx->device, "/boot/grub/grubenv",
			env_after, strlen(env_after));
}

void run_test(struct parser_test *test)
{
	struct env_test *env_test;
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(tests); i++) {
		env_test = &tests[i];
		printf("test %s: ", env_test->name);
		fflush(stdout);
		run_env_test(test, env_test);
		printf("OK\n");
		fflush(stdout);
	}
}
