
#include "parser-test.h"

void run_test(struct parser_test *test)
{
	struct discover_boot_option *opt;
	struct discover_context *ctx;
	struct discover_device *dev;

	test_read_conf_file(test, "grub2-ubuntu-13_04-x86.conf");
	test_run_parser(test, "grub2");

	ctx = test->ctx;

	check_boot_option_count(ctx, 5);

	opt = get_boot_option(ctx, 0);
	check_unresolved_resource(opt->boot_image);
	check_unresolved_resource(opt->initrd);
	check_name(opt, "Kubuntu GNU/Linux");
	check_args(opt, "root=UUID=29beca39-9181-4780-bbb2-ab5d4be59aaf ro   quiet splash $vt_handoff");

	opt = get_boot_option(ctx, 1);
	check_unresolved_resource(opt->boot_image);
	check_unresolved_resource(opt->initrd);
	check_name(opt, "Kubuntu GNU/Linux, with Linux 3.8.0-19-generic");
	check_args(opt, "root=UUID=29beca39-9181-4780-bbb2-ab5d4be59aaf ro   quiet splash $vt_handoff");

	opt = get_boot_option(ctx, 2);
	check_name(opt, "Kubuntu GNU/Linux, with Linux 3.8.0-19-generic (recovery mode)");
	check_args(opt, "root=UUID=29beca39-9181-4780-bbb2-ab5d4be59aaf ro recovery nomodeset");

	opt = get_boot_option(ctx, 3);
	check_unresolved_resource(opt->boot_image);
	check_not_present_resource(opt->initrd);
	check_name(opt, "Memory test (memtest86+)");
	check_args(opt, "");

	opt = get_boot_option(ctx, 4);
	check_unresolved_resource(opt->boot_image);
	check_not_present_resource(opt->initrd);
	check_name(opt, "Memory test (memtest86+, serial console 115200)");
	check_args(opt, "console=ttyS0,115200n8");

	/* hotplug a device with a maching UUID, and check that our
	 * resources become resolved */
	dev = test_create_device(test, "external");
	dev->uuid = "29beca39-9181-4780-bbb2-ab5d4be59aaf";
	test_hotplug_device(test, dev);

	opt = get_boot_option(ctx, 0);
	check_resolved_local_resource(opt->boot_image, dev,
		"/boot/vmlinuz-3.8.0-19-generic");
	check_resolved_local_resource(opt->initrd, dev,
		"/boot/initrd.img-3.8.0-19-generic");

	opt = get_boot_option(ctx, 1);
	check_resolved_local_resource(opt->boot_image, dev,
		"/boot/vmlinuz-3.8.0-19-generic");
	check_resolved_local_resource(opt->initrd, dev,
		"/boot/initrd.img-3.8.0-19-generic");

	opt = get_boot_option(ctx, 2);
	check_resolved_local_resource(opt->boot_image, dev,
		"/boot/vmlinuz-3.8.0-19-generic");
	check_resolved_local_resource(opt->initrd, dev,
		"/boot/initrd.img-3.8.0-19-generic");

	opt = get_boot_option(ctx, 3);
	check_resolved_local_resource(opt->boot_image, dev,
		"/boot/memtest86+.bin");
	check_not_present_resource(opt->initrd);

	opt = get_boot_option(ctx, 4);
	check_resolved_local_resource(opt->boot_image, dev,
		"/boot/memtest86+.bin");
	check_not_present_resource(opt->initrd);
}
