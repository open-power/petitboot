/* check grub2 device+path string parsing, as used in scripts */

#include "parser-test.h"

#if 0 /* PARSER_EMBEDDED_CONFIG */

v=

# local device, file present
if [ -f "/1-present" ]; then v=${v}a; fi

# local device, file absent
if [ -f "/1-absent" ]; then v=${v}b; fi;

# local device by UUID, file present
if [ -f "(00000000-0000-0000-0000-000000000001)/1-present" ]; then v=${v}c; fi;

# remote device by UUID, file present
if [ -f "(00000000-0000-0000-0000-000000000002)/2-present" ]; then v=${v}d; fi;

# non-existent device
if [ -f "(00000000-0000-0000-0000-000000000003)/present" ]; then v=${v}e; fi;

menuentry $v {
	linux /vmlinux
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

	test_add_file_data(test, dev1, "/1-present", "x", 1);
	test_add_file_data(test, dev2, "/2-present", "x", 1);

	test_read_conf_embedded(test, "/grub/grub.cfg");

	test_run_parser(test, "grub2");

	check_boot_option_count(ctx, 1);
	opt = get_boot_option(ctx, 0);
	check_name(opt, "acd");
}
