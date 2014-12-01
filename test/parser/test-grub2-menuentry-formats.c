
#include "parser-test.h"

#if 0 /* PARSER_EMBEDDED_CONFIG */
menuentry "test.0" {linux /vmlinux;}
menuentry "test.1" {linux /vmlinux}
menuentry "test.2" {linux /vmlinux }
menuentry "test.3" { linux /vmlinux; }
menuentry "test.4" {linux /vmlinux ;}
menuentry "test.5" {
linux /vmlinux;}
menuentry "test.6" {linux /vmlinux
}
menuentry "test.7" {
linux /vmlinux
}
menuentry "test.8" {
 linux /vmlinux
}
menuentry "test.9" {
 linux /vmlinux
 }
#endif

void run_test(struct parser_test *test)
{
	struct discover_boot_option *opt;
	char str[] = "test.0";
	int i;

	test_read_conf_embedded(test, "/grub2/grub.cfg");

	test_run_parser(test, "grub2");

	check_boot_option_count(test->ctx, 10);
	for (i = 0; i < 8; i++) {
		opt = get_boot_option(test->ctx, i);
		str[5] = i + '0';
		check_name(opt, str);
	}
}
