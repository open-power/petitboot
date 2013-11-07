#include "parser-test.h"

#if 0 /* PARSER_EMBEDDED_CONFIG */
menuentry 'Linux' {
	linux   /vmlinux
	initrd  /initrd
}
#endif

/* check that the PXE parser won't break on a local device */
void run_test(struct parser_test *test)
{
	test_read_conf_embedded(test, "/grub2/grub.cfg");

	test_run_parser(test, "pxe");

	check_boot_option_count(test->ctx, 0);
}
