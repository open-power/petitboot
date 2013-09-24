
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

enum boot_process_state {
	BOOT_STATE_INITIAL,
	BOOT_STATE_IMAGE_LOADING,
	BOOT_STATE_INITRD_LOADING,
	BOOT_STATE_DTB_LOADING,
	BOOT_STATE_FINISH,
	BOOT_STATE_UNKNOWN,
};


struct boot_task {
	char *local_image;
	char *local_initrd;
	char *local_dtb;
	char *args;
	struct pb_url *image, *initrd, *dtb;
	boot_status_fn status_fn;
	void *status_arg;
	enum boot_process_state state;
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

	if (boot_task->local_initrd) {
		s_initrd = talloc_asprintf(boot_task, "--initrd=%s",
				boot_task->local_initrd);
		assert(s_initrd);
		*p++ = s_initrd;	 /* 3 */
	}

	if (boot_task->local_dtb) {
		s_dtb = talloc_asprintf(boot_task, "--dtb=%s",
						boot_task->local_dtb);
		assert(s_dtb);
		*p++ = s_dtb;		 /* 4 */
	}

	if (boot_task->args) {
		s_args = talloc_asprintf(boot_task, "--append=%s",
						boot_task->args);
		assert(s_args);
		*p++ = s_args;		/* 5 */
	}

	*p++ = boot_task->local_image;	/* 6 */
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
		char **p;
	} *param, params[] = {
		{ "boot_image",		&task->local_image },
		{ "boot_initrd",	&task->local_initrd },
		{ "boot_dtb",		&task->local_dtb },
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

	setenv("boot_image", task->local_image, 1);
	if (task->local_initrd)
		setenv("boot_initrd", task->local_initrd, 1);
	if (task->local_dtb)
		setenv("boot_dtb", task->local_dtb, 1);
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

static void boot_process(void *ctx, int status)
{
	struct boot_task *task = ctx;
	unsigned int clean_image = 0;
	unsigned int clean_initrd = 0;
	unsigned int clean_dtb = 0;
	int result = -1;

	if (task->state == BOOT_STATE_INITIAL) {
		update_status(task->status_fn, task->status_arg,
				BOOT_STATUS_INFO, "loading kernel");
		task->local_image = load_url_async(task, task->image,
					&clean_image, boot_process);
		if (!task->local_image) {
			update_status(task->status_fn, task->status_arg,
					BOOT_STATUS_ERROR,
					"Couldn't load kernel image");
			goto no_load;
		}
		task->state = BOOT_STATE_IMAGE_LOADING;
		return;
	}

	if (task->state == BOOT_STATE_IMAGE_LOADING) {
		if (status) {
			update_status(task->status_fn, task->status_arg,
					BOOT_STATUS_ERROR,
					"Error loading kernel image");
			goto no_load;
		}
		task->state = BOOT_STATE_INITRD_LOADING;

		if (task->initrd) {
			update_status(task->status_fn, task->status_arg,
					BOOT_STATUS_INFO, "loading initrd");
			task->local_initrd = load_url_async(task, task->initrd,
					&clean_initrd, boot_process);
			if (!task->local_initrd) {
				update_status(task->status_fn, task->status_arg,
						BOOT_STATUS_ERROR,
						"Couldn't load initrd image");
				goto no_load;
			}
			return;
		}
	}

	if (task->state == BOOT_STATE_INITRD_LOADING) {
		if (status) {
			update_status(task->status_fn, task->status_arg,
					BOOT_STATUS_ERROR,
					"Error loading initrd");
			goto no_load;
		}
		task->state = BOOT_STATE_DTB_LOADING;

		if (task->dtb) {
			update_status(task->status_fn, task->status_arg,
					BOOT_STATUS_INFO,
					"loading device tree");
			task->local_dtb = load_url_async(task, task->dtb,
						&clean_dtb, boot_process);
			if (!task->local_dtb) {
				update_status(task->status_fn, task->status_arg,
						BOOT_STATUS_ERROR,
						"Couldn't load device tree");
				goto no_load;
			}
			return;
		}
	}

	if (task->state == BOOT_STATE_DTB_LOADING) {
		if (status) {
			update_status(task->status_fn, task->status_arg,
					BOOT_STATUS_ERROR,
					"Error loading dtb");
			goto no_load;
		}
		task->state = BOOT_STATE_FINISH;
	}


	if (task->state != BOOT_STATE_FINISH) {
		task->state = BOOT_STATE_UNKNOWN;
		return;
	}

	run_boot_hooks(task);

	update_status(task->status_fn, task->status_arg, BOOT_STATUS_INFO,
			"performing kexec_load");

	result = kexec_load(task);

	if (result) {
		update_status(task->status_fn, task->status_arg,
				BOOT_STATUS_ERROR, "kexec load failed");
	}

no_load:
	if (clean_image)
		unlink(task->local_image);
	if (clean_initrd)
		unlink(task->local_initrd);
	if (clean_dtb)
		unlink(task->local_dtb);

	if (!result) {
		update_status(task->status_fn, task->status_arg,
				BOOT_STATUS_INFO,
				"performing kexec reboot");

		result = kexec_reboot(task);

		if (result) {
			update_status(task->status_fn, task->status_arg,
					BOOT_STATUS_ERROR,
					"kexec reboot failed");
		}
	}

	talloc_free(task);
}

int boot(void *ctx, struct discover_boot_option *opt, struct boot_command *cmd,
		int dry_run, boot_status_fn status_fn, void *status_arg)
{
	struct boot_task *boot_task;
	struct pb_url *image = NULL;
	const char *boot_desc;

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

	boot_task = talloc_zero(ctx, struct boot_task);
	boot_task->image = image;
	boot_task->dry_run = dry_run;
	boot_task->status_fn = status_fn;
	boot_task->status_arg = status_arg;
	boot_task->state = BOOT_STATE_INITIAL;

	if (cmd && cmd->initrd_file) {
		boot_task->initrd = pb_url_parse(opt, cmd->initrd_file);
	} else if (opt && opt->initrd) {
		boot_task->initrd = opt->initrd->url;
	}

	if (cmd && cmd->dtb_file) {
		boot_task->dtb = pb_url_parse(opt, cmd->dtb_file);
	} else if (opt && opt->dtb) {
		boot_task->dtb = opt->dtb->url;
	}

	if (cmd && cmd->boot_args) {
		boot_task->args = talloc_strdup(boot_task, cmd->boot_args);
	} else if (opt && opt->option->boot_args) {
		boot_task->args = talloc_strdup(boot_task,
						opt->option->boot_args);
	} else {
		boot_task->args = NULL;
	}

	boot_process(boot_task, 0);

	return 0;
}
