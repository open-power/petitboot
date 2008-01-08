#ifndef _PATHS_H
#define _PATHS_H

#ifndef PREFIX
#define PREFIX "/usr"
#endif

#ifndef PKG_SHARE_DIR
#define PKG_SHARE_DIR PREFIX "/share/petitboot"
#endif

#ifndef TMP_DIR
#define TMP_DIR "/var/tmp/mnt/"
#endif

#define PBOOT_DEVICE_SOCKET "/var/tmp/petitboot-dev"
#define MOUNT_BIN "/bin/mount"
#define UMOUNT_BIN "/bin/umount"
#define BOOT_GAMEOS_BIN "/usr/bin/ps3-boot-game-os"

/* at present, all default artwork strings are const. */
#define artwork_pathname(s) (PKG_SHARE_DIR "/artwork/" s)

#endif /* _PATHS_H */
