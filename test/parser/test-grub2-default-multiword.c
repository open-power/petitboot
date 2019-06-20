
#include "parser-test.h"

#if 0 /* PARSER_EMBEDDED_CONFIG */
set default="Multiple word option"
menuentry 'Non-defalt option' --id=option0 {
	linux /vmlinux.non-default
}
menuentry 'Multiple word option' --id=option1 {
	linux   /vmlinux
}
#endif

void run_test(struct parser_test *test)
{
	struct discover_boot_option *opt;
	struct discover_context *ctx;

	test_read_conf_embedded(test, "/grub2/grub.cfg");
	test_run_parser(test, "grub2");

	ctx = test->ctx;

	check_boot_option_count(ctx, 2);

	opt = get_boot_option(ctx, 1);
	check_name(opt, "Multiple word option");
	check_resolved_local_resource(opt->boot_image, ctx->device, "/vmlinux");
	check_is_default(opt);
}
