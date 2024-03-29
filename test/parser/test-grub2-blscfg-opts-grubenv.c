#include "parser-test.h"

#if 0 /* PARSER_EMBEDDED_CONFIG */
load_env
blscfg
#endif

void run_test(struct parser_test *test)
{
	struct discover_boot_option *opt;
	struct discover_context *ctx;

	test_add_dir(test, test->ctx->device, "/loader/entries");

	test_add_file_string(test, test->ctx->device,
			     "/boot/grub2/grubenv",
			     "# GRUB Environment Block\n"
			     "kernelopts=root=/dev/mapper/fedora-root ro rd.lvm.lv=fedora/root\n");

	test_add_file_string(test, test->ctx->device,
			     "/loader/entries/6c063c8e48904f2684abde8eea303f41-4.15.2-302.fc28.x86_64.conf",
			     "title Fedora (4.15.2-302.fc28.x86_64) 28 (Twenty Eight)\n"
			     "linux /vmlinuz-4.15.2-302.fc28.x86_64\n"
			     "initrd $blank /initramfs-4.15.2-302.fc28.x86_64.img $blank\n"
			     "options $kernelopts debug\n");

	test_read_conf_embedded(test, "/boot/grub2/grub.cfg");

	test_run_parser(test, "grub2");

	ctx = test->ctx;

	opt = get_boot_option(ctx, 0);

	check_args(opt, "root=/dev/mapper/fedora-root ro rd.lvm.lv=fedora/root debug");
	check_resolved_local_resource(opt->initrd, ctx->device, "/initramfs-4.15.2-302.fc28.x86_64.img");
}
