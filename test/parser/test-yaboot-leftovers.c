#include "parser-test.h"

/* Test that an absent parameter in one boot option doesn't get set by a
 * previous option */

#if 0 /* PARSER_EMBEDDED_CONFIG */
image=/boot/vmlinux-1
  label=one
  literal="console=XXXX"

image=/boot/vmlinux-2
  label=two
#endif

void run_test(struct parser_test *test)
{
	struct discover_boot_option *opt;
	struct discover_context *ctx;

	test_read_conf_embedded(test, "/etc/yaboot.conf");

	test_run_parser(test, "yaboot");

	ctx = test->ctx;

	check_boot_option_count(ctx, 2);

	opt = get_boot_option(ctx, 0);
	check_name(opt, "one");
	check_args(opt, "console=XXXX");

	opt = get_boot_option(ctx, 1);
	check_name(opt, "two");
	check_args(opt, "");
}
