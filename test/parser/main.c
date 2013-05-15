
#include <talloc/talloc.h>

#include "parser-test.h"

extern struct parser_test *test_init(void);
extern void test_fini(struct parser_test *test);
extern void run_test(struct parser_test *test);

int main(void)
{
	struct parser_test *test;

	test = test_init();

	run_test(test);

	test_fini(test);

	return EXIT_SUCCESS;
}
