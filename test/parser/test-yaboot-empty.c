#include "parser-test.h"

static const char empty[] = "";

void run_test(struct parser_test *test)
{
	test_read_conf_data(test, "/etc/yaboot.conf", empty);

	test_run_parser(test, "yaboot");

	check_boot_option_count(test->ctx, 0);
}
