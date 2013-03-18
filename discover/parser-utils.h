#ifndef PARSER_UTILS_H
#define PARSER_UTILS_H

#include "types/types.h"
#include "parser.h"

#define streq(a,b) (!strcasecmp((a),(b)))

#define artwork_pathname(file) (PKG_SHARE_DIR "/artwork/" file)

#define __parser_funcname(_n) __register_parser ## _ ## _n
#define  _parser_funcname(_n) __parser_funcname(_n)

#define register_parser(_parser)					\
	static	__attribute__((constructor))				\
		void _parser_funcname(__COUNTER__)(void)		\
	{								\
		__register_parser(&_parser);				\
	}

void __register_parser(struct parser *parser);

void device_add_boot_option(struct device *device,
		struct boot_option *boot_option);

const char *generic_icon_file(enum generic_icon_type type);

enum generic_icon_type guess_device_type(struct discover_context *ctx);

#endif /* PARSER_UTILS_H */
