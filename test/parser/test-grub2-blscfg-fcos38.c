#include "parser-test.h"

#if 0 /* PARSER_EMBEDDED_CONFIG */
blscfg
#endif

void run_test(struct parser_test *test)
{
	struct discover_context *ctx;

	ctx = test->ctx;

	test_add_dir(test, ctx->device, "/loader/entries");

	test_read_conf_file(test, "grub2-blscfg-fcos38.conf",
			    "/loader/entries/ostree-1-fedora-coreos.conf");

	test_read_conf_embedded(test, "/grub2/grub.cfg");

	test_run_parser(test, "grub2");

	check_boot_option_count(ctx, 1);
}
