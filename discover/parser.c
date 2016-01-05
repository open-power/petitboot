
#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "types/types.h"
#include <file/file.h>
#include <log/log.h>
#include <talloc/talloc.h>

#include "device-handler.h"
#include "parser.h"
#include "parser-utils.h"
#include "paths.h"

struct p_item {
	struct list_item list;
	struct parser *parser;
};

STATIC_LIST(parsers);

static char *local_path(struct discover_context *ctx,
		struct discover_device *dev,
		const char *filename)
{
	return join_paths(ctx, dev->root_path, filename);
}

int parser_request_file(struct discover_context *ctx,
		struct discover_device *dev, const char *filename,
		char **buf, int *len)

{
	char *path;
	int rc;

	/* we only support local files at present */
	if (!dev->mount_path)
		return -1;

	path = local_path(ctx, dev, filename);

	rc = read_file(ctx, path, buf, len);

	talloc_free(path);

	return rc;
}

int parser_check_dir(struct discover_context *ctx,
		struct discover_device *dev, const char *dirname)
{
	struct stat statbuf;
	char *path;
	int rc;

	if (!dev->mount_path)
		return -1;

	path = local_path(ctx, dev, dirname);

	rc = stat(path, &statbuf);
	talloc_free(path);
	if (!rc)
		return -1;

	return S_ISDIR(statbuf.st_mode) ? 0 : -1;
}

int parser_replace_file(struct discover_context *ctx,
		struct discover_device *dev, const char *filename,
		char *buf, int len)
{
	bool release;
	char *path;
	int rc;

	if (!dev->mounted)
		return -1;

	rc = device_request_write(dev, &release);
	if (rc) {
		pb_log("Can't write file %s: device doesn't allow write\n",
				dev->device_path);
		return -1;
	}

	path = local_path(ctx, dev, filename);

	rc = replace_file(path, buf, len);

	talloc_free(path);

	device_release_write(dev, release);

	return rc;
}

int parser_request_url(struct discover_context *ctx, struct pb_url *url,
		char **buf, int *len)
{
	struct load_url_result *result;
	int rc;

	result = load_url(ctx, url);
	if (!result)
		goto out;

	rc = read_file(ctx, result->local, buf, len);
	if (rc) {
		pb_log("Read failed for the parser %s on file %s\n",
				ctx->parser->name, result->local);
		goto out_clean;
	}

	return 0;

out_clean:
	if (result->cleanup_local)
		unlink(result->local);
out:
	return -1;
}

void iterate_parsers(struct discover_context *ctx)
{
	struct p_item* i;

	pb_log("trying parsers for %s\n", ctx->device->device->id);

	list_for_each_entry(&parsers, i, list) {
		pb_debug("\ttrying parser '%s'\n", i->parser->name);
		ctx->parser = i->parser;
		i->parser->parse(ctx);
	}
	ctx->parser = NULL;
}

static void *parsers_ctx;

void __register_parser(struct parser *parser)
{
	struct p_item *i;

	if (!parsers_ctx)
		parsers_ctx = talloc_new(NULL);

	i = talloc(parsers_ctx, struct p_item);
	i->parser = parser;
	list_add(&parsers, &i->list);
}

static __attribute__((destructor)) void cleanup_parsers(void)
{
	talloc_free(parsers_ctx);
}

void parser_init(void)
{
}
