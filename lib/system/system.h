#if !defined(_PB_LIB_SYSTEM_H)
#define _PB_LIB_SYSTEM_H

struct pb_system_apps {
	const char *prefix;
	const char *cp;
	const char *kexec;
	const char *mount;
	const char *shutdown;
	const char *sftp;
	const char *tftp;
	const char *umount;
	const char *wget;
	const char *ip;
	const char *udhcpc;
	const char *vgscan;
	const char *vgchange;
	const char *pb_plugin;
	const char *pb_exec;
	const char *sh;
	const char *scsi_rescan;
};

extern const struct pb_system_apps pb_system_apps;

enum tftp_type {
	TFTP_TYPE_BUSYBOX,
	TFTP_TYPE_HPA,
	TFTP_TYPE_UNKNOWN,
	TFTP_TYPE_BROKEN,
};

extern enum tftp_type tftp_type;

int pb_run_cmd(const char *const *cmd_argv, int wait, int dry_run);
int pb_mkdir_recursive(const char *dir);
int pb_rmdir_recursive(const char *base, const char *dir);

#endif
