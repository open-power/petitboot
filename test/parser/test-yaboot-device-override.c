#include "parser-test.h"

#include <talloc/talloc.h>

#if 0 /* PARSER_EMBEDDED_CONFIG */
default=
device=/dev/sda1

image=/vmlinux.1
	label=linux.1
	initrd=initrd.1

image=/vmlinux.2
	device=/dev/sda2
	label=linux.2
	initrd=initrd.2

image=sda3:/vmlinux.3
	device=/dev/sda2
	label=linux.3
	initrd=sda3:initrd.3

image=sda4:/vmlinux.4
	device=/dev/sda3
	label=linux.4
	initrd=initrd.4
#endif

void run_test(struct parser_test *test)
{
	struct discover_boot_option *opt[4];
	struct discover_device *dev[4];
	struct discover_context *ctx;
	char *devname;
	int i;

	test_read_conf_embedded(test, "/etc/yaboot.conf");

	test_run_parser(test, "yaboot");

	ctx = test->ctx;

	check_boot_option_count(ctx, 4);

	for (i = 0; i < 4; i++)
		opt[i] = get_boot_option(ctx, i);

	check_name(opt[0], "linux.1");
	check_unresolved_resource(opt[0]->boot_image);
	check_unresolved_resource(opt[0]->initrd);

	check_name(opt[1], "linux.2");
	check_unresolved_resource(opt[1]->boot_image);
	check_unresolved_resource(opt[1]->initrd);

	check_name(opt[2], "linux.3");
	check_unresolved_resource(opt[2]->boot_image);
	check_unresolved_resource(opt[2]->initrd);

	check_name(opt[3], "linux.4");
	check_unresolved_resource(opt[3]->boot_image);
	check_unresolved_resource(opt[3]->initrd);

	/* hotplug all dependent devices */
	for (i = 0; i < 4; i++) {
		devname = talloc_asprintf(test, "sda%d", i + 1);
		dev[i] = test_create_device(test, devname);
		test_hotplug_device(test, dev[i]);
	}

	check_resolved_local_resource(opt[0]->boot_image, dev[0], "/vmlinux.1");
	check_resolved_local_resource(opt[1]->boot_image, dev[1], "/vmlinux.2");
	check_resolved_local_resource(opt[2]->boot_image, dev[2], "/vmlinux.3");
	check_resolved_local_resource(opt[3]->boot_image, dev[3], "/vmlinux.4");

	check_resolved_local_resource(opt[0]->initrd, dev[0], "/initrd.1");
	check_resolved_local_resource(opt[1]->initrd, dev[1], "/initrd.2");
	check_resolved_local_resource(opt[2]->initrd, dev[2], "/initrd.3");
	check_resolved_local_resource(opt[3]->initrd, dev[2], "/initrd.4");
}
