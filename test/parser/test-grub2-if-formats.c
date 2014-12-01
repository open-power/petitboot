
#include "parser-test.h"

#if 0 /* PARSER_EMBEDDED_CONFIG */
if true;then t1=1;fi
if true ;then t2=2;fi
if true;then t3=3 ;fi
if true;then t4=4; ;fi
if true
then t5=5
fi
if true
then t6=6;
fi
if true
 then t7=7
fi
if true
then t8=8; fi
if true
then
t9=9

fi

menuentry $t1$t2$t3$t4$t5$t6$t7$t8$t9 {linux /vmlinux}
#endif

void run_test(struct parser_test *test)
{
	struct discover_boot_option *opt;

	test_read_conf_embedded(test, "/grub2/grub.cfg");

	test_run_parser(test, "grub2");

	check_boot_option_count(test->ctx, 1);
	opt = get_boot_option(test->ctx, 0);
	check_name(opt, "123456789");
}
