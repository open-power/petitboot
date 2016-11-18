
#include "parser-test.h"

#error "very egregious error"

#if 0 /* PARSER_EMBEDDED_CONFIG */
load_env
menuentry 'Linux' {
	linux   $kernel
}
#endif

void run_test(struct parser_test *test)
{
	struct discover_boot_option *opt;
	struct discover_context *ctx;

	test_add_file_string(test, test->ctx->device,
				"/boot/grub/grubenv",
				"# GRUB Environment Block\n"
				"kernel=vmlinux-from-env\n");

	test_read_conf_embedded(test, "/boot/grub/grub.cfg");

	test_run_parser(test, "grub2");

	ctx = test->ctx;

	check_boot_option_count(ctx, 1);
	opt = get_boot_option(ctx, 0);

	check_name(opt, "Linux");
	check_resolved_local_resource(opt->boot_image, ctx->device,
			"/vmlinux-from-env");
}
