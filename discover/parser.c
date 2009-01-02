
#include <stdlib.h>

#include "pb-protocol/pb-protocol.h"

#include "device-handler.h"
#include "log.h"
#include "parser.h"
#include "parser-utils.h"

extern struct parser __start_parsers[], __stop_parsers[];

void iterate_parsers(struct discover_context *ctx)
{
	struct parser *parser;

	pb_log("trying parsers for %s\n", ctx->device_path);

	for (parser = __start_parsers; parser < __stop_parsers; parser++) {
		pb_log("\ttrying parser '%s'\n", parser->name);
		/* just use a dummy device path for now */
		if (parser->parse(ctx))
			return;
	}
	pb_log("\tno boot_options found\n");
}

static int compare_parsers(const void *a, const void *b)
{
	const struct parser *parser_a = a, *parser_b = b;

	if (parser_a->priority > parser_b->priority)
		return -1;

	if (parser_a->priority < parser_b->priority)
		return 1;

	return 0;
}

void parser_init(void)
{
	/* sort our parsers into descending priority order */
	qsort(__start_parsers, __stop_parsers - __start_parsers,
			sizeof(struct parser), compare_parsers);
}
