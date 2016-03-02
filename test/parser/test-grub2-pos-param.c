
#include "parser-test.h"

#if 0 /* PARSER_EMBEDDED_CONFIG */

function set_params_var {
	params="$1 $2 $10"
}
menuentry 'Linux' {
	set_params_var abc 123 3 4 5 6 7 8 9 bingo
	linux   test_kernel $params
}

#endif

void run_test(struct parser_test *test)
{
	struct discover_boot_option *opt;
	struct discover_context *ctx;

	test_read_conf_embedded(test, "/boot/grub/grub.cfg");

	test_run_parser(test, "grub2");

	ctx = test->ctx;

	check_boot_option_count(ctx, 1);
	opt = get_boot_option(ctx, 0);

	check_name(opt, "Linux");
	/* The linux command, $params is expected to have been set when
	 * set_params was called in menuentry.
	 */
	check_args(opt, "abc 123 bingo");
}
