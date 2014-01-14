#include "parser-test.h"

#if 0 /* PARSER_EMBEDDED_CONFIG */
append="console=ttyS0"
default=one

image=/boot/vmlinux-1
  label=one

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
	check_is_default(opt);
}
