
#include <stdio.h>

#include "ui/common/discover-client.h"
#include "ui/common/device.h"

static int print_device_add(struct device *device)
{
	int i;

	printf("new device:\n");
	printf("\tid:   %s\n", device->id);
	printf("\tname: %s\n", device->name);
	printf("\tdesc: %s\n", device->description);
	printf("\ticon: %s\n", device->icon_file);

	printf("\t%d boot options:\n", device->n_options);
	for (i = 0; i < device->n_options; i++) {
		struct boot_option *opt = &device->options[i];
		printf("\t\tid:   %s\n", opt->id);
		printf("\t\tname: %s\n", opt->name);
		printf("\t\tdesc: %s\n", opt->description);
		printf("\t\ticon: %s\n", opt->icon_file);
		printf("\t\tboot: %s\n", opt->boot_image_file);
		printf("\t\tinit: %s\n", opt->initrd_file);
		printf("\t\targs: %s\n", opt->boot_args);
	}

	return 0;
}

static void print_device_remove(const char *dev_id)
{
	printf("removed device:\n");
	printf("\tid:   %s\n", dev_id);
}

struct discover_client_ops client_ops = {
	.add_device = print_device_add,
	.remove_device = print_device_remove,
};

int main(void)
{
	struct discover_client *client;

	client = discover_client_init(&client_ops);
	if (!client)
		return -1;

	for (;;) {
		int rc;

		rc = discover_client_process(client);
		if (rc)
			break;
	}

	return 0;
}
