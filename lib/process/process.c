/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

#include <process/process.h>
#include <talloc/talloc.h>
#include <waiter/waiter.h>
#include <log/log.h>

struct procset {
	struct waitset		*waitset;
	struct list		async_list;
	int			sigchld_pipe[2];
	struct waiter		*sigchld_waiter;
};

/* Internal data type for process handling
 *
 * Allocation: these structures may have multiple references:
 *  - from the original ctx pointer
 *  - due to inclusion in async_list
 *  - due to a currently-registered waiter
 *
 */
struct process_info {
#ifdef DEBUG
	/* prevent talloc_free(process) from working */
	int			__pad;
#endif
	struct process		process;
	struct list_item	async_list;
	int			stdout_buf_len;
	struct waiter		*stdout_waiter;
	int			stdout_pipe[2];
	void			*orig_ctx;
};

static struct procset *procset;

static struct process_info *get_info(struct process *process)
{
	return container_of(process, struct process_info, process);
}

/* Read as much as possible into the currently-allocated stdout buffer, and
 * possibly realloc it for the next read */
static int process_read_stdout_once(struct process_info *procinfo)
{
	struct process *process = &procinfo->process;
	int rc, fd, max_len;

	assert(process->keep_stdout);

	fd = procinfo->stdout_pipe[0];

	max_len =  procinfo->stdout_buf_len - process->stdout_len - 1;

	rc = read(fd, process->stdout_buf + process->stdout_len, max_len);
	if (rc <= 0)
		return rc;

	process->stdout_len += rc;
	if (process->stdout_len == procinfo->stdout_buf_len - 1) {
		procinfo->stdout_buf_len *= 2;
		process->stdout_buf = talloc_realloc(procinfo,
				process->stdout_buf, char,
				procinfo->stdout_buf_len);
	}

	return rc;
}

static int process_setup_stdout_pipe(struct process_info *procinfo)
{
	int rc;

	if (!procinfo->process.keep_stdout)
		return 0;

	procinfo->stdout_buf_len = 4096;
	procinfo->process.stdout_len = 0;
	procinfo->process.stdout_buf = talloc_array(procinfo, char,
			procinfo->stdout_buf_len);

	rc = pipe(procinfo->stdout_pipe);
	if (rc) {
		pb_log("pipe failed");
		return rc;
	}
	return 0;
}

static void process_setup_stdout_parent(struct process_info *procinfo)
{
	if (!procinfo->process.keep_stdout)
		return;

	close(procinfo->stdout_pipe[1]);
}

static void process_setup_stdout_child(struct process_info *procinfo)
{
	int log = fileno(pb_log_get_stream());

	if (procinfo->process.keep_stdout)
		dup2(procinfo->stdout_pipe[1], STDOUT_FILENO);
	else
		dup2(log, STDOUT_FILENO);

	dup2(log, STDERR_FILENO);
}

static void process_finish_stdout(struct process_info *procinfo)
{
	close(procinfo->stdout_pipe[0]);
	procinfo->process.stdout_buf[procinfo->process.stdout_len] = '\0';
}

static int process_read_stdout(struct process_info *procinfo)
{
	int rc;

	if (!procinfo->process.keep_stdout)
		return 0;

	do {
		rc = process_read_stdout_once(procinfo);
	} while (rc > 0);

	process_finish_stdout(procinfo);

	return rc < 0 ? rc : 0;
}

static int process_stdout_cb(void *arg)
{
	struct process_info *procinfo = arg;
	int rc;

	rc = process_read_stdout_once(procinfo);

	/* if we're going to signal to the waitset that we're done (ie, non-zero
	 * return value), then the waiters will remove us, so we drop the
	 * reference */
	if (rc < 0) {
		talloc_unlink(procset, procinfo);
		procinfo->stdout_waiter = NULL;
		rc = -1;
	} else {
		rc = 0;
	}

	return rc;
}

static void sigchld_sigaction(int signo, siginfo_t *info,
		void *arg __attribute__((unused)))
{
	pid_t pid;
	int rc;

	if (signo != SIGCHLD)
		return;

	pid = info->si_pid;

	rc = write(procset->sigchld_pipe[1], &pid, sizeof(pid));
	if (rc != sizeof(pid))
		pb_log("%s: write failed: %s\n", __func__, strerror(errno));
}

static int sigchld_pipe_event(void *arg)
{
	struct process_info *procinfo;
	struct procset *procset = arg;
	struct process *process;
	int pid, rc;

	rc = read(procset->sigchld_pipe[0], &pid, sizeof(pid));
	if (rc != sizeof(pid))
		return 0;

	process = NULL;
	list_for_each_entry(&procset->async_list, procinfo, async_list) {
		if (procinfo->process.pid == pid) {
			process = &procinfo->process;
			break;
		}
	}

	/* We'll receive SIGCHLD for synchronous processes too; just ignore */
	if (!process)
		return 0;

	rc = waitpid(process->pid, &process->exit_status, WNOHANG);

	/* if the process is still running, ignore the event. We leave
	 * the process in async_list so we can manage the final signal */
	if (rc == 0)
		return 0;

	/* ensure we have all of the child's stdout */
	process_read_stdout(procinfo);

	if (process->exit_cb)
		process->exit_cb(process);

	list_remove(&procinfo->async_list);
	talloc_unlink(procset, procinfo);

	return 0;
}

static int process_fini(void *p)
{
	struct procset *procset = p;
	struct sigaction sa;

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = SIG_DFL;

	sigaction(SIGCHLD, &sa, NULL);

	waiter_remove(procset->sigchld_waiter);

	close(procset->sigchld_pipe[0]);
	close(procset->sigchld_pipe[1]);
	return 0;
}

struct procset *process_init(void *ctx, struct waitset *set)
{
	struct sigaction sa;
	int rc;

	procset = talloc(ctx, struct procset);
	procset->waitset = set;
	list_init(&procset->async_list);

	rc = pipe(procset->sigchld_pipe);
	if (rc) {
		pb_log("%s: pipe() failed: %s\n", __func__, strerror(errno));
		goto err_free;
	}

	procset->sigchld_waiter = waiter_register_io(set,
					procset->sigchld_pipe[0], WAIT_IN,
					sigchld_pipe_event, procset);
	if (!procset->sigchld_waiter)
		goto err_close;

	memset(&sa, 0, sizeof(sa));
	sa.sa_sigaction = sigchld_sigaction;
	sa.sa_flags = SA_SIGINFO | SA_NOCLDSTOP;

	rc = sigaction(SIGCHLD, &sa, NULL);
	if (rc) {
		pb_log("%s: sigaction() failed: %s\n", __func__,
				strerror(errno));
		goto err_remove;
	}

	talloc_set_destructor(procset, process_fini);

	return procset;

err_remove:
	waiter_remove(procset->sigchld_waiter);
err_close:
	close(procset->sigchld_pipe[0]);
	close(procset->sigchld_pipe[1]);
err_free:
	talloc_free(procset);
	return NULL;
}

struct process *process_create(void *ctx)
{
	struct process_info *info = talloc_zero(ctx, struct process_info);
	info->orig_ctx = ctx;
	return &info->process;
}

void process_release(struct process *process)
{
	struct process_info *info = get_info(process);
	talloc_unlink(info->orig_ctx, info);
}

static int process_run_common(struct process_info *procinfo)
{
	struct process *process = &procinfo->process;
	const char *arg;
	char *logmsg;
	pid_t pid;
	int rc, i;

	logmsg = talloc_asprintf(procinfo, " exe:  %s\n argv:", process->path);
	for (i = 0, arg = process->argv[i]; arg; i++, arg = process->argv[i])
		logmsg = talloc_asprintf_append(logmsg, " '%s'", arg);

	pb_log("Running command:\n%s\n", logmsg);

	rc = process_setup_stdout_pipe(procinfo);
	if (rc)
		return rc;

	pid = fork();
	if (pid < 0) {
		pb_log("%s: fork failed: %s\n", __func__, strerror(errno));
		return pid;
	}

	if (pid == 0) {
		process_setup_stdout_child(procinfo);
		execvp(process->path, (char * const *)process->argv);
		exit(EXIT_FAILURE);
	}

	process_setup_stdout_parent(procinfo);
	process->pid = pid;

	return 0;
}

int process_run_sync(struct process *process)
{
	struct process_info *procinfo = get_info(process);
	int rc;

	rc = process_run_common(procinfo);
	if (rc)
		return rc;

	process_read_stdout(procinfo);

	rc = waitpid(process->pid, &process->exit_status, 0);
	if (rc == -1) {
		pb_log("%s: waitpid failed: %s\n", __func__, strerror(errno));
		return rc;
	}

	return 0;
}

int process_run_async(struct process *process)
{
	struct process_info *procinfo = get_info(process);
	int rc;

	rc = process_run_common(procinfo);
	if (rc)
		return rc;

	if (process->keep_stdout) {
		procinfo->stdout_waiter = waiter_register_io(procset->waitset,
						procinfo->stdout_pipe[0],
						WAIT_IN, process_stdout_cb,
						procinfo);
		talloc_reference(procset, procinfo);
	}

	list_add(&procset->async_list, &procinfo->async_list);
	talloc_reference(procset, procinfo);

	return 0;
}

void process_stop_async(struct process *process)
{
	kill(process->pid, SIGTERM);
}

int process_run_simple_argv(void *ctx, const char *argv[])
{
	struct process *process;
	int rc;

	process = process_create(ctx);

	process->path = argv[0];
	process->argv = argv;

	rc = process_run_sync(process);

	if (!rc)
		rc = process->exit_status;

	process_release(process);

	return rc;
}

int process_run_simple(void *ctx, const char *name, ...)
{
	int rc, i, n_argv = 1;
	const char **argv;
	va_list ap;

	va_start(ap, name);
	while (va_arg(ap, char *))
		n_argv++;
	va_end(ap);

	argv = talloc_array(ctx, const char *, n_argv + 1);
	argv[0] = name;

	va_start(ap, name);
	for (i = 1; i < n_argv; i++)
		argv[i] = va_arg(ap, const char *);
	va_end(ap);

	argv[i] = NULL;

	rc = process_run_simple_argv(ctx, argv);

	talloc_free(argv);

	return rc;
}
