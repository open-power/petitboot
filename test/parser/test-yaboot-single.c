
#include "parser-test.h"

#if 0 /* PARSER_EMBEDDED_CONFIG */
default=linux

image=/vmlinux
	label=linux
	initrd=/initrd
#endif

void run_test(struct parser_test *test)
{
	struct discover_boot_option *opt;
	struct discover_context *ctx;

	test_read_conf_embedded(test, "/etc/yaboot.conf");

	test_run_parser(test, "yaboot");

	ctx = test->ctx;

	check_boot_option_count(ctx, 1);

	opt = get_boot_option(ctx, 0);

	check_name(opt, "linux");
	check_resolved_local_resource(opt->boot_image, ctx->device, "/vmlinux");
	check_resolved_local_resource(opt->initrd, ctx->device, "/initrd");
}
