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
void test_set_conf_source(struct parser_test *test, const char *url);

int test_run_parser(struct parser_test *test, const char *parser_name);

void test_hotplug_device(struct parser_test *test, struct discover_device *dev);

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

/**
 * Check that a boot option @opt has name @name
 */
void __check_name(struct discover_boot_option *opt, const char *name,
		const char *file, int line);
#define check_name(opt, name) \
	__check_name(opt, name, __FILE__, __LINE__)

/**
 * Check that a boot option @opt is marked as default
 */
void __check_is_default(struct discover_boot_option *opt,
		const char *file, int line);
#define check_is_default(opt) \
	__check_is_default(opt, __FILE__, __LINE__)

/**
 * Check that a resource (@res) is present, resolved, and has a local path
 * (within @dev's mount point) of @path.
 */
#define check_resolved_local_resource(res, dev, path) \
	__check_resolved_local_resource(res, dev, path, __FILE__, __LINE__)

void __check_resolved_local_resource(struct resource *res,
		struct discover_device *dev, const char *local_path,
		const char *file, int line);

/**
 * Check that a resource (@res) is present, resolved, and has a URL of
 * @url.
 */
#define check_resolved_url_resource(res, url) \
	__check_resolved_url_resource(res, url, __FILE__, __LINE__)
void __check_resolved_url_resource(struct resource *res,
		const char *url, const char *file, int line);
/**
 * Check that a resource (@res) is present but not resolved
 */
void __check_unresolved_resource(struct resource *res,
		const char *file, int line);
#define check_unresolved_resource(res) \
	__check_unresolved_resource(res, __FILE__, __LINE__)

/**
 * Check that a resource (@res) is not present
 */
void __check_not_present_resource(struct resource *res,
		const char *file, int line);
#define check_not_present_resource(res) \
	__check_not_present_resource(res, __FILE__, __LINE__)

#endif /* PARSER_TEST_H */
