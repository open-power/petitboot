#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include <log/log.h>
#include <talloc/talloc.h>
#include <flash/flash.h>

#include "hostboot.h"

void hostboot_load_versions(struct system_info *info)
{
	int n = 0;

	n = flash_parse_version(info, &info->platform_primary, true);
	if (n < 0)
		pb_log("Failed to read platform versions for current side\n");
	else
		info->n_primary = n;

	n = flash_parse_version(info, &info->platform_other, false);
	if (n < 0)
		pb_log("Failed to read platform versions for other side\n");
	else
		info->n_other = n;
}
