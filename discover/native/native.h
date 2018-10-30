#ifndef NATIVE_H
#define NATIVE_H

#include <discover/device-handler.h>

struct native_parser {
	struct discover_context		*ctx;
	struct discover_boot_option 	*opt;
	void				*scanner;
	const char			*filename;
	char				*default_name;
};

void native_parser_finish(struct native_parser *parser);
void native_set_resource(struct native_parser *parser, struct resource **,
		const char *path);
void native_append_string(struct native_parser *parser,
		char **str, const char *append);
void native_parser_create_option(struct native_parser *parser,
		const char *name);

/* external parser api */
struct native_parser *native_parser_create(struct discover_context *ctx);
void native_parser_parse(struct native_parser *parser, const char *filename,
		char *buf, int len);
#endif /* NATIVE_H */

