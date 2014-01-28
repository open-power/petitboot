
#include "parser-test.h"

void run_test(struct parser_test *test)
{
	struct discover_context *ctx;

	test_add_dir(test, test->ctx->device, "/ppc/ppc64");

	test_read_conf_file(test, "grub2-f20-ppc.conf",
			"/boot/grub/grub.cfg");

	test_run_parser(test, "grub2");

	ctx = test->ctx;

	check_boot_option_count(ctx, 3);
}
