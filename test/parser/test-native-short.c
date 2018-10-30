#include "parser-test.h"

void run_test(struct parser_test *test)
{
	struct discover_boot_option *opt;
	struct discover_context *ctx;

	test_read_conf_file(test, "native-short.conf", "/boot/petitboot.conf");

	test_run_parser(test, "native");

	ctx = test->ctx;

	check_boot_option_count(ctx, 1);

	opt = get_boot_option(ctx, 0);

	check_name(opt, "Ubuntu");
	check_resolved_local_resource(opt->boot_image, ctx->device,
			"/boot/vmlinux-4.15.0-22-generic");
	check_args(opt, "root=UUID=09d1034f-3cff-413a-af22-68be1fa5e3d8 ro");
	check_resolved_local_resource(opt->initrd, ctx->device,
			"/boot/initrd.img-4.15.0-22-generic");
}
