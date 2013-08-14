
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>
#include <sys/stat.h>

#include <process/process.h>
#include <waiter/waiter.h>
#include <talloc/talloc.h>

static const char *async_fifo = "/tmp/test-process-both.fifo";

static int do_sync_child(void)
{
	return 42;
}

static int do_async_child(void)
{
	char c;
	int fd;

	fd = open(async_fifo, O_RDONLY);
	assert(fd >= 0);

	read(fd, &c, 1);

	assert(c == 1);

	return 43;
}

struct test {
	int		async_fd;
	struct process	*sync_process;
	struct process	*async_process;

	bool		async_exited;
	int		async_exit_status;

};

static void async_exit_cb(struct process *process)
{
	struct test *test = process->data;
	test->async_exited = true;
	test->async_exit_status = process->exit_status;
}

int main(int argc, char **argv)
{
	const char *sync_child_argv[3], *async_child_argv[3];
	struct waitset *waitset;
	struct test *test;
	char c;
	int rc;

	if (argc == 2 && !strcmp(argv[1], "sync-child"))
		return do_sync_child();

	if (argc == 2 && !strcmp(argv[1], "async-child"))
		return do_async_child();

	test = talloc_zero(NULL, struct test);

	unlink(async_fifo);
	rc = mkfifo(async_fifo, 0600);
	assert(rc == 0);

	waitset = waitset_create(test);

	process_init(test, waitset);

	sync_child_argv[0] = argv[0];
	sync_child_argv[1] = "sync-child";
	sync_child_argv[2] = NULL;

	test->sync_process = process_create(test);
	test->sync_process->path = sync_child_argv[0];
	test->sync_process->argv = sync_child_argv;

	async_child_argv[0] = argv[0];
	async_child_argv[1] = "async-child";
	async_child_argv[2] = NULL;

	test->async_process = process_create(test);
	test->async_process->path = async_child_argv[0];
	test->async_process->argv = async_child_argv;
	test->async_process->exit_cb = async_exit_cb;
	test->async_process->data = test;

	process_run_async(test->async_process);

	process_run_sync(test->sync_process);

	assert(WIFEXITED(test->sync_process->exit_status));
	assert(WEXITSTATUS(test->sync_process->exit_status) == 42);

	/* now that the sync process has completed, let the async process
	 * exit */
	test->async_fd = open(async_fifo, O_WRONLY);
	assert(test->async_fd >= 0);
	c = 1;
	write(test->async_fd, &c, 1);

	for (;;) {
		waiter_poll(waitset);

		if (test->async_exited)
			break;
	}

	assert(WIFEXITED(test->async_exit_status));
	assert(WEXITSTATUS(test->async_exit_status) == 43);

	talloc_free(test);

	unlink(async_fifo);

	return EXIT_SUCCESS;
}
