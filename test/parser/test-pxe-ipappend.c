
#include "parser-test.h"
#include "network.h"

#if 0 /* PARSER_EMBEDDED_CONFIG */
default linux

label linux
kernel ./pxe/de-ad-de-ad-be-ef.vmlinuz
append command line
ipappend 2
#endif

void run_test(struct parser_test *test)
{
	struct discover_boot_option *opt;
	struct discover_context *ctx;

	test_read_conf_embedded_url(test, "tftp://host/dir/conf.txt");

	test_set_event_source(test);
	test_set_event_param(test->ctx->event, "pxeconffile",
			"tftp://host/dir/conf.txt");
	test_set_event_device(test->ctx->event, "em1");

	test_run_parser(test, "pxe");

	ctx = test->ctx;

	check_boot_option_count(ctx, 1);
	opt = get_boot_option(ctx, 0);

	check_name(opt, "linux");
	check_args(opt, "command line BOOTIF=01-01-02-03-04-05-06");
}
