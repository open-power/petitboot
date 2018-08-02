
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
#include "platform.h"

#include <security/security.h>

static const char *boot_hook_dir = PKG_SYSCONF_DIR "/boot.d";
enum {
	BOOT_HOOK_EXIT_OK	= 0,
	BOOT_HOOK_EXIT_UPDATE	= 2,
};

static void __attribute__((format(__printf__, 4, 5))) update_status(
		boot_status_fn fn, void *arg, int type, char *fmt, ...)
{
	struct status status;
	va_list ap;

	va_start(ap, fmt);
	status.message = talloc_vasprintf(NULL, fmt, ap);
	va_end(ap);

	status.type = type;
	status.backlog = false;

	pb_debug("boot status: [%d] %s\n", type, status.message);

	fn(arg, &status);

	talloc_free(status.message);
}

/**
 * kexec_load - kexec load helper.
 */
static int kexec_load(struct boot_task *boot_task)
{
	struct process *process;
	char *s_initrd = NULL;
	char *s_args = NULL;
	const char *argv[8];
	char *s_dtb = NULL;
	const char **p;
	int result;


	boot_task->local_initrd_override = NULL;
	boot_task->local_dtb_override = NULL;
	boot_task->local_image_override = NULL;

	if ((result = validate_boot_files(boot_task))) {
		if (result == KEXEC_LOAD_DECRYPTION_FALURE) {
			pb_log("%s: Aborting kexec due to"
				" decryption failure\n", __func__);
		}
		if (result == KEXEC_LOAD_SIGNATURE_FAILURE) {
			pb_log("%s: Aborting kexec due to signature"
				" verification failure\n", __func__);
		}

		goto abort_kexec;
	}

	const char* local_initrd = (boot_task->local_initrd_override) ?
		boot_task->local_initrd_override : boot_task->local_initrd;
	const char* local_dtb = (boot_task->local_dtb_override) ?
		boot_task->local_dtb_override : boot_task->local_dtb;
	const char* local_image = (boot_task->local_image_override) ?
		boot_task->local_image_override : boot_task->local_image;

	process = process_create(boot_task);
	if (!process) {
		pb_log_fn("failed to create process\n");
		return -1;
	}

	process->path = pb_system_apps.kexec;
	process->argv = argv;
	process->keep_stdout = true;
	process->add_stderr = true;

	p = argv;
	*p++ = pb_system_apps.kexec;	/* 1 */
	*p++ = "-l";			/* 2 */

	if (pb_log_get_debug()) {
		*p++ = "--debug";	/* 3 */
	}

	if (local_initrd) {
		s_initrd = talloc_asprintf(boot_task, "--initrd=%s",
				local_initrd);
		assert(s_initrd);
		*p++ = s_initrd;	 /* 4 */
	}

	if (local_dtb) {
		s_dtb = talloc_asprintf(boot_task, "--dtb=%s",
						local_dtb);
		assert(s_dtb);
		*p++ = s_dtb;		 /* 5 */
	}

	s_args = talloc_asprintf(boot_task, "--append=%s",
				boot_task->args ?: "\"\"");
	assert(s_args);
	*p++ = s_args;			/* 6 */

	*p++ = local_image;		/* 7 */
	*p++ = NULL;			/* 8 */

	result = process_run_sync(process);
	if (result) {
		pb_log_fn("failed to run process\n");
		goto abort_kexec;
	}

	result = process->exit_status;

	if (result) {
		pb_log_fn("failed: (%d)\n", result);
		update_status(boot_task->status_fn, boot_task->status_arg,
				STATUS_ERROR, "%s", process->stdout_buf);
	}

abort_kexec:
	validate_boot_files_cleanup(boot_task);

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
		pb_log_fn("failed: (%d)\n", result);

	/* okay, kexec -e -f */
	if (result) {
		result = process_run_simple(task, pb_system_apps.kexec,
						"-e", "-f", NULL);
	}

	if (result)
		pb_log_fn("failed: (%d)\n", result);


	return result;
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
	char *saveptr = NULL;

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
	unsetenv("boot_console");

	setenv("boot_image", task->local_image, 1);
	if (task->local_initrd)
		setenv("boot_initrd", task->local_initrd, 1);
	if (task->local_dtb)
		setenv("boot_dtb", task->local_dtb, 1);
	if (task->args)
		setenv("boot_args", task->args, 1);
	if (task->boot_console)
		setenv("boot_console", task->boot_console, 1);
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

	update_status(task->status_fn, task->status_arg, STATUS_INFO,
			_("Running boot hooks"));

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
	return !result || result->status == LOAD_ASYNC;
}

static int check_load(struct boot_task *task, const char *name,
		struct load_url_result *result)
{
	if (!result)
		return 0;

	if (result->status != LOAD_ERROR) {
		update_status(task->status_fn, task->status_arg,
				STATUS_ERROR,
				_("Loaded %s from %s"), name,
				pb_url_to_string(result->url));
		return 0;
	}

	pb_log("Failed to load %s from %s\n", name,
			pb_url_to_string(result->url));
	update_status(task->status_fn, task->status_arg,
			STATUS_ERROR,
			_("Couldn't load %s from %s"), name,
			pb_url_to_string(result->url));
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
	struct boot_resource *resource;
	struct load_url_result *result;
	bool pending = false;

	list_for_each_entry(&task->resources, resource, list) {
		result = resource->result;

		/* Nothing to do if a load hasn't actually started yet */
		if (!result)
			continue;

		/* We need to cleanup and free any completed loads */
		if (result == cur_result || result->status == LOAD_OK
				|| result->status == LOAD_ERROR) {
			cleanup_load(result);
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
	struct boot_resource *resource;
	int rc = -1;

	if (task->cancelled) {
		cleanup_cancellations(task, result);
		return;
	}

	list_for_each_entry(&task->resources, resource, list)
		if (load_pending(resource->result))
			return;

	list_for_each_entry(&task->resources, resource, list) {
		if (check_load(task, resource->name, resource->result))
			goto no_load;
		*resource->local_path = resource->result->local;
	}

	run_boot_hooks(task);

	update_status(task->status_fn, task->status_arg, STATUS_INFO,
			_("Performing kexec load"));

	rc = kexec_load(task);
	pb_log_fn("kexec_load returned %d\n", rc);
	if (rc == KEXEC_LOAD_DECRYPTION_FALURE) {
		update_status(task->status_fn, task->status_arg,
				STATUS_ERROR, _("Decryption failed"));
	}
	else if (rc == KEXEC_LOAD_SIGNATURE_FAILURE) {
		update_status(task->status_fn, task->status_arg,
				STATUS_ERROR,
				_("Signature verification failed"));
	}
	else if (rc == KEXEC_LOAD_SIG_SETUP_INVALID) {
		update_status(task->status_fn, task->status_arg,
				STATUS_ERROR,
				_("Invalid signature configuration"));
	}

no_load:
	list_for_each_entry(&task->resources, resource, list)
		cleanup_load(resource->result);

	if (!rc) {
		update_status(task->status_fn, task->status_arg,
				STATUS_INFO, _("Performing kexec reboot"));

		rc = kexec_reboot(task);
		if (rc) {
			update_status(task->status_fn, task->status_arg,
					STATUS_ERROR,
					_("kexec reboot failed"));
		}
	} else {
		pb_log("Failed to load all boot resources\n");
	}
}

static int start_url_load(struct boot_task *task, struct boot_resource *res)
{
	if (!res)
		return 0;

	res->result = load_url_async(task, res->url, boot_process,
				 task, NULL, task->status_arg);
	if (!res->result) {
		pb_log("Error starting load for %s at %s\n",
				res->name, pb_url_to_string(res->url));
		update_status(task->status_fn, task->status_arg,
				STATUS_ERROR, _("Error loading %s"),
				res->name);
		return -1;
	}
	return 0;
}

static struct boot_resource *add_boot_resource(struct boot_task *task,
		const char *name, struct pb_url *url,
		const char **local_path)
{
	struct boot_resource *res;

	if (!url)
		return NULL;

	res = talloc_zero(task, struct boot_resource);
	if (!res)
		return NULL;

	res->name = talloc_strdup(res, name);
	res->url = pb_url_copy(res, url);
	res->local_path = local_path;

	list_add(&task->resources, &res->list);
	return res;
}

struct boot_task *boot(void *ctx, struct discover_boot_option *opt,
		struct boot_command *cmd, int dry_run,
		boot_status_fn status_fn, void *status_arg)
{
	struct pb_url *image = NULL, *initrd = NULL, *dtb = NULL;
	struct pb_url *image_sig = NULL, *initrd_sig = NULL, *dtb_sig = NULL,
		*cmdline_sig = NULL;
	struct boot_resource *image_res, *initrd_res, *dtb_res, *tmp;
	const struct config *config = config_get();
	struct boot_task *boot_task;
	const char *boot_desc;
	int rc;
	int lockdown_type;

	if (opt && opt->option->name)
		boot_desc = opt->option->name;
	else if (cmd && cmd->boot_image_file)
		boot_desc = cmd->boot_image_file;
	else
		boot_desc = _("(unknown)");

	update_status(status_fn, status_arg, STATUS_INFO,
			_("Booting %s"), boot_desc);

	if (cmd && cmd->boot_image_file) {
		image = pb_url_parse(opt, cmd->boot_image_file);
	} else if (opt && opt->boot_image) {
		image = opt->boot_image->url;
	} else {
		pb_log_fn("no image specified\n");
		update_status(status_fn, status_arg, STATUS_INFO,
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

	if (opt && opt->proxy) {
		setenv("http_proxy", opt->proxy, 1);
		setenv("https_proxy", opt->proxy, 1);
	}

	boot_task = talloc_zero(ctx, struct boot_task);
	boot_task->dry_run = dry_run;
	boot_task->status_fn = status_fn;
	boot_task->status_arg = status_arg;
	list_init(&boot_task->resources);

	lockdown_type = lockdown_status();
	boot_task->verify_signature = (lockdown_type == PB_LOCKDOWN_SIGN);
	boot_task->decrypt_files = (lockdown_type == PB_LOCKDOWN_DECRYPT);

	if (cmd && cmd->boot_args) {
		boot_task->args = talloc_strdup(boot_task, cmd->boot_args);
	} else if (opt && opt->option->boot_args) {
		boot_task->args = talloc_strdup(boot_task,
						opt->option->boot_args);
	} else {
		boot_task->args = NULL;
	}

	if (cmd && cmd->console && !config->manual_console)
		boot_task->boot_console = talloc_strdup(boot_task, cmd->console);
	else
		boot_task->boot_console = config ? config->boot_console : NULL;

	if (boot_task->verify_signature || boot_task->decrypt_files) {
		if (cmd && cmd->args_sig_file) {
			cmdline_sig = pb_url_parse(opt, cmd->args_sig_file);
		} else if (opt && opt->args_sig_file) {
			cmdline_sig = opt->args_sig_file->url;
		} else {
			pb_log("%s: no command line signature file"
				" specified\n", __func__);
			update_status(status_fn, status_arg, STATUS_INFO,
					_("Boot failed: no command line"
						" signature file specified"));
			talloc_free(boot_task);
			return NULL;
		}
	}

	image_res = add_boot_resource(boot_task, _("kernel image"), image,
			&boot_task->local_image);
	initrd_res = add_boot_resource(boot_task, _("initrd"), initrd,
			&boot_task->local_initrd);
	dtb_res = add_boot_resource(boot_task, _("dtb"), dtb,
			&boot_task->local_dtb);

	/* start async loads for boot resources */
	rc = start_url_load(boot_task, image_res)
	  || start_url_load(boot_task, initrd_res)
	  || start_url_load(boot_task, dtb_res);

	if (boot_task->verify_signature) {
		/* Generate names of associated signature files and load */
		if (image) {
			image_sig = get_signature_url(ctx, image);
			tmp = add_boot_resource(boot_task,
					_("kernel image signature"), image_sig,
					&boot_task->local_image_signature);
			rc |= start_url_load(boot_task, tmp);
		}
		if (initrd) {
			initrd_sig = get_signature_url(ctx, initrd);
			tmp = add_boot_resource(boot_task,
					_("initrd signature"), initrd_sig,
					&boot_task->local_initrd_signature);
			rc |= start_url_load(boot_task, tmp);
		}
		if (dtb) {
			dtb_sig = get_signature_url(ctx, dtb);
			tmp = add_boot_resource(boot_task,
					_("dtb signature"), dtb_sig,
					&boot_task->local_dtb_signature);
			rc |= start_url_load(boot_task, tmp);
		}
	}

	if (boot_task->verify_signature || boot_task->decrypt_files) {
		tmp = add_boot_resource(boot_task,
				_("kernel command line signature"), cmdline_sig,
				&boot_task->local_cmdline_signature);
		rc |= start_url_load(boot_task, tmp);
	}

	/* If all URLs are local, we may be done. */
	if (rc) {
		/* Don't call boot_cancel() to preserve the status update */
		boot_task->cancelled = true;
		cleanup_cancellations(boot_task, NULL);
		return NULL;
	}

	boot_process(NULL, boot_task);

	return boot_task;
}

void boot_cancel(struct boot_task *task)
{
	task->cancelled = true;

	update_status(task->status_fn, task->status_arg, STATUS_INFO,
			_("Boot cancelled"));

	cleanup_cancellations(task, NULL);
}
