
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

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
#include <util/util.h>
#include <i18n/i18n.h>

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
	const char *local_image;
	const char *local_initrd;
	const char *local_dtb;
	const char *args;
	boot_status_fn status_fn;
	void *status_arg;
	bool dry_run;
	bool cancelled;
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

	pb_debug("boot status: [%d] %s\n", type, status.message);

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
			_("running boot hooks"));

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
			boot_hook_update(task, hooks[i]->d_name,
					process->stdout_buf);
			boot_hook_setenv(task);
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
			_("Couldn't load %s"), name);
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

static void cleanup_cancellations(struct boot_task *task,
		struct load_url_result *cur_result)
{
	struct load_url_result *result, **results[] = {
		&task->image, &task->initrd, &task->dtb,
	};
	bool pending = false;
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(results); i++) {
		result = *results[i];

		if (!result)
			continue;

		/* We need to cleanup and free any completed loads */
		if (result == cur_result || result->status == LOAD_OK
				|| result->status == LOAD_ERROR) {
			cleanup_load(result);
			talloc_free(result);
			*results[i] = NULL;

		/* ... and cancel any pending loads, which we'll free in
		 * the completion callback */
		} else if (result->status == LOAD_ASYNC) {
			load_url_async_cancel(result);
			pending = true;

		/* if we're waiting for a cancellation, we still need to
		 * wait for the completion before freeing the boot task */
		} else if (result->status == LOAD_CANCELLED) {
			pending = true;
		}
	}

	if (!pending)
		talloc_free(task);
}

static void boot_process(struct load_url_result *result, void *data)
{
	struct boot_task *task = data;
	int rc = -1;

	if (task->cancelled) {
		cleanup_cancellations(task, result);
		return;
	}

	if (load_pending(task->image) ||
			load_pending(task->initrd) ||
			load_pending(task->dtb))
		return;

	if (check_load(task, "kernel image", task->image) ||
			check_load(task, "initrd", task->initrd) ||
			check_load(task, "dtb", task->dtb))
		goto no_load;

	/* we make a copy of the local paths, as the boot hooks might update
	 * and/or create these */
	task->local_image = task->image ? task->image->local : NULL;
	task->local_initrd = task->initrd ? task->initrd->local : NULL;
	task->local_dtb = task->dtb ? task->dtb->local : NULL;

	run_boot_hooks(task);

	update_status(task->status_fn, task->status_arg, BOOT_STATUS_INFO,
			_("performing kexec_load"));

	rc = kexec_load(task);
	if (rc) {
		update_status(task->status_fn, task->status_arg,
				BOOT_STATUS_ERROR, _("kexec load failed"));
	}

no_load:
	cleanup_load(task->image);
	cleanup_load(task->initrd);
	cleanup_load(task->dtb);

	if (!rc) {
		update_status(task->status_fn, task->status_arg,
				BOOT_STATUS_INFO,
				_("performing kexec reboot"));

		rc = kexec_reboot(task);
		if (rc) {
			update_status(task->status_fn, task->status_arg,
					BOOT_STATUS_ERROR,
					_("kexec reboot failed"));
		}
	}
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
				_("Error loading %s"), name);
		return -1;
	}
	return 0;
}

struct boot_task *boot(void *ctx, struct discover_boot_option *opt,
		struct boot_command *cmd, int dry_run,
		boot_status_fn status_fn, void *status_arg)
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
		boot_desc = _("(unknown)");

	update_status(status_fn, status_arg, BOOT_STATUS_INFO,
			_("Booting %s."), boot_desc);

	if (cmd && cmd->boot_image_file) {
		image = pb_url_parse(opt, cmd->boot_image_file);
	} else if (opt && opt->boot_image) {
		image = opt->boot_image->url;
	} else {
		pb_log("%s: no image specified\n", __func__);
		update_status(status_fn, status_arg, BOOT_STATUS_INFO,
				_("Boot failed: no image specified"));
		return NULL;
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
	if (rc) {
		talloc_free(boot_task);
		return NULL;
	}

	boot_process(NULL, boot_task);

	return boot_task;
}

void boot_cancel(struct boot_task *task)
{
	task->cancelled = true;

	update_status(task->status_fn, task->status_arg, BOOT_STATUS_INFO,
			_("Boot cancelled"));

	cleanup_cancellations(task, NULL);
}
