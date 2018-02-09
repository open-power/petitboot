/* test a standard yocto syslinux wic cfg */

#include "parser-test.h"

#if 0 /* PARSER_EMBEDDED_CONFIG */
PROMPT 0
TIMEOUT 0

ALLOWOPTIONS 1
SERIAL 0 115200

DEFAULT boot
LABEL boot
KERNEL /vmlinuz
APPEND console=ttyS0,115200n8 console=tty0
#endif

void run_test(struct parser_test *test)
{
	struct discover_boot_option *opt;
	struct discover_context *ctx;

	test_read_conf_embedded(test, "/syslinux.cfg");

	test_run_parser(test, "syslinux");

	ctx = test->ctx;

	check_boot_option_count(ctx, 1);
	opt = get_boot_option(ctx, 0);

	check_name(opt, "boot");
	check_resolved_local_resource(opt->boot_image, ctx->device, "/vmlinuz");
	check_is_default(opt);
	check_args(opt, " console=ttyS0,115200n8 console=tty0");
}
