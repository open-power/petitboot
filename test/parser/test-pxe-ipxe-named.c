#include "parser-test.h"

#if 0 /* PARSER_EMBEDDED_CONFIG */
#!ipxe
kernel vmlinux --name test-option append kernel args
initrd initrd
#endif

/**
 * Test that we recognise an ipxe-formatted script obtained from bootfile_url
 * (DHCPv6 option 59) that some vendors use, and that we correctly parse the
 * --name parameter from the kernel arguments.
 */

void run_test(struct parser_test *test)
{
	struct discover_boot_option *opt;
	struct discover_context *ctx;

	test_read_conf_embedded_url(test, "tftp://host/dir1/conf");

	test_set_event_source(test);
	test_set_event_param(test->ctx->event, "bootfile_url", "tftp://host/dir1/conf");

	test_run_parser(test, "pxe");

	ctx = test->ctx;

	check_boot_option_count(ctx, 1);
	opt = get_boot_option(ctx, 0);

	check_name(opt, "test-option");

	check_resolved_url_resource(opt->boot_image,
			"tftp://host/dir1/vmlinux");
	check_resolved_url_resource(opt->initrd,
			"tftp://host/dir1/initrd");
	check_args(opt, "append kernel args");
}
