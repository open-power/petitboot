#ifndef _UDEV_H
#define _UDEV_H

enum udev_action {
	UDEV_ACTION_ADD,
	UDEV_ACTION_REMOVE,
};

struct udev_event {
	enum udev_action action;
	char *device;

	struct param {
		char *name;
		char *value;
	} *params;
	int n_params;
};

struct udev;
struct device_handler;

struct udev *udev_init(struct device_handler *handler);
int udev_trigger(struct udev *udev);

void udev_destroy(struct udev *udev);

const char *udev_event_param(struct udev_event *event, const char *name);
#endif /* _UDEV_H */
