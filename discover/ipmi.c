
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <string.h>
#include <unistd.h>

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
	fcntl(ipmi->fd, F_SETLKW, &lock);
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

	fd = open(ipmi_devnode, O_RDWR);
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

