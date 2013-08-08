
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/socket.h>
#include <linux/if.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>

#include <log/log.h>
#include <list/list.h>
#include <talloc/talloc.h>
#include <waiter/waiter.h>
#include <pb-config/pb-config.h>
#include <system/system.h>

#include "file.h"
#include "network.h"

#define HWADDR_SIZE	6
#define PIDFILE_BASE	(LOCAL_STATE_DIR "/petitboot/")

#define for_each_nlmsg(buf, nlmsg, len) \
	for (nlmsg = (struct nlmsghdr *)buf; \
		NLMSG_OK(nlmsg, len) && nlmsg->nlmsg_type != NLMSG_DONE; \
		nlmsg = NLMSG_NEXT(nlmsg, len))

#define for_each_rta(buf, rta, attrlen) \
	for (rta = (struct rtattr *)(buf); RTA_OK(rta, attrlen); \
			rta = RTA_NEXT(rta, attrlen))


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
};

struct network {
	struct list	interfaces;
	struct waiter	*waiter;
	int		netlink_sd;
	bool		manual_config;
	bool		dry_run;
};

static const struct interface_config *find_config_by_hwaddr(
		uint8_t *hwaddr)
{
	const struct config *config;
	int i;

	config = config_get();
	if (!config)
		return NULL;

	for (i = 0; i < config->network.n_interfaces; i++) {
		struct interface_config *ifconf = config->network.interfaces[i];

		if (!memcmp(ifconf->hwaddr, hwaddr, HWADDR_SIZE))
			return ifconf;
	}

	return NULL;
}

static struct interface *find_interface_by_ifindex(struct network *network,
		int ifindex)
{
	struct interface *interface;

	list_for_each_entry(&network->interfaces, interface, list)
		if (interface->ifindex == ifindex)
			return interface;

	return NULL;
}

static int network_init_netlink(struct network *network)
{
	struct sockaddr_nl addr;
	int rc;

	memset(&addr, 0, sizeof(addr));
	addr.nl_family = AF_NETLINK;
	addr.nl_groups = RTMGRP_LINK;

	network->netlink_sd = socket(AF_NETLINK, SOCK_DGRAM, NETLINK_ROUTE);
	if (network->netlink_sd < 0) {
		perror("socket(AF_NETLINK)");
		return -1;
	}

	rc = bind(network->netlink_sd, (struct sockaddr *)&addr, sizeof(addr));
	if (rc) {
		perror("bind(sockaddr_nl)");
		close(network->netlink_sd);
		return -1;
	}

	return 0;
}

static int network_send_link_query(struct network *network)
{
	int rc;
	struct {
		struct nlmsghdr nlmsg;
		struct rtgenmsg rtmsg;
	} msg;

	memset(&msg, 0, sizeof(msg));

	msg.nlmsg.nlmsg_len = sizeof(msg);
	msg.nlmsg.nlmsg_type = RTM_GETLINK;
	msg.nlmsg.nlmsg_flags = NLM_F_REQUEST | NLM_F_ROOT;
	msg.nlmsg.nlmsg_seq = 0;
	msg.nlmsg.nlmsg_pid = 0;
	msg.rtmsg.rtgen_family = AF_UNSPEC;

	rc = send(network->netlink_sd, &msg, sizeof(msg), MSG_NOSIGNAL);
	if (rc != sizeof(msg))
		return -1;

	return 0;
}

static int interface_change(struct network *network,
		struct interface *interface,
		bool up)
{
	int rc;
	const char *statestr = up ? "up" : "down";
	const char *argv[] = {
		pb_system_apps.ip,
		"link",
		"set",
		interface->name,
		statestr,
		NULL,
	};

	rc = pb_run_cmd(argv, 1, network->dry_run);
	if (rc) {
		pb_log("failed to bring interface %s %s\n", interface->name,
				statestr);
		return -1;
	}
	return 0;
}

static int interface_up(struct network *network,
		struct interface *interface)
{
	return interface_change(network, interface, true);
}

static int interface_down(struct network *network,
		struct interface *interface)
{
	return interface_change(network, interface, false);
}

static void configure_interface_dhcp(struct network *network,
		struct interface *interface)
{
	char pidfile[256];
	const char *argv[] = {
		pb_system_apps.udhcpc,
		"-R",
		"-n",
		"-p", pidfile,
		"-i", interface->name,
		NULL,
	};
	snprintf(pidfile, sizeof(pidfile), "%s/udhcpc-%s.pid",
			PIDFILE_BASE, interface->name);

	pb_run_cmd(argv, 0, network->dry_run);
	return;
}

static void configure_interface_static(struct network *network,
		struct interface *interface,
		const struct interface_config *config)
{
	const char *addr_argv[] = {
		pb_system_apps.ip,
		"address",
		"add",
		config->static_config.address,
		"dev",
		interface->name,
		NULL,
	};
	const char *route_argv[] = {
		pb_system_apps.ip,
		"route",
		"add",
		"default",
		"via",
		config->static_config.gateway,
		NULL,
	};
	int rc;


	rc = pb_run_cmd(addr_argv, 1, network->dry_run);
	if (rc) {
		pb_log("failed to add address %s to interface %s\n",
				config->static_config.address,
				interface->name);
		return;
	}

	/* we need the interface up before we can route through it */
	rc = interface_up(network, interface);
	if (rc)
		return;

	if (config->static_config.gateway)
		rc = pb_run_cmd(route_argv, 1, network->dry_run);

	if (rc) {
		pb_log("failed to add default route %s on interface %s\n",
				config->static_config.gateway,
				interface->name);
	}

	return;
}

static void configure_interface(struct network *network,
		struct interface *interface, bool up, bool link)
{
	const struct interface_config *config = NULL;

	if (interface->state == IFSTATE_IGNORED)
		return;

	/* old interface? check that we're still up and running */
	if (interface->state == IFSTATE_CONFIGURED) {
		if (!up)
			interface->state = IFSTATE_NEW;
		else if (!link)
			interface->state = IFSTATE_UP_WAITING_LINK;
		else
			return;
	}

	/* always up the lookback, no other handling required */
	if (!strcmp(interface->name, "lo")) {
		if (interface->state == IFSTATE_NEW)
			interface_up(network, interface);
		interface->state = IFSTATE_CONFIGURED;
		return;
	}

	config = find_config_by_hwaddr(interface->hwaddr);
	if (config && config->ignore) {
		pb_log("network: ignoring interface %s\n", interface->name);
		interface->state = IFSTATE_IGNORED;
		return;
	}

	/* if we're in manual config mode, we need an interface configuration */
	if (network->manual_config && !config) {
		interface->state = IFSTATE_IGNORED;
		pb_log("network: skipping %s: manual config mode, "
				"but no config for this interface\n",
				interface->name);
		return;
	}

	/* new interface? bring up to the point so we can detect a link */
	if (interface->state == IFSTATE_NEW) {
		if (!up) {
			interface_up(network, interface);
			pb_log("network: bringing up interface %s\n",
					interface->name);
			return;

		} else if (!link) {
			interface->state = IFSTATE_UP_WAITING_LINK;
		}
	}

	/* no link? wait for a notification */
	if (interface->state == IFSTATE_UP_WAITING_LINK && !link)
		return;

	pb_log("network: configuring interface %s\n", interface->name);

	if (!config || config->method == CONFIG_METHOD_DHCP) {
		configure_interface_dhcp(network, interface);

	} else if (config->method == CONFIG_METHOD_STATIC) {
		configure_interface_static(network, interface, config);
	}
}

static int network_handle_nlmsg(struct network *network, struct nlmsghdr *nlmsg)
{
	bool have_ifaddr, have_ifname;
	struct interface *interface;
	struct ifinfomsg *info;
	struct rtattr *attr;
	uint8_t ifaddr[6];
	char ifname[IFNAMSIZ+1];
	int attrlen, type;


	/* we're only interested in NEWLINK messages */
	type = nlmsg->nlmsg_type;
	if (!(type == RTM_NEWLINK || type == RTM_DELLINK))
		return 0;

	info = NLMSG_DATA(nlmsg);

	have_ifaddr = have_ifname = false;

	attrlen = nlmsg->nlmsg_len - sizeof(*info);

	/* extract the interface name and hardware address attributes */
	for_each_rta(info + 1, attr, attrlen) {
		void *data = RTA_DATA(attr);

		switch (attr->rta_type) {
		case IFLA_ADDRESS:
			memcpy(ifaddr, data, sizeof(ifaddr));
			have_ifaddr = true;
			break;

		case IFLA_IFNAME:
			strncpy(ifname, data, IFNAMSIZ);
			have_ifname = true;
			break;
		}
	}

	if (!have_ifaddr || !have_ifname)
		return -1;

	if (type == RTM_DELLINK) {
		interface = find_interface_by_ifindex(network, info->ifi_index);
		if (!interface)
			return 0;
		pb_log("network: interface %s removed\n", interface->name);
		list_remove(&interface->list);
		talloc_free(interface);
		return 0;
	}


	interface = find_interface_by_ifindex(network, info->ifi_index);
	if (!interface) {
		interface = talloc_zero(network, struct interface);
		interface->ifindex = info->ifi_index;
		interface->state = IFSTATE_NEW;
		memcpy(interface->hwaddr, ifaddr, sizeof(interface->hwaddr));
		strncpy(interface->name, ifname, sizeof(interface->name) - 1);
	}

	configure_interface(network, interface,
			info->ifi_flags & IFF_UP,
			info->ifi_flags & IFF_LOWER_UP);

	return 0;
}

static int network_netlink_process(void *arg)
{
	struct network *network = arg;
	struct nlmsghdr *nlmsg;
	unsigned int len;
	char buf[4096];
	int rc;

	rc = recv(network->netlink_sd, buf, sizeof(buf), 0);
	if (rc < 0) {
		perror("netlink recv");
		return -1;
	}

	len = rc;

	for_each_nlmsg(buf, nlmsg, len)
		network_handle_nlmsg(network, nlmsg);

	return 0;
}

static void network_init_dns(struct network *network)
{
	const struct config *config;
	int i, rc, len;
	bool modified;
	char *buf;

	if (network->dry_run)
		return;

	config = config_get();
	if (!config || !config->network.n_dns_servers)
		return;

	rc = read_file(network, "/etc/resolv.conf", &buf, &len);

	if (rc) {
		buf = talloc_strdup(network, "");
		len = 0;
	}

	modified = false;

	for (i = 0; i < config->network.n_dns_servers; i++) {
		int dns_conf_len;
		char *dns_conf;

		dns_conf = talloc_asprintf(network, "server %s\n",
				config->network.dns_servers[i]);

		if (strstr(buf, dns_conf)) {
			talloc_free(dns_conf);
			continue;
		}

		dns_conf_len = strlen(dns_conf);
		buf = talloc_realloc(network, buf, char, len + dns_conf_len);
		memcpy(buf + len, dns_conf, dns_conf_len);
		len += dns_conf_len;
		modified = true;
	}

	if (!modified)
		return;

	rc = replace_file("/etc/resolv.conf", buf, len);
	if (rc) {
		pb_log("error replacing resolv.conf: %s\n", strerror(errno));
		return;
	}

}

struct network *network_init(void *ctx, struct waitset *waitset, bool dry_run)
{
	struct network *network;
	int rc;

	network = talloc(ctx, struct network);
	list_init(&network->interfaces);
	network->manual_config = false;
	network->dry_run = dry_run;

	network_init_dns(network);

	rc = network_init_netlink(network);
	if (rc)
		goto err;

	network->waiter = waiter_register_io(waitset, network->netlink_sd,
			WAIT_IN, network_netlink_process, network);

	if (!network->waiter)
		goto err;

	rc = network_send_link_query(network);
	if (rc)
		goto err;

	return network;

err:
	network_shutdown(network);
	return NULL;
}


int network_shutdown(struct network *network)
{
	struct interface *interface;

	if (network->waiter)
		waiter_remove(network->waiter);

	list_for_each_entry(&network->interfaces, interface, list)
		interface_down(network, interface);

	close(network->netlink_sd);
	talloc_free(network);
	return 0;
}
