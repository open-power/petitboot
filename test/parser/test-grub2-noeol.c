
#include "parser-test.h"

void run_test(struct parser_test *test)
{
	const char data[] = "true";

	test_add_file_data(test, test->ctx->device, "/boot/grub/grubenv",
			data, sizeof(data));

	__test_read_conf_data(test, test->ctx->device,
			"/boot/grub/grub.cfg", data, sizeof(data));

	test_run_parser(test, "grub2");
}
