#ifndef _BOOT_H
#define _BOOT_H

struct boot_option;
struct boot_command;

int boot(void *ctx, struct discover_boot_option *opt, struct boot_command *cmd,
		int dry_run);

#endif /* _BOOT_H */
