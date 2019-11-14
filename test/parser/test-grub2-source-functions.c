
/* check that we can source other scripts, and functions can be defined
 * and called across sourced scripts */

#include "parser-test.h"

#if 0 /* PARSER_EMBEDDED_CONFIG */

function f1 {
	menuentry "f1$1" { linux $2 }
}

source /grub/2.cfg

f2 a /vmlinux

#endif

void run_test(struct parser_test *test)
{
	struct discover_boot_option *opt;
	struct discover_context *ctx;
	struct discover_device *dev;

	ctx = test->ctx;
	dev = ctx->device;

	test_read_conf_embedded(test, "/grub/grub.cfg");

	test_add_file_string(test, dev,
			"/grub/2.cfg",
			"function f2 { menuentry \"f2$1\" { linux $2 } }\n"
			"f1 a /vmlinux\n");

	test_run_parser(test, "grub2");

	check_boot_option_count(ctx, 2);

	opt = get_boot_option(ctx, 0);
	check_name(opt, "f1a");
	check_resolved_local_resource(opt->boot_image, dev, "/vmlinux");

	opt = get_boot_option(ctx, 1);
	check_name(opt, "f2a");
	check_resolved_local_resource(opt->boot_image, dev, "/vmlinux");
}
