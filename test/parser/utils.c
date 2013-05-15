
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

static int n_parsers;
static struct parser **parsers;

void __register_parser(struct parser *parser)
{
	parsers = talloc_realloc(NULL, parsers, struct parser *, n_parsers + 1);
	parsers[n_parsers] = parser;
	n_parsers++;
}

static struct discover_device *test_create_device_simple(
		struct discover_context *ctx)
{
	static int dev_idx;
	char name[10];

	sprintf(name, "__test%d", dev_idx++);

	return test_create_device(ctx, name);
}

struct discover_device *test_create_device(struct discover_context *ctx,
		const char *name)
{
	struct discover_device *dev;

	dev = talloc_zero(ctx, struct discover_device);
	dev->device = talloc_zero(dev, struct device);

	list_init(&dev->boot_options);

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
	ctx->device = test_create_device_simple(ctx);

	return ctx;
}

struct parser_test *test_init(void)
{
	struct parser_test *test;

	test = talloc_zero(NULL, struct parser_test);
	test->handler = device_handler_init(NULL, 0);
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

int test_run_parser(struct parser_test *test, const char *parser_name)
{
	struct parser *parser;
	int i, rc = 0;

	for (i = 0; i < n_parsers; i++) {
		parser = parsers[i];
		if (strcmp(parser->name, parser_name))
			continue;
		test->ctx->parser = parser;
		rc = parser->parse(test->ctx, test->conf.buf, test->conf.size);
	}

	return rc;
}
