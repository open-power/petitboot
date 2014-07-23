#ifndef _TYPES_H
#define _TYPES_H

#include <stdbool.h>
#include <stdint.h>
#include <list/list.h>

enum device_type {
	DEVICE_TYPE_NETWORK,
	DEVICE_TYPE_DISK,
	DEVICE_TYPE_OPTICAL,
	DEVICE_TYPE_ANY,
	DEVICE_TYPE_UNKNOWN,
};

struct device {
	char		*id;
	enum device_type type;
	char		*name;
	char		*description;
	char		*icon_file;

	int		n_options;
	struct list	boot_options;

	void		*ui_info;
};

struct boot_option {
	char		*device_id;
	char		*id;
	char		*name;
	char		*description;
	char		*icon_file;
	char		*boot_image_file;
	char		*initrd_file;
	char		*dtb_file;
	char		*boot_args;
	bool		is_default;

	struct list_item	list;

	void		*ui_info;
};

struct boot_command {
	char *option_id;
	char *boot_image_file;
	char *initrd_file;
	char *dtb_file;
	char *boot_args;
};

struct boot_status {
	enum {
		BOOT_STATUS_INFO,
		BOOT_STATUS_ERROR,
	} type;
	char	*message;
	char	*detail;
	int	progress;
};

struct interface_info {
	unsigned int	hwaddr_size;
	uint8_t		*hwaddr;
	char		*name;
	bool		link;
};

struct blockdev_info {
	char		*name;
	char		*uuid;
	char		*mountpoint;
};

struct system_info {
	char			*type;
	char			*identifier;
	struct interface_info	**interfaces;
	unsigned int		n_interfaces;
	struct blockdev_info	**blockdevs;
	unsigned int		n_blockdevs;
};

#define HWADDR_SIZE	6

struct interface_config {
	uint8_t	hwaddr[HWADDR_SIZE];
	bool	ignore;
	enum {
		CONFIG_METHOD_DHCP,
		CONFIG_METHOD_STATIC,
	} method;
	union {
		struct {
		} dhcp_config;
		struct {
			char *address;
			char *gateway;
		} static_config;
	};
};

struct network_config {
	struct interface_config	**interfaces;
	unsigned int		n_interfaces;
	const char		**dns_servers;
	unsigned int		n_dns_servers;
};

struct boot_priority {
	/* Boot options with higher priority values will take precedence over
	 * lower values. Negative priorities signify "don't boot this by
	 * default".
	 */
	int			priority;
	enum device_type	type;
};

struct config {
	bool			autoboot_enabled;
	unsigned int		autoboot_timeout_sec;
	struct network_config	network;
	struct boot_priority	*boot_priorities;
	unsigned int		n_boot_priorities;
	char			*boot_device;
	char			*lang;

	/* not user-settable */
	bool			safe_mode;
	bool			debug;
};

#endif /* _TYPES_H */
