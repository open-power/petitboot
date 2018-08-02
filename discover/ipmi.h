#ifndef _IPMI_H
#define _IPMI_H

#include <stdbool.h>
#include <stdint.h>

#include <types/types.h>

enum ipmi_netfn {
	IPMI_NETFN_CHASSIS	= 0x0,
	IPMI_NETFN_SE		= 0x04,
	IPMI_NETFN_APP		= 0x06,
	IPMI_NETFN_TRANSPORT	= 0x0c,
	IPMI_NETFN_AMI		= 0x3a,
};

enum ipmi_cmd {
	IPMI_CMD_CHASSIS_SET_SYSTEM_BOOT_OPTIONS	= 0x08,
	IPMI_CMD_CHASSIS_GET_SYSTEM_BOOT_OPTIONS	= 0x09,
	IPMI_CMD_SENSOR_SET				= 0x30,
	IPMI_CMD_TRANSPORT_GET_LAN_PARAMS		= 0x02,
	IPMI_CMD_APP_GET_DEVICE_ID			= 0x01,
	IPMI_CMD_APP_GET_DEVICE_ID_GOLDEN		= 0x1a,
};

enum ipmi_sensor_ids {
	IPMI_SENSOR_ID_OS_BOOT		= 0x1F,
};

struct ipmi;

static const int ipmi_timeout = 10000; /* milliseconds. */

bool ipmi_present(void);
bool ipmi_bootdev_is_valid(int x);
struct ipmi *ipmi_open(void *ctx);

int ipmi_transaction(struct ipmi *ipmi, uint8_t netfn, uint8_t cmd,
		uint8_t *req_buf, uint16_t req_len,
		uint8_t *resp_buf, uint16_t *resp_len,
		int timeout_ms);

int parse_ipmi_interface_override(struct config *config, uint8_t *buf,
				uint16_t len);
void ipmi_get_bmc_mac(struct ipmi *ipmi, uint8_t *buf);
void ipmi_get_bmc_versions(struct ipmi *ipmi, struct system_info *info);


#endif /* _IPMI_H */
