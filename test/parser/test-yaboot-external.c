
#include "parser-test.h"

#if 0 /* PARSER_EMBEDDED_CONFIG */
default=

image=external:/vmlinux
	label=linux
	initrd=external:/initrd
#endif

void run_test(struct parser_test *test)
{
	struct discover_boot_option *opt;
	struct discover_context *ctx;
	struct discover_device *dev;

	test_read_conf_embedded(test, "/yaboot.conf");

	test_run_parser(test, "yaboot");

	ctx = test->ctx;

	check_boot_option_count(ctx, 1);

	opt = get_boot_option(ctx, 0);

	check_name(opt, "linux");
	check_unresolved_resource(opt->boot_image);
	check_unresolved_resource(opt->initrd);

	dev = test_create_device(test, "external");
	test_hotplug_device(test, dev);

	check_resolved_local_resource(opt->boot_image, dev, "/vmlinux");
	check_resolved_local_resource(opt->initrd, dev, "/initrd");
}
