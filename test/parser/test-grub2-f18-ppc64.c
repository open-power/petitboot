
#include "parser-test.h"

void run_test(struct parser_test *test)
{
	struct discover_boot_option *opt;
	struct discover_context *ctx;
	int i;

	test_read_conf_file(test, "grub2-f18-ppc64.conf");
	test_run_parser(test, "grub2");

	ctx = test->ctx;

	check_boot_option_count(ctx, 2);

	for (i = 0; i < 2; i++) {
		opt = get_boot_option(ctx, i);

		check_resolved_local_resource(opt->boot_image, ctx->device,
				"/vmlinuz-3.6.10-4.fc18.ppc64p7");
		check_resolved_local_resource(opt->initrd, ctx->device,
				"/initramfs-3.6.10-4.fc18.ppc64p7.img");

		check_args(opt, "root=/dev/mapper/fedora_ltcfbl8eb-root ro "
				"rd.lvm.lv=fedora_ltcfbl8eb/swap rd.dm=0 "
				"rd.lvm.lv=fedora_ltcfbl8eb/root  rd.md=0 "
				"rd.luks=0 vconsole.keymap=us rhgb quiet");

		check_name(opt, i == 0 ?
				"Fedora" :
				"Fedora, with Linux 3.6.10-4.fc18.ppc64p7");
	}
}
