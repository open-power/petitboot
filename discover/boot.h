#ifndef _BOOT_H
#define _BOOT_H

#include <types/types.h>
#include "device-handler.h"

struct boot_option;
struct boot_command;

typedef void (*boot_status_fn)(void *arg, struct boot_status *);

struct boot_task *boot(void *ctx, struct discover_boot_option *opt,
		struct boot_command *cmd, int dry_run,
		boot_status_fn status_fn, void *status_arg);

void boot_cancel(struct boot_task *task);

struct boot_task {
	struct load_url_result *image;
	struct load_url_result *initrd;
	struct load_url_result *dtb;
	const char *local_image;
	const char *local_initrd;
	const char *local_dtb;
	char *local_image_override;
	char *local_initrd_override;
	char *local_dtb_override;
	const char *args;
	const char *boot_tty;
	boot_status_fn status_fn;
	void *status_arg;
	bool dry_run;
	bool cancelled;
	bool verify_signature;
	struct load_url_result *image_signature;
	struct load_url_result *initrd_signature;
	struct load_url_result *dtb_signature;
	struct load_url_result *cmdline_signature;
	const char *local_image_signature;
	const char *local_initrd_signature;
	const char *local_dtb_signature;
	const char *local_cmdline_signature;
};

enum {
	KEXEC_LOAD_SIG_SETUP_INVALID = 253,
	KEXEC_LOAD_SIGNATURE_FAILURE = 254,
};

#endif /* _BOOT_H */
