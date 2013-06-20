
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "log/log.h"
#include <talloc/talloc.h>
#include "system.h"

const struct pb_system_apps pb_system_apps = {
	.prefix		= PREFIX,
	.cp		= HOST_PROG_CP,
	.kexec		= HOST_PROG_KEXEC,
	.mount		= HOST_PROG_MOUNT,
	.shutdown	= HOST_PROG_SHUTDOWN,
	.sftp		= HOST_PROG_SFTP,
	.tftp		= HOST_PROG_TFTP,
	.umount		= HOST_PROG_UMOUNT,
	.wget		= HOST_PROG_WGET,
	.ip		= HOST_PROG_IP,
	.udhcpc		= HOST_PROG_UDHCPC,
};

int pb_mkdir_recursive(const char *dir)
{
	struct stat statbuf;
	char *str, *sep;
	int mode = 0755;

	if (!*dir)
		return 0;

	if (!stat(dir, &statbuf)) {
		if (!S_ISDIR(statbuf.st_mode)) {
			pb_log("%s: %s exists, but isn't a directory\n",
					__func__, dir);
			return -1;
		}
		return 0;
	}

	str = talloc_strdup(NULL, dir);
	sep = strchr(*str == '/' ? str + 1 : str, '/');

	while (1) {

		/* terminate the path at sep */
		if (sep)
			*sep = '\0';

		if (mkdir(str, mode) && errno != EEXIST) {
			pb_log("mkdir(%s): %s\n", str, strerror(errno));
			return -1;
		}

		if (!sep)
			break;

		/* reset dir to the full path */
		strcpy(str, dir);
		sep = strchr(sep + 1, '/');
	}

	talloc_free(str);

	return 0;
}

int pb_rmdir_recursive(const char *base, const char *dir)
{
	char *cur, *pos;

	/* sanity check: make sure that dir is within base */
	if (strncmp(base, dir, strlen(base)))
		return -1;

	cur = talloc_strdup(NULL, dir);

	while (strcmp(base, dir)) {

		rmdir(dir);

		/* null-terminate at the last slash */
		pos = strrchr(dir, '/');
		if (!pos)
			break;

		*pos = '\0';
	}

	talloc_free(cur);

	return 0;
}

static int read_pipe(void *ctx, int fd, char **bufp, int *lenp)
{
	int rc, len, alloc_len;
	char *buf;

	alloc_len = 4096;
	len = 0;

	buf = talloc_array(ctx, char, alloc_len);

	for (;;) {
		rc = read(fd, buf, alloc_len - len - 1);
		if (rc <= 0)
			break;

		len += rc;
		if (len == alloc_len - 1) {
			alloc_len *= 2;
			buf = talloc_realloc(ctx, buf, char, alloc_len);
		}
	}

	if (rc < 0) {
		talloc_free(buf);
		return rc;
	}

	buf[len] = '\0';
	*bufp = buf;
	*lenp = len;

	return 0;
}

/**
 * pb_run_cmd - Run the supplied command.
 * @cmd_argv: An argument list array for execv.
 * @wait: Wait for the child process to complete before returning.
 * @dry_run: Don't actually fork and exec.
 */

int pb_run_cmd(const char *const *cmd_argv, int wait, int dry_run)
{
	return pb_run_cmd_pipe(cmd_argv, wait, dry_run, NULL, NULL, NULL);
}

int pb_run_cmd_pipe(const char *const *cmd_argv, int wait, int dry_run,
		void *ctx, char **stdout_buf, int *stdout_buf_len)
{
#if defined(DEBUG)
	enum {do_debug = 1};
#else
	enum {do_debug = 0};
#endif
	int status, pipefd[2];
	pid_t pid;

	assert(!wait && stdout_buf);
	assert(!!ctx != !!stdout_buf);
	assert(!!stdout_buf != !!stdout_buf_len);

	if (do_debug) {
		const char *const *p = cmd_argv;

		pb_log("%s: %s", __func__, (dry_run ? "(dry-run) " : ""));

		while (*p) {
			pb_log("%s ", *p);
			p++;
		}
		pb_log("\n");
	} else
		pb_log("%s: %s%s\n", __func__, (dry_run ? "(dry-run) " : ""),
			cmd_argv[0]);

	if (stdout_buf) {
		*stdout_buf = NULL;
		*stdout_buf_len = 0;
	}

	if (dry_run)
		return 0;

	if (stdout_buf) {
		status = pipe(pipefd);
		if (status) {
			pb_log("pipe failed");
			return -1;
		}
	}

	pid = fork();

	if (pid == -1) {
		pb_log("%s: fork failed: %s\n", __func__, strerror(errno));
		return -1;
	}


	if (pid == 0) {
		int log = fileno(pb_log_get_stream());

		/* Redirect child output to log. */

		if (stdout_buf) {
			status = dup2(pipefd[1], STDOUT_FILENO);
		} else {
			status = dup2(log, STDOUT_FILENO);
		}
		assert(status != -1);

		status = dup2(log, STDERR_FILENO);
		assert(status != -1);

		execvp(cmd_argv[0], (char *const *)cmd_argv);
		pb_log("%s: exec failed: %s\n", __func__, strerror(errno));
		exit(EXIT_FAILURE);
	}

	if (!wait && !waitpid(pid, &status, WNOHANG))
		return 0;

	if (stdout_buf) {
		close(pipefd[1]);
		status = read_pipe(ctx, pipefd[0], stdout_buf, stdout_buf_len);
		if (status)
			return -1;
	}

	if (waitpid(pid, &status, 0) == -1) {
		pb_log("%s: waitpid failed: %s\n", __func__,
				strerror(errno));
		return -1;
	}

	if (do_debug && WIFSIGNALED(status) && WTERMSIG(status) == SIGINT)
		pb_log("%s: signaled\n", __func__);

	if (!WIFEXITED(status)) {
		pb_log("%s: %s failed\n", __func__, cmd_argv[0]);
		return -1;
	}

	if (WEXITSTATUS(status))
		pb_log("%s: WEXITSTATUS %d\n", __func__, WEXITSTATUS(status));

	return WEXITSTATUS(status);
}
