#include <string.h>
#include <types/types.h>
#include <talloc/talloc.h>
#include <sys/socket.h>
#include <linux/if.h>
#include "network.h"

struct interface {
	int	ifindex;
	char	name[IFNAMSIZ];
	uint8_t	hwaddr[HWADDR_SIZE];

	enum {
		IFSTATE_NEW,
		IFSTATE_UP_WAITING_LINK,
		IFSTATE_CONFIGURED,
		IFSTATE_IGNORED,
	} state;

	struct list_item list;
	struct process *udhcpc_process;
	struct discover_device *dev;
};

static struct interface test = {
	.name = "em1",
	.hwaddr = {1,2,3,4,5,6},
};

static struct interface *find_interface_by_name(struct network *network,
				const char *name)
{
	(void)network;

	if (!strcmp(test.name, name))
		return &test;

	return NULL;
}

uint8_t *find_mac_by_name(void *ctx, struct network *network,
		const char *name)
{
	struct interface *interface;
	(void)network;

	interface = find_interface_by_name(network, name);
	if (!interface)
		return NULL;

	return talloc_memdup(ctx, &interface->hwaddr,
			     sizeof(uint8_t) * HWADDR_SIZE);
}

void network_requery_device(struct network *network,
		struct discover_device *dev)
{
	(void)network;
	(void)dev;
}
