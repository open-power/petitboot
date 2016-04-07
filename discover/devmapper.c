#include <talloc/talloc.h>
#include <types/types.h>
#include <log/log.h>
#include <errno.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "libdevmapper.h"
#include "devmapper.h"
#include "platform.h"

#define MERGE_INTERVAL_US	200000

struct target {
	uint64_t	start_sector;
	uint64_t	end_sector;
	char		*ttype;
	char		*params;
};

static unsigned long read_param_uint(struct discover_device *device,
				const char *param)
{
	unsigned long value = 0;
	const char *tmp;

	tmp = discover_device_get_param(device, param);
	if (!tmp) {
		pb_debug("Could not retrieve parameter '%s' for %s\n",
			 param, device->device_path);
		errno = EINVAL;
	} else {
		errno = 0;
		value = strtoul(tmp, NULL, 0);
	}

	/* Return errno and result directly */
	return value;
}

/* Return the number of sectors on a block device. Zero represents an error */
static uint64_t get_block_sectors(struct discover_device *device)
{
	unsigned long major, minor, sectors = 0;
	char *attr, *buf = NULL;
	struct stat sb;
	int fd = -1;
	ssize_t sz;

	sectors = read_param_uint(device, "ID_PART_ENTRY_SIZE");
	if (!errno)
		return (uint64_t)sectors;
	else
		pb_debug("Error reading sector count for %s: %m\n",
		       device->device_path);

	/* Either the udev property is missing or we failed to parse it.
	 * Instead try to directly read the size attribute out of sysfs */
	major = read_param_uint(device, "MAJOR");
	if (errno) {
		pb_debug("Error reading %s major number\n", device->device_path);
		return 0;
	}
	minor = read_param_uint(device, "MINOR");
	if (errno) {
		pb_debug("Error reading %s minor number\n", device->device_path);
		return 0;
	}

	attr = talloc_asprintf(device, "/sys/dev/block/%lu:%lu/size",
			       major, minor);
	if (stat(attr, &sb)) {
		pb_debug("Failed to stat %s, %m\n", attr);
		goto out;
	}

	fd = open(attr, O_RDONLY);
	if (fd < 0) {
		pb_debug("Failed to open sysfs attribute for %s\n",
			 device->device_path);
		goto out;
	}

	buf = talloc_array(device, char, sb.st_size);
	if (!buf) {
		pb_debug("Failed to allocate space for attr\n");
		goto out;
	}

	sz = read(fd, buf, sb.st_size);
	if (sz <= 0) {
		pb_debug("Failed to read sysfs attr: %m\n");
		goto out;
	}

	sectors = strtoul(buf, NULL, 0);
	if (errno) {
		pb_debug("Failed to read sectors from sysfs: %m\n");
		sectors = 0;
	}

out:
	close(fd);
	talloc_free(buf);
	talloc_free(attr);
	return (uint64_t)sectors;
}

/*
 * The system's libdm may or may not have udev sync support. Tell libdm
 * to manage the creation of device nodes itself rather than waiting on udev
 * to do it
 */
static inline int set_cookie(struct dm_task *task, uint32_t *cookie)
{
	uint16_t udev_rules = 0;
	*cookie = 0;

	dm_udev_set_sync_support(0);
	udev_rules |= DM_UDEV_DISABLE_DM_RULES_FLAG |
		DM_UDEV_DISABLE_SUBSYSTEM_RULES_FLAG;

	return dm_task_set_cookie(task, cookie, udev_rules);
}

static bool snapshot_merge_complete(const char *dm_name)
{
	uint64_t sectors, meta_sectors;
	char *params = NULL,  *target_type = NULL;
	uint64_t start, length;
	struct dm_task *task;
	bool result = true;
	int n;

	task = dm_task_create(DM_DEVICE_STATUS);
	if (!task) {
		pb_log("%s: Error creating task\n", __func__);
		return result;
	}

	if (!dm_task_set_name(task, dm_name)) {
		pb_log("No dm-device named '%s'\n", dm_name);
		goto out;
	}

	if (!dm_task_run(task)) {
		pb_log("Unable to retrieve status for '%s'\n", dm_name);
		goto out;
	}

	dm_get_next_target(task, NULL, &start, &length, &target_type, &params);

	if (!params) {
		pb_log("Unable to retrieve params for '%s'\n", dm_name);
		goto out;
	}

	if (!strncmp(params, "Invalid", strlen("Invalid"))) {
		pb_log("dm-device %s has become invalid\n", dm_name);
		goto out;
	}

	/* Merge is complete when metadata sectors are the only sectors
	 * allocated - see Documentation/device-mapper/snapshot.txt */
	n = sscanf(params, "%" SCNu64 "/%*u %" SCNu64,
			&sectors, &meta_sectors);
	if (n != 2) {
		pb_log("%s unexpected status: '%s'\n", dm_name, params);
		goto out;
	}
	result = sectors == meta_sectors;

	pb_debug("%s merging; %" PRIu64 " sectors, %" PRIu64
			" metadata sectors\n",
		 dm_name, sectors, meta_sectors);

out:
	/* In case of error or an invalid snapshot return true so callers will
	 * move on and catch the error */
	dm_task_destroy(task);
	return result;
}

/* Resume or suspend dm device */
static int set_device_active(const char *dm_name, bool active)
{
	struct dm_task *task;
	uint32_t cookie;
	int rc = -1;

	if (active)
		task = dm_task_create(DM_DEVICE_RESUME);
	else
		task = dm_task_create(DM_DEVICE_SUSPEND);

	if (!task) {
		pb_log("%s: Could not create dm_task\n", __func__);
		return rc;
	}

	if (!dm_task_set_name(task, dm_name)) {
		pb_log("No dm-device named '%s'\n", dm_name);
		goto out;
	}

	if (!set_cookie(task, &cookie))
		goto out;

	if (!dm_task_run(task)) {
		pb_log("Unable to %s device '%s'\n",
		       active ? "resume" : "suspend", dm_name);
		goto out;
	}

	rc = 0;

	/* Wait for /dev/mapper/ entries to be updated */
	dm_udev_wait(cookie);

out:
	dm_task_destroy(task);
	return rc;
}

/* Run a DM_DEVICE_CREATE task with provided table (ttype and params) */
static int run_create_task(const char *dm_name, const struct target *target)
{
	struct dm_task *task;
	uint32_t cookie;

	pb_debug("%s: %lu %lu '%s' '%s'\n", __func__,
		 target->start_sector, target->end_sector,
		 target->ttype, target->params);

	task = dm_task_create(DM_DEVICE_CREATE);
	if (!task) {
		pb_log("Error creating new dm-task\n");
		return -1;
	}

	if (!dm_task_set_name(task, dm_name))
		return -1;

	if (!dm_task_add_target(task, target->start_sector, target->end_sector,
				target->ttype, target->params))
		return -1;

	if (!dm_task_set_add_node(task, DM_ADD_NODE_ON_CREATE))
		return -1;

	if (!set_cookie(task, &cookie))
		return -1;

	if (!dm_task_run(task)) {
		pb_log("Error executing dm-task\n");
		return -1;
	}

	/* Wait for /dev/mapper/ entry to appear */
	dm_udev_wait(cookie);

	dm_task_destroy(task);
	return 0;
}

static int create_base(struct discover_device *device)
{
	struct target target;
	char *name = NULL;
	int rc = -1;

	if (!device->ramdisk)
		return rc;

	target.start_sector = 0;
	target.end_sector = device->ramdisk->sectors;

	target.ttype = talloc_asprintf(device,  "linear");
	target.params = talloc_asprintf(device, "%s 0", device->device_path);
	if (!target.ttype || !target.params) {
		pb_log("Failed to allocate map parameters\n");
		goto out;
	}

	name = talloc_asprintf(device, "%s-base", device->device->id);
	if (!name || run_create_task(name, &target))
		goto out;

	device->ramdisk->base = talloc_asprintf(device, "/dev/mapper/%s-base",
					device->device->id);
	if (!device->ramdisk->base) {
		pb_log("Failed to track new device /dev/mapper/%s-base\n",
			device->device->id);
		goto out;
	}

	rc = 0;

out:
	talloc_free(name);
	talloc_free(target.params);
	talloc_free(target.ttype);
	return rc;
}

static int create_origin(struct discover_device *device)
{
	struct target target;
	char *name = NULL;
	int rc = -1;

	if (!device->ramdisk || !device->ramdisk->base)
		return -1;

	target.start_sector = 0;
	target.end_sector = device->ramdisk->sectors;

	target.ttype = talloc_asprintf(device,  "snapshot-origin");
	target.params = talloc_asprintf(device, "%s", device->ramdisk->base);
	if (!target.ttype || !target.params) {
		pb_log("Failed to allocate map parameters\n");
		goto out;
	}

	name = talloc_asprintf(device, "%s-origin", device->device->id);
	if (!name || run_create_task(name, &target))
		goto out;

	device->ramdisk->origin = talloc_asprintf(device,
					"/dev/mapper/%s-origin",
					device->device->id);
	if (!device->ramdisk->origin) {
		pb_log("Failed to track new device /dev/mapper/%s-origin\n",
		       device->device->id);
		goto out;
	}

	rc = 0;

out:
	talloc_free(name);
	talloc_free(target.params);
	talloc_free(target.ttype);
	return rc;
}

static int create_snapshot(struct discover_device *device)
{
	struct target target;
	int rc = -1;

	if (!device->ramdisk || !device->ramdisk->base ||
	    !device->ramdisk->origin)
		return -1;

	target.start_sector = 0;
	target.end_sector = device->ramdisk->sectors;

	target.ttype = talloc_asprintf(device,  "snapshot");
	target.params = talloc_asprintf(device, "%s %s P 8",
		 device->ramdisk->base, device->ramdisk->path);
	if (!target.ttype || !target.params) {
		pb_log("Failed to allocate snapshot parameters\n");
		goto out;
	}

	if (run_create_task(device->device->id, &target))
		goto out;

	device->ramdisk->snapshot = talloc_asprintf(device, "/dev/mapper/%s",
						device->device->id);
	if (!device->ramdisk->snapshot) {
		pb_log("Failed to track new device /dev/mapper/%s\n",
		       device->device->id);
		goto out;
	}

	rc = 0;

out:
	talloc_free(target.params);
	talloc_free(target.ttype);
	return rc;
}

int devmapper_init_snapshot(struct device_handler *handler,
		     struct discover_device *device)
{
	struct ramdisk_device *ramdisk;

	if (config_get()->disable_snapshots)
		return 0;

	ramdisk = device_handler_get_ramdisk(handler);
	if (!ramdisk) {
		pb_log("No ramdisk available for snapshot %s\n",
		       device->device->id);
		return -1;
	}

	ramdisk->sectors = get_block_sectors(device);
	if (!ramdisk->sectors) {
		pb_log("Error retreiving sectors for %s\n",
		       device->device->id);
		return -1;
	}

	device->ramdisk = ramdisk;

	/* Create linear map */
	if (create_base(device)) {
		pb_log("Error creating linear base\n");
		goto err;
	}

	/* Create snapshot-origin */
	if (create_origin(device)) {
		pb_log("Error creating snapshot-origin\n");
		goto err;
	}

	if (set_device_active(device->ramdisk->origin, false)) {
		pb_log("Failed to suspend origin\n");
		goto err;
	}

	/* Create snapshot */
	if (create_snapshot(device)) {
		pb_log("Error creating snapshot\n");
		goto err;
	}

	if (set_device_active(device->ramdisk->origin, true)) {
		pb_log("Failed to resume origin\n");
		goto err;
	}

	pb_log("Snapshot successfully created for %s\n", device->device->id);

	return 0;

err:
	pb_log("Error creating snapshot devices for %s\n", device->device->id);
	devmapper_destroy_snapshot(device);
	return -1;
}

/* Destroy specific dm device */
static int destroy_device(const char *dm_name)
{
	struct dm_task *task;
	uint32_t cookie;
	int rc = -1;

	task = dm_task_create(DM_DEVICE_REMOVE);
	if (!task) {
		pb_log("%s: could not create dm_task\n", __func__);
		return -1;
	}

	if (!dm_task_set_name(task, dm_name)) {
		pb_log("No dm device named '%s'\n", dm_name);
		goto out;
	}

	if (!set_cookie(task, &cookie))
		goto out;

	if (!dm_task_run(task)) {
		pb_log("Unable to remove device '%s'\n", dm_name);
		goto out;
	}

	rc = 0;

	/* Wait for /dev/mapper/ entries to be removed */
	dm_udev_wait(cookie);

out:
	dm_task_destroy(task);
	return rc;
}

/* Destroy all dm devices related to a discover_device's snapshot */
int devmapper_destroy_snapshot(struct discover_device *device)
{
	int rc = -1;

	if (!device->ramdisk)
		return 0;

	if (device->mounted) {
		pb_log("Can not remove snapshot: %s is mounted\n",
		       device->device->id);
		return -1;
	}

	/* Clean up dm devices in order */
	if (device->ramdisk->snapshot)
		if (destroy_device(device->ramdisk->snapshot))
			goto out;

	if (device->ramdisk->origin)
		if (destroy_device(device->ramdisk->origin))
			goto out;

	if (device->ramdisk->base)
		if (destroy_device(device->ramdisk->base))
			goto out;

	rc = 0;
out:
	if (rc)
		pb_log("Warning: %s snapshot not cleanly removed\n",
		       device->device->id);
	device_handler_release_ramdisk(device);
	return rc;
}

static int reload_snapshot(struct discover_device *device, bool merge)
{
	struct target target;
	struct dm_task *task;
	int rc = -1;

	target.start_sector = 0;
	target.end_sector = device->ramdisk->sectors;

	if (merge) {
		target.ttype = talloc_asprintf(device,  "snapshot-merge");
		target.params = talloc_asprintf(device, "%s %s P 8",
			 device->ramdisk->base, device->ramdisk->path);
	} else {
		target.ttype = talloc_asprintf(device,  "snapshot-origin");
		target.params = talloc_asprintf(device, "%s",
			 device->ramdisk->base);
	}
	if (!target.ttype || !target.params) {
		pb_log("%s: failed to allocate parameters\n", __func__);
		goto err1;
	}

	task = dm_task_create(DM_DEVICE_RELOAD);
	if (!task) {
		pb_log("%s: Error creating task\n", __func__);
		goto err1;
	}

	if (!dm_task_set_name(task, device->ramdisk->origin)) {
		pb_log("No dm-device named '%s'\n", device->ramdisk->origin);
		goto err2;
	}

	if (!dm_task_add_target(task, target.start_sector, target.end_sector,
				target.ttype, target.params)) {
		pb_log("%s: Failed to set target\n", __func__);
		goto err2;
	}

	if (!dm_task_run(task)) {
		pb_log("Failed to reload %s\n", device->ramdisk->origin);
		goto err2;
	}

	rc = 0;
err2:
	dm_task_destroy(task);
err1:
	talloc_free(target.ttype);
	talloc_free(target.params);
	return rc;
}

int devmapper_merge_snapshot(struct discover_device *device)
{
	if (device->mounted) {
		pb_log("%s: %s still mounted\n", __func__, device->device->id);
		return -1;
	}

	/* Suspend origin device */
	if (set_device_active(device->ramdisk->origin, false)) {
		pb_log("%s: failed to suspend %s\n",
		       __func__, device->ramdisk->origin);
		return -1;
	}

	/* Destroy snapshot */
	if (destroy_device(device->ramdisk->snapshot)) {
		/* The state of the snapshot is unknown, but try to
		 * resume to allow the snapshot to be remounted */
		set_device_active(device->ramdisk->origin, true);
		return -1;
	}
	talloc_free(device->ramdisk->snapshot);
	device->ramdisk->snapshot = NULL;

	/* Reload origin device for merging */
	reload_snapshot(device, true);

	/* Resume origin device */
	set_device_active(device->ramdisk->origin, true);

	/* Block until merge complete */
	while (!snapshot_merge_complete(device->ramdisk->origin))
		usleep(MERGE_INTERVAL_US);

	/* Suspend origin device */
	set_device_active(device->ramdisk->origin, false);

	/* Reload origin device */
	reload_snapshot(device, false);

	/* Re-create snapshot */
	if (create_snapshot(device))
		return -1;

	/* Resume origin device */
	return set_device_active(device->ramdisk->origin, true);
}
