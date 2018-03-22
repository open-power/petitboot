#include "parser-test.h"

#if 0 /* PARSER_EMBEDDED_CONFIG */
blscfg
#endif

void run_test(struct parser_test *test)
{
	struct discover_boot_option *opt;
	struct discover_context *ctx;

	test_add_file_string(test, test->ctx->device,
			     "/loader/entries/6c063c8e48904f2684abde8eea303f41-4.15.2-302.fc28.x86_64.conf",
			     "title Fedora (4.15.2-302.fc28.x86_64) 28 (Twenty Eight)\n"
			     "linux /vmlinuz-4.15.2-302.fc28.x86_64\n"
			     "initrd /initramfs-4.15.2-302.fc28.x86_64.img\n"
			     "options root=/dev/mapper/fedora-root ro rd.lvm.lv=fedora/root\n\n");

	test_add_file_string(test, test->ctx->device,
			     "/loader/entries/6c063c8e48904f2684abde8eea303f41-4.14.18-300.fc28.x86_64.conf",
			     "title Fedora (4.14.18-300.fc28.x86_64) 28 (Twenty Eight)\n"
			     "linux /vmlinuz-4.14.18-300.fc28.x86_64\n"
			     "initrd /initramfs-4.14.18-300.fc28.x86_64.img\n"
			     "options root=/dev/mapper/fedora-root ro rd.lvm.lv=fedora/root\n");

	test_read_conf_embedded(test, "/boot/grub2/grub.cfg");

	test_run_parser(test, "grub2");

	ctx = test->ctx;

	check_boot_option_count(ctx, 2);
	opt = get_boot_option(ctx, 0);

	check_name(opt, "Fedora (4.15.2-302.fc28.x86_64) 28 (Twenty Eight)");
	check_resolved_local_resource(opt->boot_image, ctx->device,
			"/vmlinuz-4.15.2-302.fc28.x86_64");

	opt = get_boot_option(ctx, 1);

	check_name(opt, "Fedora (4.14.18-300.fc28.x86_64) 28 (Twenty Eight)");
	check_resolved_local_resource(opt->initrd, ctx->device,
			"/initramfs-4.14.18-300.fc28.x86_64.img");
}
