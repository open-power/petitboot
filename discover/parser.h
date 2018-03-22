#ifndef _PARSER_H
#define _PARSER_H

#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

#include "device-handler.h"

struct discover_context;
struct device_handler;
struct resource;

/**
 * Our config parser.
 *
 * Each parser is responsible for creating discover_boot_options from config
 * files found on new devices. The boot items discovered during parse will have
 * 'resources' attached (see @discover_boot_option), which may already be
 * resolved (in the case of a device-local filename, or a URL), or unresolved
 * (in the case of a filename on another device).
 *
 * If the boot option contains references to unresolved resources, the
 * device handler will not inform clients about the boot options, as
 * they're not properly "available" at this stage. The handler will attempt to
 * resolve them whenever new devices are discovered, by calling the parser's
 * resolve_resource function. Once a boot option's resources are full resolved,
 * the option can be sent to clients.
 */
struct parser {
	char			*name;
	int			(*parse)(struct discover_context *ctx);
	bool			(*resolve_resource)(
						struct device_handler *handler,
						struct resource *res);
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

/* File IO functions for parsers; these should be the only interface that
 * parsers use to access a device's filesystem.
 *
 * These are intended for small amounts of data, typically text
 * configuration and state files.  Note that parser_request_file,
 * and parser_replace_file work only on non-directories.
 */
int parser_request_file(struct discover_context *ctx,
		struct discover_device *dev, const char *filename,
		char **buf, int *len);
int parser_replace_file(struct discover_context *ctx,
		struct discover_device *dev, const char *filename,
		char *buf, int len);
int parser_request_url(struct discover_context *ctx, struct pb_url *url,
		char **buf, int *len);
/* parser_stat_path returns 0 if path can be stated on dev by the
 * running user.  Note that this function follows symlinks, like the
 * stat system call.  When the function returns 0, also fills in
 * statbuf for the path.  Returns non-zero on error.  This function
 * does not have the limitations on file size that the functions above
 * do.  Unlike some of the functions above, this function also works
 * on directories. */
int parser_stat_path(struct discover_context *ctx,
		struct discover_device *dev, const char *path,
		struct stat *statbuf);
/* Function used to list the files on a directory. The dirname should
 * be relative to the discover context device mount path. It returns
 * the number of files returned in files or a negative value on error.
 */
int parser_scandir(struct discover_context *ctx, const char *dirname,
		   struct dirent ***files, int (*filter)(const struct dirent *),
		   int (*comp)(const struct dirent **, const struct dirent **));

#endif /* _PARSER_H */
