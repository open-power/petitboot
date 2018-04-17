#include "parser-test.h"

#if 0 /* PARSER_EMBEDDED_CONFIG */
set default=2
menuentry 'title Fedora (4.15-9-304.fc28.x86_64) 28 (Twenty Eight)' {
	linux   /vmlinuz-4.15-9-301.fc28.x86_64
}

menuentry 'title Fedora (4.15.6-300.fc28.x86_64) 28 (Twenty Eight)' {
	linux   /vmlinuz-4.15.6-300.fc28.x86_64
}
blscfg
#endif

void run_test(struct parser_test *test)
{
	struct discover_boot_option *opt;
	struct discover_context *ctx;

	test_add_file_string(test, test->ctx->device,
			     "/loader/entries/6c063c8e48904f2684abde8eea303f41-4.15.2-300.fc28.x86_64.conf",
			     "title Fedora (4.15.2-300.fc28.x86_64) 28 (Twenty Eight)\n"
			     "linux /vmlinuz-4.15.2-300.fc28.x86_64\n"
			     "initrd /initramfs-4.15.2-300.fc28.x86_64.img\n"
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

	opt = get_boot_option(ctx, 2);

	check_name(opt, "Fedora (4.15.2-300.fc28.x86_64) 28 (Twenty Eight)");

	check_is_default(opt);
}
