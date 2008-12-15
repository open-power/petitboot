
#ifndef _MESSAGE_H
#define _MESSAGE_H

enum device_action {
	DEV_ACTION_ADD_DEVICE = 0,
	DEV_ACTION_ADD_OPTION = 1,
	DEV_ACTION_REMOVE_DEVICE = 2,
	DEV_ACTION_REMOVE_OPTION = 3
};

struct device {
	char *id;
	char *name;
	char *description;
	char *icon_file;

	struct boot_option {
		char *id;
		char *name;
		char *description;
		char *icon_file;
		char *boot_image_file;
		char *initrd_file;
		char *boot_args;
	} *options;
	int n_options;
};


#endif /* _MESSAGE_H */
