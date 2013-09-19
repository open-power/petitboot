
#include <assert.h>
#include <err.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <talloc/talloc.h>
#include <types/types.h>
#include <url/url.h>

#include "device-handler.h"
#include "parser.h"
#include "resource.h"

#include "parser-test.h"

struct p_item {
	struct list_item list;
	struct parser *parser;
};

STATIC_LIST(parsers);

void __register_parser(struct parser *parser)
{
	struct p_item* i = talloc(NULL, struct p_item);

	i->parser = parser;
	list_add(&parsers, &i->list);
}

static void __attribute__((destructor)) __cleanup_parsers(void)
{
	struct p_item *item, *tmp;

	list_for_each_entry_safe(&parsers, item, tmp, list)
		talloc_free(item);
}

static struct discover_device *test_create_device_simple(
		struct parser_test *test)
{
	static int dev_idx;
	char name[10];

	sprintf(name, "__test%d", dev_idx++);

	return test_create_device(test, name);
}

struct discover_device *test_create_device(struct parser_test *test,
		const char *name)
{
	struct discover_device *dev;

	dev = discover_device_create(test->handler, name);

	dev->device->id = talloc_strdup(dev, name);
	dev->device_path = talloc_asprintf(dev, "/dev/%s", name);
	dev->mount_path = talloc_asprintf(dev, "/test/mount/%s", name);

	return dev;
}

static struct discover_context *test_create_context(struct parser_test *test)
{
	struct discover_context *ctx;

	ctx = talloc_zero(test, struct discover_context);
	assert(ctx);

	list_init(&ctx->boot_options);
	ctx->device = test_create_device_simple(test);
	device_handler_add_device(test->handler, ctx->device);

	return ctx;
}

extern struct config *test_config_init(struct parser_test *test);

struct parser_test *test_init(void)
{
	struct parser_test *test;

	test = talloc_zero(NULL, struct parser_test);
	test->config = test_config_init(test);
	test->handler = device_handler_init(NULL, NULL, 0);
	test->ctx = test_create_context(test);

	return test;
}

void test_fini(struct parser_test *test)
{
	device_handler_destroy(test->handler);
	talloc_free(test);
}

void __test_read_conf_data(struct parser_test *test,
		const char *buf, size_t len)
{
	test->conf.size = len;
	test->conf.buf = talloc_memdup(test, buf, len);
}

void test_read_conf_file(struct parser_test *test, const char *filename)
{
	struct stat stat;
	char *path;
	int fd, rc;

	path = talloc_asprintf(test, "%s/%s", TEST_CONF_BASE, filename);

	fd = open(path, O_RDONLY);
	if (fd < 0)
		err(EXIT_FAILURE, "Can't open test conf file %s\n", path);

	rc = fstat(fd, &stat);
	assert(!rc);
	(void)rc;

	test->conf.size = stat.st_size;
	test->conf.buf = talloc_array(test, char, test->conf.size + 1);

	rc = read(fd, test->conf.buf, test->conf.size);
	assert(rc == (ssize_t)test->conf.size);

	*(char *)(test->conf.buf + test->conf.size) = '\0';

	close(fd);
	talloc_free(path);
}

void test_set_conf_source(struct parser_test *test, const char *url)
{
	test->ctx->conf_url = pb_url_parse(test, url);
	assert(test->ctx->conf_url);
}

int test_run_parser(struct parser_test *test, const char *parser_name)
{
	struct p_item* i;

	list_for_each_entry(&parsers, i, list) {
		if (strcmp(i->parser->name, parser_name))
			continue;
		test->ctx->parser = i->parser;
		return i->parser->parse(test->ctx, test->conf.buf, test->conf.size);
	}

	errx(EXIT_FAILURE, "%s: parser '%s' not found", __func__, parser_name);
}

bool resource_resolve(struct device_handler *handler, struct parser *parser,
		struct resource *resource)
{
	if (!resource)
		return true;
	if (resource->resolved)
		return true;

	assert(parser);
	assert(parser->resolve_resource);

	return parser->resolve_resource(handler, resource);
}

void boot_option_resolve(struct device_handler *handler,
		struct discover_boot_option *opt)
{
	resource_resolve(handler, opt->source, opt->boot_image);
	resource_resolve(handler, opt->source, opt->initrd);
	resource_resolve(handler, opt->source, opt->icon);
}

void test_hotplug_device(struct parser_test *test, struct discover_device *dev)
{
	struct discover_boot_option *opt;

	device_handler_add_device(test->handler, dev);

	list_for_each_entry(&test->ctx->boot_options, opt, list)
		boot_option_resolve(test->handler, opt);
}

struct discover_boot_option *get_boot_option(struct discover_context *ctx,
		int idx)
{
	struct discover_boot_option *opt;
	int i = 0;

	list_for_each_entry(&ctx->boot_options, opt, list) {
		if (i++ == idx)
			return opt;
	}

	assert(0);

	return NULL;
}

void __check_boot_option_count(struct discover_context *ctx, int count,
		const char *file, int line)
{
	struct discover_boot_option *opt;
	int i = 0;

	list_for_each_entry(&ctx->boot_options, opt, list)
		i++;

	if (i == count)
		return;

	fprintf(stderr, "%s:%d: boot option count check failed\n", file, line);
	fprintf(stderr, "expected %d options, got %d:\n", count, i);

	i = 1;
	list_for_each_entry(&ctx->boot_options, opt, list)
		fprintf(stderr, "  %2d: %s [%s]\n", i++, opt->option->name,
				opt->option->id);

	exit(EXIT_FAILURE);
}

void __check_args(struct discover_boot_option *opt, const char *args,
		const char *file, int line)
{
	int rc;

	if (!opt->option->boot_args) {
		fprintf(stderr, "%s:%d: arg check failed\n", file, line);
		fprintf(stderr, "  no arguments parsed\n");
		fprintf(stderr, "  expected '%s'\n", args);
		exit(EXIT_FAILURE);
	}

	rc = strcmp(opt->option->boot_args, args);
	if (rc) {
		fprintf(stderr, "%s:%d: arg check failed\n", file, line);
		fprintf(stderr, "  got      '%s'\n", opt->option->boot_args);
		fprintf(stderr, "  expected '%s'\n", args);
		exit(EXIT_FAILURE);
	}
}

void __check_name(struct discover_boot_option *opt, const char *name,
		const char *file, int line)
{
	int rc;

	rc = strcmp(opt->option->name, name);
	if (rc) {
		fprintf(stderr, "%s:%d: name check failed\n", file, line);
		fprintf(stderr, "  got      '%s'\n", opt->option->name);
		fprintf(stderr, "  expected '%s'\n", name);
		exit(EXIT_FAILURE);
	}
}

void __check_is_default(struct discover_boot_option *opt,
		const char *file, int line)
{
	if (opt->option->is_default)
		return;

	fprintf(stderr, "%s:%d: default check failed\n", file, line);
	exit(EXIT_FAILURE);
}

void __check_resolved_local_resource(struct resource *res,
		struct discover_device *dev, const char *local_path,
		const char *file, int line)
{
	const char *exp_url, *got_url;

	if (!res)
		errx(EXIT_FAILURE, "%s:%d: No resource", file, line);

	if (!res->resolved)
		errx(EXIT_FAILURE, "%s:%d: Resource is not resolved",
				file, line);

	exp_url = talloc_asprintf(res, "file://%s%s",
			dev->mount_path, local_path);
	got_url = pb_url_to_string(res->url);

	if (strcmp(got_url, exp_url)) {
		fprintf(stderr, "%s:%d: Resource mismatch\n", file, line);
		fprintf(stderr, "  got      '%s'\n", got_url);
		fprintf(stderr, "  expected '%s'\n", exp_url);
		exit(EXIT_FAILURE);
	}
}

void __check_resolved_url_resource(struct resource *res,
		const char *url, const char *file, int line)
{
	char *res_url;

	if (!res)
		errx(EXIT_FAILURE, "%s:%d: No resource", file, line);

	if (!res->resolved)
		errx(EXIT_FAILURE, "%s:%d: Resource is not resolved",
				file, line);

	res_url = pb_url_to_string(res->url);
	if (strcmp(url, res_url)) {
		fprintf(stderr, "%s:%d: Resource mismatch\n", file, line);
		fprintf(stderr, "  got      '%s'\n", res_url);
		fprintf(stderr, "  expected '%s'\n", url);
		exit(EXIT_FAILURE);
	}
}
void __check_unresolved_resource(struct resource *res,
		const char *file, int line)
{
	if (!res)
		errx(EXIT_FAILURE, "%s:%d: No resource", file, line);

	if (res->resolved)
		errx(EXIT_FAILURE, "%s:%d: Resource is resolved", file, line);
}

void __check_not_present_resource(struct resource *res,
		const char *file, int line)
{
	if (res)
		errx(EXIT_FAILURE, "%s:%d: Resource present", file, line);
}
