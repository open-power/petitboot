
#include "parser-test.h"

#if 0 /* PARSER_EMBEDDED_CONFIG */
menuentry 'Linux' --class test-class $menuentry_id_option 'test-id' {
	linux   /vmlinux arg1=value1 arg2
	initrd  /initrd
}
#endif

void run_test(struct parser_test *test)
{
	struct discover_boot_option *opt;
	struct discover_context *ctx;

	test_read_conf_embedded(test);
	test_run_parser(test, "grub2");

	ctx = test->ctx;

	check_boot_option_count(ctx, 1);
	opt = get_boot_option(ctx, 0);

	check_name(opt, "Linux");
	check_resolved_local_resource(opt->boot_image, ctx->device, "/vmlinux");
	check_resolved_local_resource(opt->initrd, ctx->device, "/initrd");

	check_args(opt, "arg1=value1 arg2");
}
