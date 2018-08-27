
#include "parser-test.h"

static const char config[] =
	"default=linux\n"
	"linux='/vmlinux initrd=/initrd arg1=value1 arg2'\n"
	"hdd='/vmlinux initrd=/initrd'\n";

void run_test(struct parser_test *test)
{
	struct discover_boot_option *opt;
	struct discover_context *ctx;

	test_read_conf_data(test, "/kboot.conf", config);

	test_run_parser(test, "kboot");

	ctx = test->ctx;

	check_boot_option_count(ctx, 2);
	opt = get_boot_option(ctx, 0);

	check_name(opt, "linux");
	check_resolved_local_resource(opt->boot_image, ctx->device, "/vmlinux");
	check_resolved_local_resource(opt->initrd, ctx->device, "/initrd");

	check_args(opt, "arg1=value1 arg2");

	check_is_default(opt);
}
