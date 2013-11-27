
#include "parser-test.h"

#if 0 /* PARSER_EMBEDDED_CONFIG */
if [ -s $prefix/grubenv ]; then
  load_env
fi
if [ "${next_entry}" ] ; then
   set default="${next_entry}"
   set next_entry=
   save_env next_entry
   set boot_once=true
else
   set default="${saved_entry}"
fi
menuentry 'test saved option' {
	linux   vmlinux
}
#endif



void run_test(struct parser_test *test)
{
	struct discover_boot_option *opt;
	struct discover_context *ctx;

	test_add_file_string(test, test->ctx->device,
				"/boot/grub/grubenv",
				"# GRUB Environment Block\n"
				"saved_entry=test saved option\n"
				"#############################");

	test_read_conf_embedded(test, "/boot/grub/grub.cfg");

	test_run_parser(test, "grub2");

	ctx = test->ctx;

	check_boot_option_count(ctx, 1);
	opt = get_boot_option(ctx, 0);

	check_name(opt, "test saved option");
	check_resolved_local_resource(opt->boot_image, ctx->device,
			"/vmlinux");
	check_is_default(opt);
}
