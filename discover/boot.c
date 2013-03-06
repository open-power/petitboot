
#include "boot.h"

int boot(void *ctx, struct boot_option *opt, struct boot_command *cmd,
		int dry_run)
{
	/* todo: run kexec with options from opt & cmd */
	(void)ctx;
	(void)opt;
	(void)cmd;
	(void)dry_run;

	return 0;
}
