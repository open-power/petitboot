
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
	.cp = "/bin/cp",
	.kexec = "/sbin/kexec",
	.mount = "/bin/mount",
	.shutdown = "/sbin/shutdown",
	.sftp = "/usr/bin/sftp",
	.tftp = "/usr/bin/tftp",
	.umount = "/bin/umount",
	.wget = "/usr/bin/wget",
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

/**
 * pb_run_cmd - Run the supplied command.
 * @cmd_argv: An argument list array for execv.
 */

int pb_run_cmd(const char *const *cmd_argv)
{
#if defined(DEBUG)
	enum {do_debug = 1};
#else
	enum {do_debug = 0};
#endif
	int status;
	pid_t pid;

	if (do_debug) {
		const char *const *p = cmd_argv;

		pb_log("%s: ", __func__);
		while (*p) {
			pb_log("%s ", *p);
			p++;
		}
		pb_log("\n");
	} else
		pb_log("%s: %s\n", __func__, cmd_argv[0]);

	pid = fork();

	if (pid == -1) {
		pb_log("%s: fork failed: %s\n", __func__, strerror(errno));
		return -1;
	}

	if (pid == 0) {
		int log = fileno(pb_log_get_stream());

		/* Redirect child output to log. */

		status = dup2(log, STDOUT_FILENO);
		assert(status != -1);

		status = dup2(log, STDERR_FILENO);
		assert(status != -1);

		execvp(cmd_argv[0], (char *const *)cmd_argv);
		pb_log("%s: exec failed: %s\n", __func__, strerror(errno));
		exit(EXIT_FAILURE);
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
