
#include "parser-test.h"

void run_test(struct parser_test *test)
{
	struct discover_boot_option *opt;
	struct discover_context *ctx;
	struct discover_device *dev;

	ctx = test->ctx;

	dev = test_create_device(test, "bootdev");
	dev->label = "boot";
	device_handler_add_device(test->handler, dev);

	test_read_conf_file(test, "grub2-rhcos-ootpa.conf",
			"/grub/grub.cfg");

	/* add the ignition.firstboot file on the boot-labelled partition,
	 * to check that we can source this correctly */
	test_add_file_string(test, dev,
			"/ignition.firstboot",
			"ignition_extra_kcmdline=meep\n");

	test_run_parser(test, "grub2");

	check_boot_option_count(ctx, 1);

	opt = get_boot_option(ctx, 0);
	check_name(opt,
		"Red Hat Enterprise Linux CoreOS 42.80.20191030.0 (Ootpa) (ostree)");
	check_args(opt, "console=tty0 console=hvc0,115200n8 "
			"rootflags=defaults,prjquota rw "
			"ignition.firstboot rd.neednet=1 ip=dhcp meep "
			"root=UUID=8d8a5c3b-97e6-4d7b-bb87-206af5a9d851 "
			"ostree=/ostree/boot.0/rhcos/6264e4be818e20cf1021bd6e7aa8c76147ce07dec186468c7dfbbc9c5dfc7d8b/0 "
			"ignition.platform.id=openstack");
}
