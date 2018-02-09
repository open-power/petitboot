
#include "parser-test.h"

#if 0 /* PARSER_EMBEDDED_CONFIG */

APPEND console=ttyS0

LABEL linux
LINUX /vmlinuz
APPEND console=tty0

LABEL backup
KERNEL /backup/vmlinuz
APPEND root=/dev/sdb
INITRD /boot/initrd

LABEL hyphen
KERNEL /test/vmlinuz
APPEND -

#endif

void run_test(struct parser_test *test)
{
	struct discover_boot_option *opt;
	struct discover_context *ctx;

	test_read_conf_embedded(test, "/syslinux/syslinux.cfg");

	test_run_parser(test, "syslinux");

	ctx = test->ctx;

	check_boot_option_count(ctx, 3);
	opt = get_boot_option(ctx, 2);

	check_name(opt, "linux");
	check_resolved_local_resource(opt->boot_image, ctx->device, "/vmlinuz");
	check_is_default(opt);
	check_args(opt, "console=ttyS0 console=tty0");
	check_not_present_resource(opt->initrd);

	opt = get_boot_option(ctx, 1);

	check_name(opt, "backup");
	check_resolved_local_resource(opt->boot_image, ctx->device, "/backup/vmlinuz");
	check_args(opt, "console=ttyS0 root=/dev/sdb");
	check_resolved_local_resource(opt->initrd, ctx->device, "/boot/initrd");

	opt = get_boot_option(ctx, 0);

	check_name(opt, "hyphen");
	check_resolved_local_resource(opt->boot_image, ctx->device, "/test/vmlinuz");
	check_args(opt, "");
	check_not_present_resource(opt->initrd);
}
