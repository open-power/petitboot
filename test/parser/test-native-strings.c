#include "parser-test.h"

#if 0 /* PARSER_EMBEDDED_CONFIG */

name A longer name

image /some/kernel
initrd /some/other/initrd
args console=tty0 console=hvc0 debug 
dtb /a/dtb
description Contains a number of words

#endif

void run_test(struct parser_test *test)
{
	struct discover_boot_option *opt;
	struct discover_context *ctx;

	test_read_conf_embedded(test, "/boot/petitboot.conf");

	test_run_parser(test, "native");

	ctx = test->ctx;

	check_boot_option_count(ctx, 1);

	opt = get_boot_option(ctx, 0);

	check_name(opt, "A longer name");
	check_resolved_local_resource(opt->boot_image, ctx->device, "/some/kernel");
	check_args(opt, "console=tty0 console=hvc0 debug ");
	check_resolved_local_resource(opt->initrd, ctx->device, "/some/other/initrd");
	check_resolved_local_resource(opt->dtb, ctx->device, "/a/dtb");
}
