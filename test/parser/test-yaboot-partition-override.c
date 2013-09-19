#include "parser-test.h"

#if 0 /* PARSER_EMBEDDED_CONFIG */
device=sda1
partition=2

image=/vmlinux
	label=linux
#endif

void run_test(struct parser_test *test)
{
	struct discover_boot_option *opt;
	struct discover_context *ctx;
	struct discover_device *dev;

	test_read_conf_embedded(test);
	test_run_parser(test, "yaboot");

	ctx = test->ctx;

	check_boot_option_count(ctx, 1);

	opt = get_boot_option(ctx, 0);

	check_name(opt, "linux");
	check_unresolved_resource(opt->boot_image);

	dev = test_create_device(test, "sda2");
	test_hotplug_device(test, dev);

	check_resolved_local_resource(opt->boot_image, dev, "/vmlinux");
}
