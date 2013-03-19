
#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "types/types.h"
#include <log/log.h>
#include <talloc/talloc.h>

#include "device-handler.h"
#include "parser.h"
#include "parser-utils.h"
#include "paths.h"

static int n_parsers;
static struct parser **parsers;

static const int max_file_size = 1024 * 1024;

static int read_file(struct discover_context *ctx,
		const char *filename, char **bufp, int *lenp)
{
	struct stat statbuf;
	int rc, fd, i, len;
	char *buf;

	fd = open(filename, O_RDONLY);
	if (fd < 0)
		return -1;

	rc = fstat(fd, &statbuf);
	if (rc < 0)
		goto err_close;

	len = statbuf.st_size;
	if (len > max_file_size)
		goto err_close;

	buf = talloc_array(ctx, char, len);
	if (!buf)
		goto err_close;

	for (i = 0; i < len; i += rc) {
		rc = read(fd, buf + i, len - i);

		/* unexpected EOF: trim and return */
		if (rc == 0) {
			len = i;
			break;
		}

		if (rc < 0)
			goto err_free;

	}

	close(fd);
	*bufp = buf;
	*lenp = len;
	return 0;

err_free:
	talloc_free(buf);
err_close:
	close(fd);
	return -1;
}

static char *local_path(struct discover_context *ctx,
		const char *filename)
{
	return join_paths(ctx, ctx->device->mount_path, filename);
}

static void iterate_parser_files(struct discover_context *ctx,
		const struct parser *parser)
{
	const char * const *filename;
	const char *path;

	if (!parser->filenames)
		return;

	for (filename = parser->filenames; *filename; filename++) {
		int rc, len;
		char *buf;

		path = local_path(ctx, *filename);
		if (!path)
			continue;

		rc = read_file(ctx, path, &buf, &len);
		if (!rc) {
			parser->parse(ctx, buf, len);
			talloc_free(buf);
		}
	}
}

void iterate_parsers(struct discover_context *ctx, enum conf_method method)
{
	int i;

	pb_log("trying parsers for %s\n", ctx->device->device->id);

	if (method == CONF_METHOD_LOCAL_FILE) {
		for (i = 0; i < n_parsers; i++) {
			if (parsers[i]->method != CONF_METHOD_LOCAL_FILE)
				continue;

			pb_log("\ttrying parser '%s'\n", parsers[i]->name);
			ctx->parser = parsers[i];
			iterate_parser_files(ctx, ctx->parser);
		}
		ctx->parser = NULL;
	}
}

void __register_parser(struct parser *parser)
{
	parsers = talloc_realloc(NULL, parsers, struct parser *, n_parsers + 1);
	parsers[n_parsers] = parser;
	n_parsers++;
}

void parser_init(void)
{
}
