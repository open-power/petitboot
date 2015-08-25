#include <string.h>
#include <types/types.h>
#include <i18n/i18n.h>

const char *ipmi_bootdev_display_name(enum ipmi_bootdev bootdev)
{
	switch (bootdev) {
	case IPMI_BOOTDEV_NONE:
		return _("None");
	case IPMI_BOOTDEV_NETWORK:
		return _("Network");
	case IPMI_BOOTDEV_DISK:
		return _("Disk");
	case IPMI_BOOTDEV_SAFE:
		return _("Safe Mode");
	case IPMI_BOOTDEV_CDROM:
		return _("Optical");
	case IPMI_BOOTDEV_SETUP:
		return _("Setup Mode");
	default:
		return _("Unknown");
	}
}

const char *device_type_display_name(enum device_type type)
{
	switch (type) {
	case DEVICE_TYPE_DISK:
		return _("Disk");
	case DEVICE_TYPE_USB:
		return _("USB");
	case DEVICE_TYPE_OPTICAL:
		return _("CD/DVD");
	case DEVICE_TYPE_NETWORK:
		return _("Network");
	case DEVICE_TYPE_ANY:
		return _("Any");
	case DEVICE_TYPE_UNKNOWN:
	default:
		return _("Unknown");
	}
}

const char *device_type_name(enum device_type type)
{
	switch (type) {
	case DEVICE_TYPE_DISK:
		return "disk";
	case DEVICE_TYPE_USB:
		return "usb";
	case DEVICE_TYPE_OPTICAL:
		return "optical";
	case DEVICE_TYPE_NETWORK:
		return "network";
	case DEVICE_TYPE_ANY:
		return "any";
	case DEVICE_TYPE_UNKNOWN:
	default:
		return "unknown";
	}
}

enum device_type find_device_type(const char *str)
{
	if (!strncmp(str, "disk", strlen("disk")))
		return DEVICE_TYPE_DISK;
	if (!strncmp(str, "usb", strlen("usb")))
		return DEVICE_TYPE_USB;
	if (!strncmp(str, "optical", strlen("optical")))
		return DEVICE_TYPE_OPTICAL;
	if (!strncmp(str, "network", strlen("network")))
		return DEVICE_TYPE_NETWORK;
	if (!strncmp(str, "any", strlen("any")))
		return DEVICE_TYPE_ANY;

	return DEVICE_TYPE_UNKNOWN;
}
