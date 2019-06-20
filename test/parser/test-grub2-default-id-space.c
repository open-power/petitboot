
#include "parser-test.h"

#if 0 /* PARSER_EMBEDDED_CONFIG */
set default=option1
menuentry 'test-option-0' --id option0 {
	linux   /vmlinux.0
}
menuentry 'test-option-1' --id option1 {
	linux   /vmlinux.1
}
menuentry 'test-option-2' --id option2 {
	linux   /vmlinux.2
}
#endif

void run_test(struct parser_test *test)
{
	struct discover_boot_option *opt;
	struct discover_context *ctx;

	test_read_conf_embedded(test, "/boot/grub2/grub.cfg");
	test_run_parser(test, "grub2");

	ctx = test->ctx;

	check_boot_option_count(ctx, 3);
	opt = get_boot_option(ctx, 1);

	check_name(opt, "test-option-1");
	check_resolved_local_resource(opt->boot_image, ctx->device,
					"/vmlinux.1");
	check_is_default(opt);
}
