#ifndef _DISCOVER_CLIENT_H
#define _DISCOVER_CLIENT_H

#include <types/types.h>
#include <waiter/waiter.h>

struct discover_client;

struct pb_boot_data {
	char *image;
	char *initrd;
	char *dtb;
	char *args;
};

/**
 * struct discover_client_ops - Application supplied client info.
 * @device_add: PB_PROTOCOL_ACTION_ADD event callback.
 * @device_remove: PB_PROTOCOL_ACTION_REMOVE event callback.
 * @cb_arg: Client managed convenience variable passed to callbacks.
 */

struct discover_client_ops {
	int (*device_add)(struct device *device, void *arg);
	int (*boot_option_add)(struct device *dev, struct boot_option *option,
			void *arg);
	void (*device_remove)(struct device *device, void *arg);
	void (*update_status)(struct boot_status *status, void *arg);
	void (*update_sysinfo)(struct system_info *sysinfo, void *arg);
	void *cb_arg;
};

struct discover_client *discover_client_init(struct waitset *waitset,
	const struct discover_client_ops *ops, void *cb_arg);

void discover_client_destroy(struct discover_client *client);

/**
 * Get the number of devices that the discover client has stored. This
 * is the set of devices that have been added and not removed
 *
 * @param client The discover client
 * @return	 The number of devices that have been added.
 */
int discover_client_device_count(struct discover_client *client);

/**
 * Get the device at a specific index.
 * @param client A pointer to the discover client
 * @param index  The index of the device to retrieve
 * @return	 The device at the specified index, or NULL if none exists
 */
struct device *discover_client_get_device(struct discover_client *client,
		int index);

/* Tell the discover server to boot an image
 * @param client A pointer to the discover client
 * @param boot_command The command to boot
 */
int discover_client_boot(struct discover_client *client,
		const struct device *device,
		const struct boot_option *boot_option,
		const struct pb_boot_data *data);

/* Tell the discover server to cancel the default boot option, if any
 */
int discover_client_cancel_default(struct discover_client *client);
#endif
