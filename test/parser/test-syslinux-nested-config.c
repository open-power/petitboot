
#include "parser-test.h"


void run_test(struct parser_test *test)
{
	struct discover_boot_option *opt;
	struct discover_context *ctx;

	test_read_conf_file(test, "syslinux-include-root.cfg", "/boot/syslinux/syslinux.cfg");
	test_read_conf_file(test, "syslinux-include-nest-1.cfg", "/syslinux-include-nest-1.cfg");
	test_read_conf_file(test, "syslinux-include-nest-2.cfg", "/boot/syslinux/syslinux-include-nest-2.cfg");

	test_run_parser(test, "syslinux");

	ctx = test->ctx;

	check_boot_option_count(ctx, 3);

	opt = get_boot_option(ctx, 1);

	check_name(opt, "boot");
	check_resolved_local_resource(opt->boot_image, ctx->device, "/bzImage-boot");
	check_is_default(opt);
	check_args(opt, "console=ttyS0 root=/dev/sda");
	check_resolved_local_resource(opt->initrd, ctx->device, "/initrd-boot");

	opt = get_boot_option(ctx, 2);

	check_name(opt, "backup");
	check_resolved_local_resource(opt->boot_image, ctx->device, "/backup/vmlinuz");
	check_args(opt, "console=ttyS0 root=/dev/sdb");
	check_resolved_local_resource(opt->initrd, ctx->device, "/boot/initrd");

	opt = get_boot_option(ctx, 0);

	check_name(opt, "linux");
	check_resolved_local_resource(opt->boot_image, ctx->device, "/boot/bzImage");
	check_args(opt, "console=ttyS0 root=/dev/sdc");
	check_not_present_resource(opt->initrd);
}
