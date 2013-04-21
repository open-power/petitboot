#ifndef _UDEV_H
#define _UDEV_H

struct pb_udev;
struct device_handler;
struct waitset;

struct pb_udev *udev_init(struct waitset *waitset,
	struct device_handler *handler);
int udev_trigger(struct pb_udev *udev);
void udev_destroy(struct pb_udev *udev);

#endif /* _UDEV_H */
