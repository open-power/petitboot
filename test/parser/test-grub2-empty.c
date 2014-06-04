
#include "parser-test.h"

void run_test(struct parser_test *test)
{
	__test_read_conf_data(test, test->ctx->device,
			"/grub2/grub.cfg", "", 0);
	test_run_parser(test, "grub2");
	check_boot_option_count(test->ctx, 0);
}
