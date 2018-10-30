#include "parser-test.h"

#if 0 /* PARSER_EMBEDDED_CONFIG */

default first

name first
image /vmlinuz
args console=hvc0
initrd /initrd

name second
image /boot/vmlinuz
args console=tty0
initrd /boot/initrd

#endif

void run_test(struct parser_test *test)
{
	struct discover_boot_option *opt;
	struct discover_context *ctx;

	test_read_conf_embedded(test, "/boot/petitboot.conf");

	test_run_parser(test, "native");

	ctx = test->ctx;

	check_boot_option_count(ctx, 2);

	opt = get_boot_option(ctx, 0);

	check_name(opt, "first");
	check_resolved_local_resource(opt->boot_image, ctx->device, "/vmlinuz");
	check_args(opt, "console=hvc0");
	check_resolved_local_resource(opt->initrd, ctx->device, "/initrd");
	check_is_default(opt);

	opt = get_boot_option(ctx, 1);
	check_name(opt, "second");
	check_resolved_local_resource(opt->boot_image, ctx->device, "/boot/vmlinuz");
	check_args(opt, "console=tty0");
	check_resolved_local_resource(opt->initrd, ctx->device, "/boot/initrd");
}
