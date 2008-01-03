#ifndef PATHS_H
#define PATHS_H

/**
 * Get the mountpoint for a device
 */
const char *mountpoint_for_device(const char *dev_path);

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
char *resolve_path(const char *path, const char *current_mountpoint);

/**
 * Set the base directory for newly-created mountpoints
 */
void set_mount_base(const char *path);

#endif /* PATHS_H */
