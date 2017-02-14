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
#ifndef PROCESS_H
#define PROCESS_H

#include <stdbool.h>
#include <sys/types.h>

#include <list/list.h>
#include <waiter/waiter.h>

struct process;
struct procset;
struct process_info;

typedef void	(*process_exit_cb)(struct process *);

struct process {
	/* caller-provided configuration */
	const char		*path;
	const char		**argv;
	bool			keep_stdout;
	bool			add_stderr;
	bool			raw_stdout;
	process_exit_cb		exit_cb;
	void			*data;
	waiter_cb		stdout_cb;
	void			*stdout_data;

	/* runtime data */
	pid_t			pid;
	int			stdout_len;
	char			*stdout_buf;

	/* post-execution information */
	int			exit_status;
	bool			cancelled;
};

/* Process management system init. process_init must be called before
 * process_create. The pointer returned can be talloc_free()-ed, or can be
 * automatically freed through destruction of the ctx talloc tree.
 */
struct procset *process_init(void *ctx, struct waitset *set, bool dry_run);

struct process *process_create(void *ctx);

/* process_release: release our reference to the process, but potentially
 * leave it running. When the process exits, associated resources will
 * be deallocated.
 */
void process_release(struct process *process);

/* Synchronous interface. These functions will all block while waiting for
 * the process to exit.
 */
int process_run_sync(struct process *process);
int process_run_simple_argv(void *ctx, const char *argv[]);
int process_run_simple(void *ctx, const char *name, ...)
	__attribute__((sentinel(0)));

/* Asynchronous interface. When a process is run with process_run_async, the
 * function returns without wait()ing for the child process to exit. If the
 * process' exit_cb member is set, that callback will be invoked when the
 * process exits.
 */
int process_run_async(struct process *process);

void process_stop_async(struct process *process);
void process_stop_async_all(void);

/* helper function to determine if a process exited cleanly, with a non-zero
 * exit status */
bool process_exit_ok(struct process *process);

/* Functions to assist callers using a custom stdout callback */
struct process *procinfo_get_process(struct process_info *procinfo);
int process_stdout_custom(struct process_info *procinfo, char **line);

#endif /* PROCESS_H */
