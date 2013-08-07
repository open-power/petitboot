
#include "parser-test.h"

#if 0 /* PARSER_EMBEDDED_CONFIG */
default=linux
root=/dev/sda1

image=/vmlinux
	label=linux 1

image=/vmlinux
	label=linux 2
	root=/dev/sda2
#endif

void run_test(struct parser_test *test)
{
	struct discover_boot_option *opt;
	struct discover_context *ctx;

	test_read_conf_embedded(test);
	test_run_parser(test, "yaboot");

	ctx = test->ctx;

	check_boot_option_count(ctx, 2);

	opt = get_boot_option(ctx, 0);
	check_name(opt, "linux 1");
	check_args(opt, "root=/dev/sda1");

	opt = get_boot_option(ctx, 1);
	check_name(opt, "linux 2");
	check_args(opt, "root=/dev/sda2");
}
