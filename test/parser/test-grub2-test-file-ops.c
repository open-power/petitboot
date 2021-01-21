
#include "parser-test.h"

#if 0 /* PARSER_EMBEDDED_CONFIG */
status=success

if [ -f /file_that_does_not_exist -a $status = success ]
then status=fail_f_1
fi
if [ -f /dir -a $status = success ]
then status=fail_f_2
fi
if [ ! -f /empty_file -a $status = success ]
then status=fail_f_3
fi
if [ -f "" -a $status = success ]
then status=fail_f_4
fi
if [ -f / -a $status = success ]
then status=fail_f_5
fi

if [ -s /file_that_does_not_exist -a $status = success ]
then status=fail_s_1
fi
if [ ! -s /dir -a $status = success ]
then status=fail_s_2
fi
if [ -s /empty_file -a $status = success ]
then status=fail_s_3
fi
if [ ! -s /non_empty_file -a $status = success ]
then status=fail_s_4
fi
if [ -s "" -a $status = success ]
then status=fail_s_5
fi
if [ ! -s / -a $status = success ]
then status=fail_s_6
fi

if [ -d /file_that_does_not_exist -a $status = success ]
then status=fail_d_1
fi
if [ ! -d /dir -a $status = success ]
then status=fail_d_2
fi
if [ -d /empty_file -a $status = success ]
then status=fail_d_3
fi
if [ -d "" -a $status = success ]
then status=fail_d_4
fi
if [ ! -d / -a $status = success ]
then status=fail_d_5
fi

menuentry $status {
  linux /vmlinux
}
#endif

void run_test(struct parser_test *test)
{
	struct discover_boot_option *opt;
	struct discover_context *ctx;

	ctx = test->ctx;

	test_read_conf_embedded(test, "/grub2/grub.cfg");
	test_add_dir(test, ctx->device, "/");
	test_add_file_data(test, ctx->device, "/empty_file", "", 0);
	test_add_file_data(test, ctx->device, "/non_empty_file", "1", 1);
	test_add_dir(test, ctx->device, "/dir");

	test_run_parser(test, "grub2");

	check_boot_option_count(ctx, 1);
	opt = get_boot_option(ctx, 0);

	check_name(opt, "success");
}
