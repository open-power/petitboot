
#include <libtwin/twin.h>

#define LOG(fmt...)	printf(fmt)

#define PBOOT_MAX_DEV		16
#define PBOOT_MAX_OPTION	16

int pboot_add_device(const char *dev_id, const char *name,
		twin_pixmap_t *pixmap);
int pboot_add_option(int devindex, const char *title,
		     const char *subtitle, twin_pixmap_t *badge, void *data);
int pboot_remove_device(const char *dev_id);

int pboot_start_device_discovery(int udev_trigger);
void pboot_exec_option(void *data);
void pboot_message(const char *message);
