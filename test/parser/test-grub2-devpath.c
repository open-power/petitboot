/* check grub2 device+path string parsing */

#include "parser-test.h"

#if 0 /* PARSER_EMBEDDED_CONFIG */

# local
menuentry a {
	linux /vmlinux
}

# local, with an empty device-string
menuentry b {
	linux ()/vmlinux
}

# local, specified by root env var
root=00000000-0000-0000-0000-000000000001
menuentry c {
	linux /vmlinux
}

# remote, specified by root env var
root=00000000-0000-0000-0000-000000000002
menuentry d {
	linux /vmlinux
}

# local, full dev+path spec
menuentry e {
	linux (00000000-0000-0000-0000-000000000001)/vmlinux
}

# remote, full dev+path spec
menuentry f {
	linux (00000000-0000-0000-0000-000000000002)/vmlinux
}

# invalid: incomplete dev+path spec
menuentry g {
	linux (00000000-0000-0000-0000-000000000001
}

# invalid: no path
menuentry h {
	linux (00000000-0000-0000-0000-000000000001)
}


#endif

void run_test(struct parser_test *test)
{
	struct discover_device *dev1, *dev2;
	struct discover_boot_option *opt;
	struct discover_context *ctx;

	ctx = test->ctx;

	/* set local uuid */
	dev1 = test->ctx->device;
	dev1->uuid = "00000000-0000-0000-0000-000000000001";

	dev2 = test_create_device(test, "extdev");
	dev2->uuid = "00000000-0000-0000-0000-000000000002";
	device_handler_add_device(ctx->handler, dev2);

	test_read_conf_embedded(test, "/grub/grub.cfg");

	test_run_parser(test, "grub2");

	check_boot_option_count(ctx, 6);

	opt = get_boot_option(ctx, 0);
	check_name(opt, "a");
	check_resolved_local_resource(opt->boot_image, dev1, "/vmlinux");

	opt = get_boot_option(ctx, 1);
	check_name(opt, "b");
	check_resolved_local_resource(opt->boot_image, dev1, "/vmlinux");

	opt = get_boot_option(ctx, 2);
	check_name(opt, "c");
	check_resolved_local_resource(opt->boot_image, dev1, "/vmlinux");

	opt = get_boot_option(ctx, 3);
	check_name(opt, "d");
	check_resolved_local_resource(opt->boot_image, dev2, "/vmlinux");

	opt = get_boot_option(ctx, 4);
	check_name(opt, "e");
	check_resolved_local_resource(opt->boot_image, dev1, "/vmlinux");

	opt = get_boot_option(ctx, 5);
	check_name(opt, "f");
	check_resolved_local_resource(opt->boot_image, dev2, "/vmlinux");

}
