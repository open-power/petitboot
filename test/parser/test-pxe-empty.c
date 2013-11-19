
#include "parser-test.h"

#if 0 /* PARSER_EMBEDDED_CONFIG */

#endif

void run_test(struct parser_test *test)
{
	struct discover_context *ctx;

	test_read_conf_embedded(test, "conf.txt");

	test_set_event_source(test);
	test_set_event_param(test->ctx->event, "pxeconffile",
			"tftp://host/dir/conf.txt");

	test_run_parser(test, "pxe");

	ctx = test->ctx;

	check_boot_option_count(ctx, 0);
}
