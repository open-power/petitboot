
#include "parser-test.h"

/*
 * yocto default efi-grub wks doesn't put a space between the menuentry
 * label and the '{'
 */

#if 0 /* PARSER_EMBEDDED_CONFIG */
serial --unit=0 --speed=115200 --word=8 --parity=no --stop=1
default=boot
timeout=0
menuentry 'boot'{
linux /bzImage console=ttyS0,115200n8 console=tty0
}
#endif

void run_test(struct parser_test *test)
{
	struct discover_boot_option *opt;
	struct discover_context *ctx;

	test_read_conf_embedded(test, "/efi/boot/grub.cfg");

	test_run_parser(test, "grub2");

	ctx = test->ctx;

	check_boot_option_count(ctx, 1);
	opt = get_boot_option(ctx, 0);

	check_name(opt, "boot");
	check_resolved_local_resource(opt->boot_image, ctx->device, "/bzImage");
	check_not_present_resource(opt->initrd);
	check_is_default(opt);

	check_args(opt, "console=ttyS0,115200n8 console=tty0");
}
