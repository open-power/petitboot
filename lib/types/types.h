#ifndef _TYPES_H
#define _TYPES_H

#include <stdbool.h>
#include <list/list.h>

enum device_type {
	DEVICE_TYPE_NETWORK,
	DEVICE_TYPE_DISK,
	DEVICE_TYPE_OPTICAL,
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

#endif /* _TYPES_H */
