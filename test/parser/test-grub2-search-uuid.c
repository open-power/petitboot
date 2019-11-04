/* check for grub2 search command, searching by FS UUID */

#include "parser-test.h"

#if 0 /* PARSER_EMBEDDED_CONFIG */

# valid UUID
search --set=v1 --fs-uuid ee0cc6fa-1dba-48f2-8f5b-19e4b8de8c37

# invalid UUID: will fall back to passing the UUID through
search --set=v2 --fs-uuid 92b0da57-6e04-4e54-960b-85e6bb060433

# no 'type' argument defaults to UUID search
search --set=v3 ee0cc6fa-1dba-48f2-8f5b-19e4b8de8c37

menuentry $v1 {
    linux /vmlinux
}

menuentry $v2 {
    linux /vmlinux
}

menuentry $v3 {
    linux /vmlinux
}
#endif

void run_test(struct parser_test *test)
{
	struct discover_boot_option *opt;
	struct discover_context *ctx;
	struct discover_device *dev;

	ctx = test->ctx;

	dev = test_create_device(test, "testdev");
	dev->uuid = "ee0cc6fa-1dba-48f2-8f5b-19e4b8de8c37";
	device_handler_add_device(test->handler, dev);

	test_read_conf_embedded(test, "/grub/grub.cfg");

	test_run_parser(test, "grub2");

	check_boot_option_count(ctx, 3);

	opt = get_boot_option(ctx, 0);
	check_name(opt, dev->device->id);

	opt = get_boot_option(ctx, 1);
	check_name(opt, "92b0da57-6e04-4e54-960b-85e6bb060433");

	opt = get_boot_option(ctx, 2);
	check_name(opt, dev->device->id);
}
