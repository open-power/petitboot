
/* check that have a maximum source recursion limit */

#include "parser-test.h"

#if 0 /* PARSER_EMBEDDED_CONFIG */

name=a$name

menuentry $name {
	linux /a
}

source /grub/grub.cfg

#endif

void run_test(struct parser_test *test)
{
	struct discover_boot_option *opt;
	struct discover_context *ctx;
	struct discover_device *dev;

	ctx = test->ctx;
	dev = ctx->device;

	test_read_conf_embedded(test, "/grub/grub.cfg");

	test_run_parser(test, "grub2");

	/* we error out after 10 levels, but we should still have
	 * parse results up to that point
	 */
	check_boot_option_count(ctx, 11);

	opt = get_boot_option(ctx, 0);
	check_name(opt, "a");
	check_resolved_local_resource(opt->boot_image, dev, "/a");

	opt = get_boot_option(ctx,10);
	check_name(opt, "aaaaaaaaaaa");
	check_resolved_local_resource(opt->boot_image, dev, "/a");
}
