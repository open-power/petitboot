
#include <stdlib.h>

#include "pb-protocol/pb-protocol.h"
#include <log/log.h>

#include "device-handler.h"
#include "parser.h"
#include "parser-utils.h"

struct parser __native_parser;
struct parser __yaboot_parser;
struct parser __kboot_parser;
struct parser __grub2_parser;

static const struct parser *const parsers[] = {
//	&__native_parser,
	&__yaboot_parser,
	&__kboot_parser,
	NULL
};

void iterate_parsers(struct discover_context *ctx)
{
	int i;
	unsigned int count = 0;

	pb_log("trying parsers for %s\n", ctx->device_path);

	for (i = 0; parsers[i]; i++) {
		pb_log("\ttrying parser '%s'\n", parsers[i]->name);
		count += parsers[i]->parse(ctx);
	}
	if (!count)
		pb_log("\tno boot_options found\n");
}

void parser_init(void)
{
}
