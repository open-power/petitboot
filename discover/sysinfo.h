#ifndef SYSINFO_H
#define SYSINFO_H

#include <types/types.h>

struct discover_server;

const struct system_info *system_info_get(void);

void system_info_register_interface(unsigned hwaddr_size, uint8_t *hwaddr,
		const char *name);

void system_info_init(struct discover_server *server);

#endif /* SYSINFO_H */

