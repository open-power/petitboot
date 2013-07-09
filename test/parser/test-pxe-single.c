
#include "parser-test.h"

#if 0 /* PARSER_EMBEDDED_CONFIG */
default linux

label linux
kernel ./pxe/de-ad-de-ad-be-ef.vmlinuz
append command line
initrd=./pxe/de-ad-de-ad-be-ef.initrd
#endif

void run_test(struct parser_test *test)
{
	struct discover_boot_option *opt;
	struct discover_context *ctx;

	test_read_conf_embedded(test);
	test_set_conf_source(test, "tftp://host/dir/conf.txt");
	test_run_parser(test, "pxe");

	ctx = test->ctx;

	check_boot_option_count(ctx, 1);
	opt = get_boot_option(ctx, 0);

	check_name(opt, "linux");
	check_args(opt, "command line");

	check_resolved_url_resource(opt->boot_image,
			"tftp://host/dir/./pxe/de-ad-de-ad-be-ef.vmlinuz");
	check_resolved_url_resource(opt->initrd,
			"tftp://host/dir/./pxe/de-ad-de-ad-be-ef.initrd");
}
