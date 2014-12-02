#ifndef _IPMI_H
#define _IPMI_H

#include <stdbool.h>
#include <stdint.h>

enum ipmi_bootdev {
	IPMI_BOOTDEV_NONE = 0x0,
	IPMI_BOOTDEV_NETWORK = 0x1,
	IPMI_BOOTDEV_DISK = 0x2,
	IPMI_BOOTDEV_SAFE = 0x3,
	IPMI_BOOTDEV_CDROM = 0x5,
	IPMI_BOOTDEV_SETUP = 0x6,
};

bool ipmi_bootdev_is_valid(int x);
bool ipmi_present(void);

#endif /* _IPMI_H */
