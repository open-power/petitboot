
#include "ipmi.h"

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

bool ipmi_present(void)
{
	return false;
}

