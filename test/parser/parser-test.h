#ifndef PARSER_TEST_H
#define PARSER_TEST_H

#include <stdlib.h>

#include "device-handler.h"
#include "resource.h"

struct parser_test {
	struct device_handler *handler;
	struct discover_context *ctx;
	struct {
		void	*buf;
		size_t	size;
	} conf;
};

/* interface required for parsers */
void __register_parser(struct parser *parser);

/* test functions */
struct discover_device *test_create_device(struct discover_context *ctx,
		const char *name);

#define test_read_conf_data(t, d) \
	__test_read_conf_data(t, d, sizeof(d))

void __test_read_conf_data(struct parser_test *test,
		const char *buf, size_t len);
void test_read_conf_file(struct parser_test *test, const char *filename);

int test_run_parser(struct parser_test *test, const char *parser_name);

#endif /* PARSER_TEST_H */
