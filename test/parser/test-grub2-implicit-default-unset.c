
#include "parser-test.h"

#if 0 /* PARSER_EMBEDDED_CONFIG */
menuentry 'test.1' {
	linux   /vmlinux
}
menuentry 'test.2' {
	linux   /vmlinux
}
#endif

void run_test(struct parser_test *test)
{
	struct discover_boot_option *opt;

	test_read_conf_embedded(test, "/grub2/grub.cfg");
	test_run_parser(test, "grub2");

	check_boot_option_count(test->ctx, 2);
	opt = get_boot_option(test->ctx, 0);

	check_name(opt, "test.1");
	check_is_default(opt);
}
