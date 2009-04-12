#ifndef _UDEV_H
#define _UDEV_H

struct udev;
struct device_handler;

struct udev *udev_init(struct device_handler *handler);
int udev_trigger(struct udev *udev);
void udev_destroy(struct udev *udev);

#endif /* _UDEV_H */
