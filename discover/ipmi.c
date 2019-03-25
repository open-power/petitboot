
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/types.h>

#include <linux/ipmi.h>

#include <log/log.h>
#include <talloc/talloc.h>

#include "ipmi.h"

struct ipmi {
	int	fd;
	long	seq;
};

static const char *ipmi_devnode = "/dev/ipmi0";

bool ipmi_bootdev_is_valid(int x)
{
	switch (x) {
	case IPMI_BOOTDEV_NONE:
	case IPMI_BOOTDEV_NETWORK:
	case IPMI_BOOTDEV_DISK:
	case IPMI_BOOTDEV_SAFE:
	case IPMI_BOOTDEV_CDROM:
	case IPMI_BOOTDEV_SETUP:
		return true;
	}

	return false;
}

static int ipmi_send(struct ipmi *ipmi, uint8_t netfn, uint8_t cmd,
		uint8_t *buf, uint16_t len)
{
	struct ipmi_system_interface_addr addr;
	struct ipmi_req req;
	int rc;

	memset(&addr, 0, sizeof(addr));
	addr.addr_type = IPMI_SYSTEM_INTERFACE_ADDR_TYPE;
	addr.channel = IPMI_BMC_CHANNEL;

	memset(&req, 0, sizeof(req));
	req.addr = (unsigned char *)&addr;
	req.addr_len = sizeof(addr);

	req.msgid = ipmi->seq++;

	req.msg.data = buf;
	req.msg.data_len = len;
	req.msg.netfn = netfn;
	req.msg.cmd = cmd;

	rc = ioctl(ipmi->fd, IPMICTL_SEND_COMMAND, &req);
	if (rc < 0) {
		pb_log("IPMI: send (netfn %d, cmd %d, %d bytes) failed: %m\n",
				netfn, cmd, len);
		return -1;
	}

	return 0;
}

static int ipmi_recv(struct ipmi *ipmi, uint8_t *netfn, uint8_t *cmd,
		long *seq, uint8_t *buf, uint16_t *len)
{
	struct ipmi_recv recv;
	struct ipmi_addr addr;
	int rc;

	recv.addr = (unsigned char *)&addr;
	recv.addr_len = sizeof(addr);
	recv.msg.data = buf;
	recv.msg.data_len = *len;

	rc = ioctl(ipmi->fd, IPMICTL_RECEIVE_MSG_TRUNC, &recv);
	if (rc < 0 && errno != EMSGSIZE) {
		pb_log("IPMI: recv (%d bytes) failed: %m\n", *len);
		return -1;
	} else if (rc < 0 && errno == EMSGSIZE) {
		pb_debug("IPMI: truncated message (netfn %d, cmd %d, "
				"size %d), continuing anyway\n",
				recv.msg.netfn, recv.msg.cmd, *len);
	}

	*netfn = recv.msg.netfn;
	*cmd = recv.msg.cmd;
	*seq = recv.msgid;
	*len = recv.msg.data_len;

	return 0;
}

int ipmi_transaction(struct ipmi *ipmi, uint8_t netfn, uint8_t cmd,
		uint8_t *req_buf, uint16_t req_len,
		uint8_t *resp_buf, uint16_t *resp_len,
		int timeout_ms)
{
	struct timeval start, now, delta;
	struct pollfd pollfds[1];
	struct flock lock;
	int expired_ms, rc;

	memset(&lock, 0, sizeof(lock));
	lock.l_type = F_WRLCK;
	lock.l_whence = SEEK_SET;
	rc = fcntl(ipmi->fd, F_SETLKW, &lock);
	if (rc == -1) {
		pb_log("IPMI: error locking IPMI device: %m\n");
		return rc;
	}

	rc = ipmi_send(ipmi, netfn, cmd, req_buf, req_len);
	if (rc)
		goto out;

	pollfds[0].fd = ipmi->fd;
	pollfds[0].events = POLLIN;

	gettimeofday(&start, NULL);
	expired_ms = 0;

	for (;;) {
		uint8_t resp_netfn, resp_cmd;
		long seq;

		rc = poll(pollfds, 1, timeout_ms - expired_ms);

		if (rc < 0) {
			pb_log("IPMI: poll() error %m");
			break;
		}
		if (rc == 0) {
			pb_log("IPMI: timeout waiting for response "
					"(netfn %d, cmd %d)\n", netfn, cmd);
			rc = -1;
			break;
		}

		if (!(pollfds[0].revents & POLLIN)) {
			pb_log("IPMI: unexpected fd status from poll?\n");
			rc = -1;
			break;
		}

		rc = ipmi_recv(ipmi, &resp_netfn, &resp_cmd, &seq,
				resp_buf, resp_len);
		if (rc)
			break;

		if (seq != ipmi->seq - 1) {
			pb_log("IPMI: out-of-sequence reply: "
					"exp %ld, got %ld\n",
					ipmi->seq, seq);

			if (timeout_ms) {
				gettimeofday(&now, NULL);
				timersub(&now, &start, &delta);
				expired_ms = (delta.tv_sec * 1000) +
						(delta.tv_usec / 1000);

				if (expired_ms >= timeout_ms) {
					rc = -1;
					break;
				}
			}
		} else {
			pb_debug("IPMI: netfn(%x->%x), cmd(%x->%x)\n",
					netfn, resp_netfn, cmd, resp_cmd);
			rc = 0;
			goto out;
		}
	}

out:
	lock.l_type = F_UNLCK;
	if (fcntl(ipmi->fd, F_SETLKW, &lock) == -1)
		pb_log("IPMI: error unlocking IPMI device: %m\n");
	return rc ? -1 : 0;
}

static int ipmi_destroy(void *p)
{
	struct ipmi *ipmi = p;
	close(ipmi->fd);
	return 1;
}

struct ipmi *ipmi_open(void *ctx)
{
	struct ipmi *ipmi;
	int fd;

	fd = open(ipmi_devnode, O_RDWR | O_CLOEXEC);
	if (fd < 0) {
		pb_log("IPMI: can't open IPMI device %s: %m\n", ipmi_devnode);
		return NULL;
	}

	ipmi = talloc(ctx, struct ipmi);
	ipmi->fd = fd;
	ipmi->seq = 0;

	talloc_set_destructor(ipmi, ipmi_destroy);

	return ipmi;
}

bool ipmi_present(void)
{
	return !access(ipmi_devnode, R_OK | W_OK);
}

/* Reads and applies an IPMI interface config override, which closely follows
 * the format of an interface config struct as described in lib/types */
int parse_ipmi_interface_override(struct config *config, uint8_t *buf,
				uint16_t len)
{
	struct interface_config *ifconf;
	char *ipstr, *gatewaystr;
	uint8_t hwsize, ipsize;
	int addr_type, i = 0;
	socklen_t addr_len;

	/* Get 1-byte hardware address size and ip address size */
	memcpy(&hwsize, &buf[i], sizeof(hwsize));
	i += sizeof(hwsize);
	memcpy(&ipsize, &buf[i], sizeof(ipsize));
	i += sizeof(ipsize);

	if (!hwsize || !ipsize) {
		pb_log_fn("Empty response\n");
		return -1;
	}

	/* At the moment only support 6-byte MAC addresses */
	if (hwsize != sizeof(ifconf->hwaddr)) {
		pb_log("Unsupported HW address size in network override: %u\n",
		       hwsize);
		return -1;
	}

	/* Sanity check the IP address size */
	if (ipsize == 4) {
		addr_type = AF_INET;
		addr_len = INET_ADDRSTRLEN;
	} else if (ipsize == 16) {
		addr_type = AF_INET6;
		addr_len = INET6_ADDRSTRLEN;
	} else {
		pb_log("Unsupported IP address size: %u\n", ipsize);
		return -1;
	}

	/* Everything past here is the interface config */
	ifconf = talloc_zero(config, struct interface_config);
	if (!ifconf) {
		pb_log("Failed to allocate network override\n");
		return -1;
	}

	/* Hardware Address */
	memcpy(ifconf->hwaddr, &buf[i], hwsize);
	i += hwsize;

	/* Check 1-byte ignore and method flags */
	ifconf->ignore = !!buf[i++];
	ifconf->method = !!buf[i++];

	if (ifconf->method == CONFIG_METHOD_STATIC) {
		if (ipsize + ipsize  + 1 > len - i) {
			pb_log("Expected data greater than buffer size\n");
			talloc_free(ifconf);
			return -1;
		}

		/* IP address */
		ipstr = talloc_array(ifconf, char, addr_len);
		if (!inet_ntop(addr_type, &buf[i], ipstr, addr_len)) {
			pb_log("Failed to convert ipaddr: %m\n");
			talloc_free(ifconf);
			return -1;
		}
		i += ipsize;

		/* IP address subnet */
		ifconf->static_config.address = talloc_asprintf(ifconf,
						"%s/%u", ipstr, buf[i]);
		i++;

		/* Gateway address */
		gatewaystr = talloc_array(ifconf, char, addr_len);
		if (!inet_ntop(addr_type, &buf[i], gatewaystr, addr_len)) {
			pb_log("Failed to convert gateway: %m\n");
			talloc_free(ifconf);
			return -1;
		}
		ifconf->static_config.gateway = gatewaystr;
	}

	ifconf->override = true;
	pb_log("Applying IPMI network interface override\n");

	/* Replace any existing interface config */
	talloc_free(config->network.interfaces);
	config->network.n_interfaces = 1;
	config->network.interfaces = talloc(config, struct interface_config *);
	config->network.interfaces[0] = ifconf;

	return 0;
}

void ipmi_get_bmc_mac(struct ipmi *ipmi, uint8_t *buf)
{
	uint16_t resp_len = 8;
	uint8_t resp[8];
	uint8_t req[] = { 0x1, 0x5, 0x0, 0x0 };
	char *debug_buf;
	int i, rc;

	rc = ipmi_transaction(ipmi, IPMI_NETFN_TRANSPORT,
			IPMI_CMD_TRANSPORT_GET_LAN_PARAMS,
			req, sizeof(req),
			resp, &resp_len,
			ipmi_timeout);

	debug_buf = format_buffer(ipmi, resp, resp_len);
	pb_debug_fn("BMC MAC resp [%d][%d]:\n%s\n",
			rc, resp_len, debug_buf);
	talloc_free(debug_buf);

	if (rc == 0 && resp_len > 0) {
		for (i = 2; i < resp_len; i++) {
			buf[i - 2] = resp[i];
		}
	}

}

/*
 * Retrieve info from the "Get Device ID" IPMI commands.
 * See Chapter 20.1 in the IPMIv2 specification.
 */
void ipmi_get_bmc_versions(struct ipmi *ipmi, struct system_info *info)
{
	uint16_t resp_len = 16;
	uint8_t resp[16], bcd;
	char *debug_buf;
	int rc;

	/* Retrieve info from current side */
	rc = ipmi_transaction(ipmi, IPMI_NETFN_APP,
			IPMI_CMD_APP_GET_DEVICE_ID,
			NULL, 0,
			resp, &resp_len,
			ipmi_timeout);

	debug_buf = format_buffer(ipmi, resp, resp_len);
	pb_debug_fn("BMC version resp [%d][%d]:\n%s\n",
			rc, resp_len, debug_buf);
	talloc_free(debug_buf);

	if (rc == 0 && (resp_len == 12 || resp_len == 16)) {
		info->bmc_current = talloc_array(info, char *, 4);
		info->n_bmc_current = 4;

		info->bmc_current[0] = talloc_asprintf(info, "Device ID: 0x%x",
						resp[1]);
		info->bmc_current[1] = talloc_asprintf(info, "Device Rev: 0x%x",
						resp[2]);
		bcd = resp[4] & 0x0f;
		bcd += 10 * (resp[4] >> 4);
		/* rev1.rev2.aux_revision */
		info->bmc_current[2] = talloc_asprintf(info,
				"Firmware version: %u.%02u",
				resp[3], bcd);
		if (resp_len == 16) {
			info->bmc_current[2] = talloc_asprintf_append(
					info->bmc_current[2],
					".%02x%02x%02x%02x",
					resp[12], resp[13], resp[14], resp[15]);
		}
		bcd = resp[5] & 0x0f;
		bcd += 10 * (resp[5] >> 4);
		info->bmc_current[3] = talloc_asprintf(info, "IPMI version: %u",
						bcd);
	} else
		pb_debug_fn("Failed to retrieve Device ID from IPMI\n");

	/* Retrieve info from golden side */
	memset(resp, 0, sizeof(resp));
	resp_len = 16;
	rc = ipmi_transaction(ipmi, IPMI_NETFN_AMI,
			IPMI_CMD_APP_GET_DEVICE_ID_GOLDEN,
			NULL, 0,
			resp, &resp_len,
			ipmi_timeout);

	debug_buf = format_buffer(ipmi, resp, resp_len);
	pb_debug_fn("BMC golden resp [%d][%d]:\n%s\n",
			rc, resp_len, debug_buf);
	talloc_free(debug_buf);

	if (rc == 0 && (resp_len == 12 || resp_len == 16)) {
		info->bmc_golden = talloc_array(info, char *, 4);
		info->n_bmc_golden = 4;

		info->bmc_golden[0] = talloc_asprintf(info, "Device ID: 0x%x",
						resp[1]);
		info->bmc_golden[1] = talloc_asprintf(info, "Device Rev: 0x%x",
						resp[2]);
		bcd = resp[4] & 0x0f;
		bcd += 10 * (resp[4] >> 4);
		/* rev1.rev2.aux_revision */
		info->bmc_golden[2] = talloc_asprintf(info,
				"Firmware version: %u.%02u",
				resp[3], bcd);
		if (resp_len == 16) {
			info->bmc_golden[2] = talloc_asprintf_append(
					info->bmc_golden[2],
					".%02x%02x%02x%02x",
					resp[12], resp[13], resp[14], resp[15]);
		}
		bcd = resp[5] & 0x0f;
		bcd += 10 * (resp[5] >> 4);
		info->bmc_golden[3] = talloc_asprintf(info, "IPMI version: %u",
						bcd);
	} else
		pb_debug_fn("Failed to retrieve Golden Device ID from IPMI\n");
}

