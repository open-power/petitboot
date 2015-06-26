#ifndef _IPMI_H
#define _IPMI_H

#include <stdbool.h>
#include <stdint.h>

#include <types/types.h>

enum ipmi_netfn {
	IPMI_NETFN_CHASSIS	= 0x0,
	IPMI_NETFN_SE		= 0x04,
};

enum ipmi_cmd {
	IPMI_CMD_CHASSIS_SET_SYSTEM_BOOT_OPTIONS	= 0x08,
	IPMI_CMD_CHASSIS_GET_SYSTEM_BOOT_OPTIONS	= 0x09,
	IPMI_CMD_SENSOR_SET				= 0x30,
};

enum ipmi_sensor_ids {
	IPMI_SENSOR_ID_OS_BOOT		= 0x1F,
};

struct ipmi;

bool ipmi_present(void);
bool ipmi_bootdev_is_valid(int x);
struct ipmi *ipmi_open(void *ctx);

int ipmi_transaction(struct ipmi *ipmi, uint8_t netfn, uint8_t cmd,
		uint8_t *req_buf, uint16_t req_len,
		uint8_t *resp_buf, uint16_t *resp_len,
		int timeout_ms);

#endif /* _IPMI_H */
