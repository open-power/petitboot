
#include "parser-test.h"

#if 0 /* PARSER_EMBEDDED_CONFIG */
set default="linux2"

menuentry 'Linux 1' --id=linux1 {
	linux   /vmlinux1
}

menuentry 'Linux 2' --id=linux2 {
	linux   /vmlinux2
}
#endif

void run_test(struct parser_test *test)
{
	struct discover_boot_option *opt;
	struct discover_context *ctx;

	test_read_conf_embedded(test, "/grub2/grub.cfg");
	test_run_parser(test, "grub2");

	ctx = test->ctx;
	check_boot_option_count(ctx, 2);
	opt = get_boot_option(ctx, 1);
	check_is_default(opt);
}
