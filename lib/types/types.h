#ifndef _TYPES_H
#define _TYPES_H

#include <stdbool.h>
#include <stdint.h>
#include <list/list.h>

enum device_type {
	DEVICE_TYPE_NETWORK,
	DEVICE_TYPE_DISK,
	DEVICE_TYPE_USB,
	DEVICE_TYPE_OPTICAL,
	DEVICE_TYPE_ANY,
	DEVICE_TYPE_UNKNOWN,
};

enum ipmi_bootdev {
	IPMI_BOOTDEV_NONE = 0x00,
	IPMI_BOOTDEV_NETWORK = 0x01,
	IPMI_BOOTDEV_DISK = 0x2,
	IPMI_BOOTDEV_SAFE = 0x3,
	IPMI_BOOTDEV_CDROM = 0x5,
	IPMI_BOOTDEV_SETUP = 0x6,
	IPMI_BOOTDEV_INVALID = 0xff,
};

const char *ipmi_bootdev_display_name(enum ipmi_bootdev bootdev);
const char *device_type_display_name(enum device_type type);
const char *device_type_name(enum device_type type);
enum device_type find_device_type(const char *str);

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
	char		*args_sig_file;
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
	char *args_sig_file;
	char *console;
};

struct status {
	enum status_type {
		STATUS_INFO,
		STATUS_ERROR,
	} type;
	char	*message;
	bool	backlog;
};

struct statuslog_entry {
	struct status		*status;
	struct list_item	list;
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
	char			**platform_primary;
	char			**platform_other;
	unsigned int		n_primary;
	unsigned int		n_other;
	char			**bmc_current;
	char			**bmc_golden;
	unsigned int		n_bmc_current;
	unsigned int		n_bmc_golden;
	uint8_t			*bmc_mac;
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
			char *url;
		} static_config;
	};
	bool	override;
};

struct network_config {
	struct interface_config	**interfaces;
	unsigned int		n_interfaces;
	const char		**dns_servers;
	unsigned int		n_dns_servers;
};

struct autoboot_option {
	enum {
		BOOT_DEVICE_TYPE,
		BOOT_DEVICE_UUID
	} boot_type;
	union {
		enum device_type	type;
		char			*uuid;
	};
};

struct config {
	bool			autoboot_enabled;
	unsigned int		autoboot_timeout_sec;
	struct network_config	network;

	struct autoboot_option	*autoboot_opts;
	unsigned int		n_autoboot_opts;

	unsigned int		ipmi_bootdev;
	bool			ipmi_bootdev_persistent;

	char			*http_proxy;
	char			*https_proxy;

	bool			allow_writes;

	char			*boot_console;
	bool			manual_console;
	char			*lang;

	/* not user-settable */
	unsigned int		n_consoles;
	char			**consoles;
	bool			disable_snapshots;
	bool			safe_mode;
	bool			debug;
};

#endif /* _TYPES_H */
