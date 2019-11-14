/* check that we can source other files recursively */

#include "parser-test.h"

#if 0 /* PARSER_EMBEDDED_CONFIG */

menuentry a {
	linux /a
}

source /grub/2.cfg

menuentry c {
	linux /c
}

#endif

void run_test(struct parser_test *test)
{
	struct discover_boot_option *opt;
	struct discover_context *ctx;
	struct discover_device *dev;

	ctx = test->ctx;
	dev = ctx->device;

	test_read_conf_embedded(test, "/grub/grub.cfg");

	/* four levels of config files, the last defining a boot option */
	test_add_file_string(test, dev,
			"/grub/2.cfg",
			"source /grub/3.cfg\n");

	test_add_file_string(test, dev,
			"/grub/3.cfg",
			"source /grub/4.cfg\n");

	test_add_file_string(test, dev,
			"/grub/4.cfg",
			"menuentry b { linux /b }\n");

	test_run_parser(test, "grub2");

	check_boot_option_count(ctx, 3);

	opt = get_boot_option(ctx, 0);
	check_name(opt, "a");
	check_resolved_local_resource(opt->boot_image, dev, "/a");

	opt = get_boot_option(ctx, 1);
	check_name(opt, "b");
	check_resolved_local_resource(opt->boot_image, dev, "/b");

	opt = get_boot_option(ctx, 2);
	check_name(opt, "c");
	check_resolved_local_resource(opt->boot_image, dev, "/c");
}
