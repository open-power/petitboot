
#include <stdlib.h>

#include "pb-protocol/pb-protocol.h"

#include "device-handler.h"
#include "log.h"
#include "parser.h"
extern struct parser kboot_parser;

/* array of parsers, ordered by priority */
static struct parser *parsers[] = {
	&kboot_parser,
	NULL
};

void iterate_parsers(struct discover_context *ctx)
{
	int i;

	pb_log("trying parsers for %s\n", ctx->device_path);

	for (i = 0; parsers[i]; i++) {
		pb_log("\ttrying parser '%s'\n", parsers[i]->name);
		/* just use a dummy device path for now */
		if (parsers[i]->parse(ctx))
			return;
	}
	pb_log("\tno boot_options found\n");
}
