
#include "parser-test.h"

#if 0 /* PARSER_EMBEDDED_CONFIG */
menuentry 'Linux 1' {
	search --set=root 48c1b787-20ad-47ce-b9eb-b108dddc3535
	linux   /vmlinux
}

menuentry 'Linux 2' {
	search --set=root 48c1b787-20ad-47ce-b9eb-b108dddc3535
	linux   /vmlinux
}
#endif

void run_test(struct parser_test *test)
{
	struct discover_boot_option *opt[2];
	struct discover_context *ctx;
	struct discover_device *dev;

	test_read_conf_embedded(test);
	test_run_parser(test, "grub2");

	ctx = test->ctx;

	check_boot_option_count(ctx, 2);

	opt[0] = get_boot_option(ctx, 0);
	opt[1] = get_boot_option(ctx, 1);

	check_unresolved_resource(opt[0]->boot_image);
	check_unresolved_resource(opt[1]->boot_image);

	dev = test_create_device(ctx, "external");
	dev->uuid = "48c1b787-20ad-47ce-b9eb-b108dddc3535";
	test_hotplug_device(test, dev);

	check_resolved_local_resource(opt[0]->boot_image, dev, "/vmlinux");
	check_resolved_local_resource(opt[1]->boot_image, dev, "/vmlinux");
}
