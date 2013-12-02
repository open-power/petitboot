
#include "parser-test.h"

#if 0 /* PARSER_EMBEDDED_CONFIG */
menuentry 'Linux' {
	search --set=root ec50d321-aab1-4335-8a87-aa8fadd80a09
	linux   /vmlinux
}
#endif

void run_test(struct parser_test *test)
{
	struct discover_boot_option *opt;
	struct discover_context *ctx;
	struct discover_device *dev;

	test_read_conf_embedded(test, "/grub.cfg");

	test_run_parser(test, "grub2");

	ctx = test->ctx;

	check_boot_option_count(ctx, 1);
	opt = get_boot_option(ctx, 0);
	check_name(opt, "Linux");
	check_unresolved_resource(opt->boot_image);

	test_remove_device(test, test->ctx->device);

	dev = test_create_device(test, "external");
	dev->uuid = "ec50d321-aab1-4335-8a87-aa8fadd80a09";
	test_hotplug_device(test, dev);

	check_boot_option_count(ctx, 0);
}
