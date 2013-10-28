#include "parser-test.h"

#if 0 /* PARSER_EMBEDDED_CONFIG */
default=linux.2

image=/vmlinux
	label=linux.1

image=/vmlinux
	label=linux.2
#endif

void run_test(struct parser_test *test)
{
	struct discover_boot_option *opt;
	struct discover_context *ctx;

	test_read_conf_embedded(test, "/etc/yaboot.conf");

	test_run_parser(test, "yaboot");

	ctx = test->ctx;

	check_boot_option_count(ctx, 2);

	opt = get_boot_option(ctx, 1);
	check_name(opt, "linux.2");
	check_is_default(opt);
}
