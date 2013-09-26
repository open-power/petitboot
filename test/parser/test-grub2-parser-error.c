
#include "parser-test.h"

#if 0 /* PARSER_EMBEDDED_CONFIG */
{
#endif

void run_test(struct parser_test *test)
{
	test_read_conf_embedded(test);
	test_run_parser(test, "grub2");
	check_boot_option_count(test->ctx, 0);
}
