#ifndef PATHS_H
#define PATHS_H

/**
 * Given a string (eg /dev/sda1, sda1 or UUID=B8E53381CA9EA0E3), parse the
 * device path (eg /dev/sda1). Any device descriptions read from config files
 * should be parsed into the path first.
 *
 * The cur_dev is provided for some remapping situations. If NULL is provided,
 * no remapping will be done.
 *
 * Returns a newly-allocated string.
 */
char *parse_device_path(const char *dev_str, const char *current_device);

/**
 * Get the mountpoint for a device.
 */
const char *mountpoint_for_device(const char *dev);

/**
 * Resolve a path given in a config file, to a path in the local filesystem.
 * Paths may be of the form:
 *  device:path (eg /dev/sda:/boot/vmlinux)
 *
 * or just a path:
 *  /boot/vmlinux
 * - in this case, the current mountpoint is used.
 *
 * Returns a newly-allocated string containing a full path to the file in path
 */
char *resolve_path(const char *path, const char *current_device);


/**
 * Set the base directory for newly-created mountpoints
 */
void set_mount_base(const char *path);

/**
 * Utility function for joining two paths. Adds a / between a and b if
 * required.
 *
 * Returns a newly-allocated string.
 */
char *join_paths(const char *a, const char *b);

/**
 * encode a disk label (or uuid) for use in a symlink.
 */
char *encode_label(const char *label);

#endif /* PATHS_H */
