
#include "parser-test.h"

#if 0 /* PARSER_EMBEDDED_CONFIG */
default linux

label linux
kernel ./kernel
append command line
initrd /initrd
#endif

void run_test(struct parser_test *test)
{
	struct discover_boot_option *opt;
	struct discover_context *ctx;

	test_read_conf_embedded_url(test, "tftp://host/path/to/conf.txt");

	test_set_event_source(test);
	test_set_event_param(test->ctx->event, "tftp", "host");
	test_set_event_param(test->ctx->event, "pxepathprefix", "/path/to/");
	test_set_event_param(test->ctx->event, "pxeconffile", "conf.txt");

	test_run_parser(test, "pxe");

	ctx = test->ctx;

	check_boot_option_count(ctx, 1);
	opt = get_boot_option(ctx, 0);

	check_name(opt, "linux");
	check_args(opt, "command line");

	check_resolved_url_resource(opt->boot_image,
			"tftp://host/path/to/./kernel");
	check_resolved_url_resource(opt->initrd, "tftp://host/initrd");
}
