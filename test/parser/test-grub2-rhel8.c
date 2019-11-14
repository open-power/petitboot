
#include "parser-test.h"

void run_test(struct parser_test *test)
{
	struct discover_context *ctx;
	struct discover_device *dev;

	ctx = test->ctx;

	dev = test_create_device(test, "boot");
	dev->uuid = "9a8ea027-4829-45b9-829b-18ed6cc1f33b";
	device_handler_add_device(test->handler, dev);

	test_read_conf_file(test, "grub2-rhel8.conf",
			"/grub/grub.cfg");

	test_run_parser(test, "grub2");

	check_boot_option_count(ctx, 3);
}
