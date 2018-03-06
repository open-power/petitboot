#ifndef _BOOT_H
#define _BOOT_H

#include <types/types.h>
#include "device-handler.h"

struct boot_option;
struct boot_command;

typedef void (*boot_status_fn)(void *arg, struct status *);

struct boot_task *boot(void *ctx, struct discover_boot_option *opt,
		struct boot_command *cmd, int dry_run,
		boot_status_fn status_fn, void *status_arg);

void boot_cancel(struct boot_task *task);

struct boot_task {
	const char *local_image;
	const char *local_initrd;
	const char *local_dtb;
	char *local_image_override;
	char *local_initrd_override;
	char *local_dtb_override;
	const char *args;
	const char *boot_console;
	boot_status_fn status_fn;
	void *status_arg;
	bool dry_run;
	bool cancelled;
	bool verify_signature;
	bool decrypt_files;
	const char *local_image_signature;
	const char *local_initrd_signature;
	const char *local_dtb_signature;
	const char *local_cmdline_signature;
	struct list resources;
};

struct boot_resource {
	struct load_url_result *result;
	struct pb_url *url;
	const char **local_path;
	const char *name;

	struct list_item list;
};

enum {
	KEXEC_LOAD_DECRYPTION_FALURE = 252,
	KEXEC_LOAD_SIG_SETUP_INVALID = 253,
	KEXEC_LOAD_SIGNATURE_FAILURE = 254,
};

#endif /* _BOOT_H */
