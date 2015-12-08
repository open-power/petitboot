#ifndef HOSTBOOT_H
#define HOSTBOOT_H

#include "config.h"
#include <types/types.h>

#ifdef MTD_SUPPORT
void hostboot_load_versions(struct system_info *info);
#else
static inline void hostboot_load_versions(struct system_info *info)
{
	(void)info;
}
#endif
#endif /* HOSTBOOT_H */
