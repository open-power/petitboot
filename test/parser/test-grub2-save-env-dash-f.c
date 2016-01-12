
#include <string.h>

#include <talloc/talloc.h>

#include "parser-test.h"

#if 0 /* PARSER_EMBEDDED_CONFIG */
hello=world
save_env -f env_file hello
#endif

static const char *envsig = "# GRUB Environment Block\n";

void run_test(struct parser_test *test)
{
	const char *env_before, *env_after;

	/* The environment file must be preallocated */

	/* The padding at the end of the environment block is the length of
	 * "hello=world\n" */
	env_before = talloc_asprintf(test, "%s%s", envsig,
					"############");
	test_add_file_data(test, test->ctx->device, "/boot/grub/env_file",
				env_before, strlen(env_before));

	env_after = talloc_asprintf(test, "%s%s", envsig,
					"hello=world\n");

	test_read_conf_embedded(test, "/boot/grub/grub.cfg");

	test_run_parser(test, "grub2");

	check_file_contents(test, test->ctx->device, "/boot/grub/env_file",
				env_after, strlen(env_after));
}
