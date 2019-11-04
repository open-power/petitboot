/* check for grub2 search command, searching by partition label */

#include "parser-test.h"

#if 0 /* PARSER_EMBEDDED_CONFIG */

# valid label
search --set=v1 --label testlabel

v2=prev
# invalid label: does not alter v2
search --set=v2 --label invalidlabel

menuentry $v1 {
    linux /vmlinux
}

menuentry $v2 {
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
	dev->label = "testlabel";
	device_handler_add_device(test->handler, dev);

	test_read_conf_embedded(test, "/grub/grub.cfg");

	test_run_parser(test, "grub2");

	check_boot_option_count(ctx, 2);

	opt = get_boot_option(ctx, 0);
	check_name(opt, "testdev");

	opt = get_boot_option(ctx, 1);
	check_name(opt, "prev");
}
