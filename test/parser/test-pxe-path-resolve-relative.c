
#include "parser-test.h"

#if 0 /* PARSER_EMBEDDED_CONFIG */
label linux
kernel vmlinux
initrd /initrd
#endif

void run_test(struct parser_test *test)
{
	struct discover_boot_option *opt;
	struct discover_context *ctx;

	test_read_conf_embedded_url(test, "tftp://host/path/conf.txt");

	test_set_event_source(test);
	test_set_event_param(test->ctx->event, "siaddr", "host");
	test_set_event_param(test->ctx->event, "pxeconffile", "path/conf.txt");

	test_run_parser(test, "pxe");

	ctx = test->ctx;

	check_boot_option_count(ctx, 1);
	opt = get_boot_option(ctx, 0);

	check_name(opt, "linux");

	/* even though the initrd is specifed as /initrd, pxelinux treats
	 * this as relative. */
	check_resolved_url_resource(opt->boot_image,
			"tftp://host/path/vmlinux");
	check_resolved_url_resource(opt->initrd,
			"tftp://host/path/initrd");
}
