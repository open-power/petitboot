/* test a standard yocto syslinux wic cfg */

#include "parser-test.h"

#if 0 /* PARSER_EMBEDDED_CONFIG */


DEFAULT boot

KERNEL /vmlinuz
APPEND console=tty0

LABEL backup
KERNEL /backup/vmlinuz
APPEND root=/dev/sdb
INITRD /boot/initrd

IMPLICIT 0

#endif

void run_test(struct parser_test *test)
{
	struct discover_boot_option *opt;
	struct discover_context *ctx;

	test_read_conf_embedded(test, "/boot/syslinux/syslinux.cfg");

	test_run_parser(test, "syslinux");

	ctx = test->ctx;

	check_boot_option_count(ctx, 1);

	opt = get_boot_option(ctx, 0);

	check_name(opt, "backup");
	check_resolved_local_resource(opt->boot_image, ctx->device, "/backup/vmlinuz");
	check_args(opt, " root=/dev/sdb");
	check_resolved_local_resource(opt->initrd, ctx->device, "/boot/initrd");
}
