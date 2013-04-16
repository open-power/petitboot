
#include <assert.h>

#include <log/log.h>
#include <pb-protocol/pb-protocol.h>
#include <system/system.h>
#include <talloc/talloc.h>
#include <url/url.h>

#include "device-handler.h"
#include "boot.h"
#include "paths.h"
#include "resource.h"

/**
 * kexec_load - kexec load helper.
 * @l_image: The local image file for kexec to execute.
 * @l_initrd: Optional local initrd file for kexec --initrd, can be NULL.
 * @args: Optional command line args for kexec --append, can be NULL.
 */

static int kexec_load(const char *l_image, const char *l_initrd,
	const char *args, int dry_run)
{
	int result;
	const char *argv[6];
	const char **p;
	char *s_initrd = NULL;
	char *s_args = NULL;

	p = argv;
	*p++ = pb_system_apps.kexec;	/* 1 */
	*p++ = "-l";			/* 2 */

	if (l_initrd) {
		s_initrd = talloc_asprintf(NULL, "--initrd=%s", l_initrd);
		assert(s_initrd);
		*p++ = s_initrd;	 /* 3 */
	}

	if (args) {
		s_args = talloc_asprintf(NULL, "--append=%s", args);
		assert(s_args);
		*p++ = s_args;		/* 4 */
	}

	*p++ = l_image;			/* 5 */
	*p++ = NULL;			/* 6 */

	result = pb_run_cmd(argv, 1, dry_run);

	if (result)
		pb_log("%s: failed: (%d)\n", __func__, result);

	talloc_free(s_initrd);
	talloc_free(s_args);

	return result;
}

/**
 * kexec_reboot - Helper to boot the new kernel.
 *
 * Must only be called after a successful call to kexec_load().
 */

static int kexec_reboot(int dry_run)
{
	int result = 0;
	const char *argv[4];
	const char **p;

	/* First try running shutdown.  Init scripts should run 'exec -e' */

	p = argv;
	*p++ = pb_system_apps.shutdown;	/* 1 */
	*p++ =  "-r";			/* 2 */
	*p++ =  "now";			/* 3 */
	*p++ =  NULL;			/* 4 */

	result = pb_run_cmd(argv, 1, dry_run);

	/* On error, force a kexec with the -e option */

	if (result) {
		p = argv;
		*p++ = pb_system_apps.kexec;	/* 1 */
		*p++ = "-e";			/* 2 */
		*p++ = NULL;			/* 3 */

		result = pb_run_cmd(argv, 1, 0);
	}

	if (result)
		pb_log("%s: failed: (%d)\n", __func__, result);

	return result;
}

int boot(void *ctx, struct discover_boot_option *opt, struct boot_command *cmd,
		int dry_run)
{
	char *local_image, *local_initrd;
	unsigned int clean_image = 0;
	unsigned int clean_initrd = 0;
	struct pb_url *image, *initrd;
	char *args;
	int result;

	local_initrd = NULL;
	image = NULL;
	initrd = NULL;
	args = NULL;

	if (cmd->boot_image_file) {
		image = pb_url_parse(opt, cmd->boot_image_file);
	} else if (opt && opt->boot_image) {
		image = opt->boot_image->url;
	} else {
		pb_log("%s: no image specified", __func__);
		return -1;
	}

	if (cmd->initrd_file) {
		initrd = pb_url_parse(opt, cmd->initrd_file);
	} else if (opt && opt->initrd) {
		initrd = opt->initrd->url;
	}

	if (cmd->boot_args) {
		args = talloc_strdup(ctx, cmd->boot_args);
	} else if (opt && opt->option->boot_args) {
		args = talloc_strdup(ctx, opt->option->boot_args);
	}

	result = -1;

	local_image = load_url(NULL, image, &clean_image);
	if (!local_image)
		goto no_load;

	if (initrd) {
		local_initrd = load_url(NULL, initrd, &clean_initrd);
		if (!local_initrd)
			goto no_load;
	}

	result = kexec_load(local_image, local_initrd, args, dry_run);

no_load:
	if (clean_image)
		unlink(local_image);
	if (clean_initrd)
		unlink(local_initrd);

	talloc_free(local_image);
	talloc_free(local_initrd);

	if (!result)
		result = kexec_reboot(dry_run);

	return result;
}
