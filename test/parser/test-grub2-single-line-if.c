
#include "parser-test.h"

#if 0 /* PARSER_EMBEDDED_CONFIG */
var=empty

if true; then var=true; else var=false; fi

menuentry "option $var" {
	linux   /vmlinux
}
#endif

void run_test(struct parser_test *test)
{
	struct discover_boot_option *opt;
	struct discover_context *ctx;

	test_read_conf_embedded(test, "/grub.cfg");

	test_run_parser(test, "grub2");

	ctx = test->ctx;

	check_boot_option_count(ctx, 1);
	opt = get_boot_option(ctx, 0);

	check_name(opt, "option true");
}
