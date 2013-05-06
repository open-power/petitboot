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

struct discover_boot_option *get_boot_option(struct discover_context *ctx,
		int idx);

/* embedded config */
extern const char __embedded_config[];
extern const size_t __embedded_config_size;
#define test_read_conf_embedded(t) \
	__test_read_conf_data(t, __embedded_config, __embedded_config_size)

/**
 * Checks for parser results.
 *
 * These return void, but will respond to check failures by printing a reason
 * for the failure, and exit the test with a non-zero exit status.
 */

/**
 * Check that we have an expected number of boot options parsed. If not,
 * print out what we did find, then exit.
 */
#define check_boot_option_count(ctx, count) \
	__check_boot_option_count(ctx, count, __FILE__, __LINE__)
void __check_boot_option_count(struct discover_context *ctx, int count,
		const char *file, int line);
/*
 * Check that a boot option @opt has args @args
 */
void __check_args(struct discover_boot_option *opt, const char *args,
		const char *file, int line);
#define check_args(opt, args) \
	__check_args(opt, args, __FILE__, __LINE__)

#endif /* PARSER_TEST_H */
