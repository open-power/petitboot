
#define _GNU_SOURCE

#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>
#include <dirent.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>

#include <log/log.h>
#include <pb-protocol/pb-protocol.h>
#include <process/process.h>
#include <system/system.h>
#include <talloc/talloc.h>
#include <url/url.h>

#include "device-handler.h"
#include "boot.h"
#include "paths.h"
#include "resource.h"

static const char *boot_hook_dir = PKG_SYSCONF_DIR "/boot.d";
enum {
	BOOT_HOOK_EXIT_OK	= 0,
	BOOT_HOOK_EXIT_UPDATE	= 2,
};

struct boot_task {
	struct load_url_result *image;
	struct load_url_result *initrd;
	struct load_url_result *dtb;
	const char *args;
	boot_status_fn status_fn;
	void *status_arg;
	bool dry_run;
};

/**
 * kexec_load - kexec load helper.
 */
static int kexec_load(struct boot_task *boot_task)
{
	int result;
	const char *argv[7];
	const char **p;
	char *s_initrd = NULL;
	char *s_dtb = NULL;
	char *s_args = NULL;

	p = argv;
	*p++ = pb_system_apps.kexec;	/* 1 */
	*p++ = "-l";			/* 2 */

	if (boot_task->initrd) {
		s_initrd = talloc_asprintf(boot_task, "--initrd=%s",
				boot_task->initrd->local);
		assert(s_initrd);
		*p++ = s_initrd;	 /* 3 */
	}

	if (boot_task->dtb) {
		s_dtb = talloc_asprintf(boot_task, "--dtb=%s",
						boot_task->dtb->local);
		assert(s_dtb);
		*p++ = s_dtb;		 /* 4 */
	}

	if (boot_task->args) {
		s_args = talloc_asprintf(boot_task, "--append=%s",
						boot_task->args);
		assert(s_args);
		*p++ = s_args;		/* 5 */
	}

	*p++ = boot_task->image->local;	/* 6 */
	*p++ = NULL;			/* 7 */

	result = process_run_simple_argv(boot_task, argv);

	if (result)
		pb_log("%s: failed: (%d)\n", __func__, result);

	return result;
}

/**
 * kexec_reboot - Helper to boot the new kernel.
 *
 * Must only be called after a successful call to kexec_load().
 */

static int kexec_reboot(struct boot_task *task)
{
	int result;

	/* First try running shutdown.  Init scripts should run 'exec -e' */
	result = process_run_simple(task, pb_system_apps.shutdown, "-r",
			"now", NULL);

	/* On error, force a kexec with the -e option */
	if (result) {
		result = process_run_simple(task, pb_system_apps.kexec,
						"-e", NULL);
	}

	if (result)
		pb_log("%s: failed: (%d)\n", __func__, result);

	/* okay, kexec -e -f */
	if (result) {
		result = process_run_simple(task, pb_system_apps.kexec,
						"-e", "-f", NULL);
	}

	if (result)
		pb_log("%s: failed: (%d)\n", __func__, result);


	return result;
}

static void __attribute__((format(__printf__, 4, 5))) update_status(
		boot_status_fn fn, void *arg, int type, char *fmt, ...)
{
	struct boot_status status;
	va_list ap;

	va_start(ap, fmt);
	status.message = talloc_vasprintf(NULL, fmt, ap);
	va_end(ap);

	status.type = type;
	status.progress = -1;
	status.detail = NULL;

	fn(arg, &status);

	talloc_free(status.message);
}

static void boot_hook_update_param(void *ctx, struct boot_task *task,
		const char *name, const char *value)
{
	struct p {
		const char *name;
		const char **p;
	} *param, params[] = {
		{ "boot_image",		&task->image->local },
		{ "boot_initrd",	&task->initrd->local },
		{ "boot_dtb",		&task->dtb->local },
		{ "boot_args",		&task->args },
		{ NULL, NULL },
	};

	for (param = params; param->name; param++) {
		if (strcmp(param->name, name))
			continue;

		*param->p = talloc_strdup(ctx, value);
		return;
	}
}

static void boot_hook_update(struct boot_task *task, const char *hookname,
		char *buf)
{
	char *line, *name, *val, *sep;
	char *saveptr;

	for (;; buf = NULL) {

		line = strtok_r(buf, "\n", &saveptr);
		if (!line)
			break;

		sep = strchr(line, '=');
		if (!sep)
			continue;

		*sep = '\0';
		name = line;
		val = sep + 1;

		boot_hook_update_param(task, task, name, val);

		pb_log("boot hook %s specified %s=%s\n",
				hookname, name, val);
	}
}

static void boot_hook_setenv(struct boot_task *task)
{
	unsetenv("boot_image");
	unsetenv("boot_initrd");
	unsetenv("boot_dtb");
	unsetenv("boot_args");

	setenv("boot_image", task->image->local, 1);
	if (task->initrd)
		setenv("boot_initrd", task->initrd->local, 1);
	if (task->dtb)
		setenv("boot_dtb", task->dtb->local, 1);
	if (task->args)
		setenv("boot_args", task->args, 1);
}

static int hook_filter(const struct dirent *dirent)
{
	return dirent->d_type == DT_REG || dirent->d_type == DT_LNK;
}

static int hook_cmp(const struct dirent **a, const struct dirent **b)
{
	return strcmp((*a)->d_name, (*b)->d_name);
}

static void run_boot_hooks(struct boot_task *task)
{
	struct dirent **hooks;
	int i, n;

	n = scandir(boot_hook_dir, &hooks, hook_filter, hook_cmp);
	if (n < 1)
		return;

	update_status(task->status_fn, task->status_arg, BOOT_STATUS_INFO,
			"running boot hooks");

	boot_hook_setenv(task);

	for (i = 0; i < n; i++) {
		const char *argv[2] = { NULL, NULL };
		struct process *process;
		char *path;
		int rc;

		path = join_paths(task, boot_hook_dir, hooks[i]->d_name);

		if (access(path, X_OK)) {
			talloc_free(path);
			continue;
		}

		process = process_create(task);

		argv[0] = path;
		process->path = path;
		process->argv = argv;
		process->keep_stdout = true;

		pb_log("running boot hook %s\n", hooks[i]->d_name);

		rc = process_run_sync(process);
		if (rc) {
			pb_log("boot hook exec failed!\n");

		} else if (WIFEXITED(process->exit_status) &&
			   WEXITSTATUS(process->exit_status)
				== BOOT_HOOK_EXIT_UPDATE) {
			/* if the hook returned with BOOT_HOOK_EXIT_UPDATE,
			 * then we process stdout to look for updated params
			 */
			if (rc == BOOT_HOOK_EXIT_UPDATE) {
				boot_hook_update(task, hooks[i]->d_name,
						process->stdout_buf);
				boot_hook_setenv(task);
			}
		}

		process_release(process);
		talloc_free(path);
	}

	free(hooks);
}

static bool load_pending(struct load_url_result *result)
{
	return result && result->status == LOAD_ASYNC;
}

static int check_load(struct boot_task *task, const char *name,
		struct load_url_result *result)
{
	if (!result)
		return 0;
	if (result->status != LOAD_ERROR)
		return 0;

	update_status(task->status_fn, task->status_arg,
			BOOT_STATUS_ERROR,
			"Couldn't load %s", name);
	return -1;
}

static void cleanup_load(struct load_url_result *result)
{
	if (!result)
		return;
	if (result->status != LOAD_OK)
		return;
	if (!result->cleanup_local)
		return;
	unlink(result->local);
}

static void boot_process(struct load_url_result *result __attribute__((unused)),
		void *data)
{
	struct boot_task *task = data;
	int rc = -1;

	if (load_pending(task->image) ||
			load_pending(task->initrd) ||
			load_pending(task->dtb))
		return;

	if (check_load(task, "kernel image", task->image) ||
			check_load(task, "initrd", task->initrd) ||
			check_load(task, "dtb", task->dtb))
		goto no_load;

	run_boot_hooks(task);

	update_status(task->status_fn, task->status_arg, BOOT_STATUS_INFO,
			"performing kexec_load");

	rc = kexec_load(task);
	if (rc) {
		update_status(task->status_fn, task->status_arg,
				BOOT_STATUS_ERROR, "kexec load failed");
	}

no_load:
	cleanup_load(task->image);
	cleanup_load(task->initrd);
	cleanup_load(task->dtb);

	if (!rc) {
		update_status(task->status_fn, task->status_arg,
				BOOT_STATUS_INFO,
				"performing kexec reboot");

		rc = kexec_reboot(task);
		if (rc) {
			update_status(task->status_fn, task->status_arg,
					BOOT_STATUS_ERROR,
					"kexec reboot failed");
		}
	}

	talloc_free(task);
}

static int start_url_load(struct boot_task *task, const char *name,
		struct pb_url *url, struct load_url_result **result)
{
	if (!url)
		return 0;

	*result = load_url_async(task, url, boot_process, task);
	if (!*result) {
		update_status(task->status_fn, task->status_arg,
				BOOT_STATUS_ERROR,
				"Error loading %s", name);
		return -1;
	}
	return 0;
}

int boot(void *ctx, struct discover_boot_option *opt, struct boot_command *cmd,
		int dry_run, boot_status_fn status_fn, void *status_arg)
{
	struct pb_url *image = NULL, *initrd = NULL, *dtb = NULL;
	struct boot_task *boot_task;
	const char *boot_desc;
	int rc;

	if (opt && opt->option->name)
		boot_desc = opt->option->name;
	else if (cmd && cmd->boot_image_file)
		boot_desc = cmd->boot_image_file;
	else
		boot_desc = "(unknown)";

	update_status(status_fn, status_arg, BOOT_STATUS_INFO,
			"Booting %s.", boot_desc);

	if (cmd && cmd->boot_image_file) {
		image = pb_url_parse(opt, cmd->boot_image_file);
	} else if (opt && opt->boot_image) {
		image = opt->boot_image->url;
	} else {
		pb_log("%s: no image specified\n", __func__);
		update_status(status_fn, status_arg, BOOT_STATUS_INFO,
				"Boot failed: no image specified");
		return -1;
	}

	if (cmd && cmd->initrd_file) {
		initrd = pb_url_parse(opt, cmd->initrd_file);
	} else if (opt && opt->initrd) {
		initrd = opt->initrd->url;
	}

	if (cmd && cmd->dtb_file) {
		dtb = pb_url_parse(opt, cmd->dtb_file);
	} else if (opt && opt->dtb) {
		dtb = opt->dtb->url;
	}

	boot_task = talloc_zero(ctx, struct boot_task);
	boot_task->dry_run = dry_run;
	boot_task->status_fn = status_fn;
	boot_task->status_arg = status_arg;

	if (cmd && cmd->boot_args) {
		boot_task->args = talloc_strdup(boot_task, cmd->boot_args);
	} else if (opt && opt->option->boot_args) {
		boot_task->args = talloc_strdup(boot_task,
						opt->option->boot_args);
	} else {
		boot_task->args = NULL;
	}

	/* start async loads for boot resources */
	rc = start_url_load(boot_task, "kernel image", image, &boot_task->image)
	  || start_url_load(boot_task, "initrd", initrd, &boot_task->initrd)
	  || start_url_load(boot_task, "dtb", dtb, &boot_task->dtb);

	/* If all URLs are local, we may be done. */
	if (!rc)
		boot_process(NULL, boot_task);

	return rc;
}
