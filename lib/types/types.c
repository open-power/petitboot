#include <string.h>
#include <types/types.h>
#include <i18n/i18n.h>

const char *device_type_display_name(enum device_type type)
{
	switch (type) {
	case DEVICE_TYPE_DISK:
		return _("Disk");
	case DEVICE_TYPE_OPTICAL:
		return _("Optical");
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
	if (!strncmp(str, "optical", strlen("optical")))
		return DEVICE_TYPE_OPTICAL;
	if (!strncmp(str, "network", strlen("network")))
		return DEVICE_TYPE_NETWORK;
	if (!strncmp(str, "any", strlen("any")))
		return DEVICE_TYPE_ANY;

	return DEVICE_TYPE_UNKNOWN;
}
