#ifndef _UDEV_H
#define _UDEV_H

struct pb_udev;
struct device_handler;
struct waitset;

struct pb_udev *udev_init(struct device_handler *handler,
		struct waitset *waitset);

void udev_reinit(struct pb_udev *udev);

#endif /* _UDEV_H */
