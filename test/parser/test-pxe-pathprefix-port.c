
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

	test_read_conf_embedded_url(test,
			"http://host:8080/path/to/pxelinux.cfg/default");

	test_set_event_source(test);
	test_set_event_param(test->ctx->event, "ip", "192.168.0.1");
	test_set_event_param(test->ctx->event, "pxepathprefix",
			"http://host:8080/path/to/");

	test_run_parser(test, "pxe");

	ctx = test->ctx;

	check_boot_option_count(ctx, 1);
	opt = get_boot_option(ctx, 0);

	check_name(opt, "linux");
	check_args(opt, "command line");

	check_resolved_url_resource(opt->boot_image,
			"http://host:8080/path/to/./kernel");
	check_resolved_url_resource(opt->initrd,
			"http://host:8080/path/to/initrd");
}
