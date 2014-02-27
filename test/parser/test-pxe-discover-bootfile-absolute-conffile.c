
#include "parser-test.h"

#if 0 /* PARSER_EMBEDDED_CONFIG */
label linux
kernel vmlinux
initrd initrd
#endif

/**
 * Test that a pxeconffile option (DHCP opt 209) takes precedence over
 * configuration discovery, and is resolved as an absolute path, overriding
 * any prefix in bootfile.
 */

void run_test(struct parser_test *test)
{
	struct discover_boot_option *opt;
	struct discover_context *ctx;

	/* fixme: we should have a normalised URL here */
	test_read_conf_embedded_url(test, "tftp://host//dir1/conf");

	test_set_event_source(test);
	test_set_event_param(test->ctx->event, "bootfile", "dir2/binary");
	test_set_event_param(test->ctx->event, "pxeconffile", "/dir1/conf");
	test_set_event_param(test->ctx->event, "tftp", "host");

	test_run_parser(test, "pxe");

	ctx = test->ctx;

	check_boot_option_count(ctx, 1);
	opt = get_boot_option(ctx, 0);

	check_name(opt, "linux");

	check_resolved_url_resource(opt->boot_image,
			"tftp://host/dir1/vmlinux");
	check_resolved_url_resource(opt->initrd,
			"tftp://host/dir1/initrd");
}
