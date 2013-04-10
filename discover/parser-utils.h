#ifndef PARSER_UTILS_H
#define PARSER_UTILS_H

#include "types/types.h"
#include "parser.h"

#define streq(a,b) (!strcasecmp((a),(b)))

#define artwork_pathname(file) (PKG_SHARE_DIR "/artwork/" file)

#define define_parser(__name, __parse_fn)			\
	struct parser 							\
	__ ## __name ## _parser = {					\
		.name		= #__name,				\
		.parse		= __parse_fn,				\
	};

void device_add_boot_option(struct device *device,
		struct boot_option *boot_option);

const char *generic_icon_file(enum generic_icon_type type);

enum generic_icon_type guess_device_type(struct discover_context *ctx);

#endif /* PARSER_UTILS_H */
