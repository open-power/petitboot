#include "parser-test.h"

#if 0 /* PARSER_EMBEDDED_CONFIG */

name native-option
image /vmlinuz
args console=hvc0
initrd /initrd

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

	check_name(opt, "native-option");
	check_resolved_local_resource(opt->boot_image, ctx->device, "/vmlinuz");
	check_args(opt, "console=hvc0");
	check_resolved_local_resource(opt->initrd, ctx->device, "/initrd");
}
