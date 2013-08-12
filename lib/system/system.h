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
};

extern const struct pb_system_apps pb_system_apps;

int pb_run_cmd(const char *const *cmd_argv, int wait, int dry_run);
int pb_mkdir_recursive(const char *dir);
int pb_rmdir_recursive(const char *base, const char *dir);

#endif
