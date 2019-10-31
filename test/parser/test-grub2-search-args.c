
/* check for multiple styles of option parsing for the 'search' command */

#include "parser-test.h"

#if 0 /* PARSER_EMBEDDED_CONFIG */

# no --set arugment will set the 'root' var
search a
search --set=v1 b
search --set v2 c
search --set=v3 --no-floppy d
search --no-floppy --set=v4 e

menuentry $root$v1$v2$v3$v4 {
    linux /vmlinux
}

#endif

void run_test(struct parser_test *test)
{
	struct discover_boot_option *opt;
	struct discover_context *ctx;

	ctx = test->ctx;

	test_read_conf_embedded(test, "/grub/grub.cfg");

	test_run_parser(test, "grub2");

	check_boot_option_count(ctx, 1);
	opt = get_boot_option(ctx, 0);
	check_name(opt, "abcde");
}
