
#include <string.h>

#include <talloc/talloc.h>
#include <process/process.h>
#include <log/log.h>

#include "discover-server.h"
#include "platform.h"
#include "sysinfo.h"

static struct system_info *sysinfo;
static struct discover_server *server;

const struct system_info *system_info_get(void)
{
	return sysinfo;
}


void system_info_set_interface_address(unsigned int hwaddr_size,
		uint8_t *hwaddr, const char *address)
{
	struct interface_info *if_info;
	unsigned int i;
	char mac[20];

	for (i = 0; i < sysinfo->n_interfaces; i++) {
		if_info = sysinfo->interfaces[i];

		if (if_info->hwaddr_size != hwaddr_size)
			continue;

		if (memcmp(if_info->hwaddr, hwaddr, hwaddr_size))
			continue;

		/* Found an existing interface. Notify clients if a new address
		 * is set */
		if (!if_info->address || strcmp(if_info->address, address)) {
			talloc_free(if_info->address);
			if_info->address = talloc_strdup(if_info, address);
			discover_server_notify_system_info(server, sysinfo);
			return;
		}
	}

	mac_str(hwaddr, hwaddr_size, mac, sizeof(mac));
	pb_log("Couldn't find interface matching %s\n", mac);
}

void system_info_register_interface(unsigned int hwaddr_size, uint8_t *hwaddr,
		const char *name, bool link)
{
	struct interface_info *if_info;
	unsigned int i;

	for (i = 0; i < sysinfo->n_interfaces; i++) {
		bool changed = false;

		if_info = sysinfo->interfaces[i];

		if (if_info->hwaddr_size != hwaddr_size)
			continue;

		if (memcmp(if_info->hwaddr, hwaddr, hwaddr_size))
			continue;

		/* Found an existing interface. Notify clients on any name or
		 * link changes */
		if (strcmp(if_info->name, name)) {
			talloc_free(if_info->name);
			if_info->name = talloc_strdup(if_info, name);
			changed = true;
		}

		if (if_info->link != link) {
			if_info->link = link;
			changed = true;
		}

		if (changed)
			discover_server_notify_system_info(server, sysinfo);

		return;
	}

	if_info = talloc_zero(sysinfo, struct interface_info);
	if_info->hwaddr_size = hwaddr_size;
	if_info->hwaddr = talloc_memdup(if_info, hwaddr, hwaddr_size);
	if_info->name = talloc_strdup(if_info, name);
	if_info->link = link;

	sysinfo->n_interfaces++;
	sysinfo->interfaces = talloc_realloc(sysinfo, sysinfo->interfaces,
						struct interface_info *,
						sysinfo->n_interfaces);
	sysinfo->interfaces[sysinfo->n_interfaces - 1] = if_info;

	discover_server_notify_system_info(server, sysinfo);
}

void system_info_register_blockdev(const char *name, const char *uuid,
		const char *mountpoint)
{
	struct blockdev_info *bd_info;
	unsigned int i;

	for (i = 0; i < sysinfo->n_blockdevs; i++) {
		bd_info = sysinfo->blockdevs[i];

		if (strcmp(bd_info->name, name))
			continue;

		/* update the mountpoint and UUID, and we're done */
		talloc_free(bd_info->mountpoint);
		bd_info->uuid = talloc_strdup(bd_info, uuid);
		bd_info->mountpoint = talloc_strdup(bd_info, mountpoint);
		discover_server_notify_system_info(server, sysinfo);
		return;
	}

	bd_info = talloc_zero(sysinfo, struct blockdev_info);
	bd_info->name = talloc_strdup(bd_info, name);
	bd_info->uuid = talloc_strdup(bd_info, uuid);
	bd_info->mountpoint = talloc_strdup(bd_info, mountpoint);

	sysinfo->n_blockdevs++;
	sysinfo->blockdevs = talloc_realloc(sysinfo, sysinfo->blockdevs,
						struct blockdev_info *,
						sysinfo->n_blockdevs);
	sysinfo->blockdevs[sysinfo->n_blockdevs - 1] = bd_info;

	discover_server_notify_system_info(server, sysinfo);
}

void system_info_init(struct discover_server *s)
{
	server = s;
	sysinfo = talloc_zero(server, struct system_info);
	platform_get_sysinfo(sysinfo);
}
