#ifndef _PARSER_H
#define _PARSER_H

struct discover_context;

struct parser {
	char			*name;
	const char * const	*filenames;
	int			(*parse)(struct discover_context *ctx);
};

enum generic_icon_type {
	ICON_TYPE_DISK,
	ICON_TYPE_USB,
	ICON_TYPE_OPTICAL,
	ICON_TYPE_NETWORK,
	ICON_TYPE_UNKNOWN
};

#define streq(a,b) (!strcasecmp((a),(b)))

void parser_init(void);

void iterate_parsers(struct discover_context *ctx);
int parse_user_event(struct discover_context *ctx, struct event *event);

#endif /* _PARSER_H */
