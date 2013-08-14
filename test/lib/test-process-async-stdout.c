
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <process/process.h>
#include <waiter/waiter.h>
#include <talloc/talloc.h>

static int do_child(void)
{
	printf("forty two\n");
	return 42;
}

struct test {
	bool	exited;
	int	exit_status;
	char	*stdout_buf;
	int	stdout_len;
};

static void exit_cb(struct process *process)
{
	struct test *test = process->data;

	test->exited = true;
	test->exit_status = process->exit_status;

	test->stdout_len = process->stdout_len;
	test->stdout_buf = talloc_steal(test, process->stdout_buf);
}

int main(int argc, char **argv)
{
	struct waitset *waitset;
	struct process *process;
	const char *child_argv[3];
	struct test *test;

	if (argc == 2 && !strcmp(argv[1], "child"))
		return do_child();

	test = talloc_zero(NULL, struct test);

	waitset = waitset_create(test);

	process_init(test, waitset);

	child_argv[0] = argv[0];
	child_argv[1] = "child";
	child_argv[2] = NULL;

	process = process_create(test);
	process->path = child_argv[0];
	process->argv = child_argv;
	process->keep_stdout = true;
	process->exit_cb = exit_cb;
	process->data = test;

	process_run_async(process);

	for (;;) {
		waiter_poll(waitset);

		if (test->exited)
			break;
	}

	assert(WIFEXITED(test->exit_status));
	assert(WEXITSTATUS(test->exit_status) == 42);
	assert(test->stdout_len == strlen("forty two\n"));
	assert(test->stdout_buf);
	assert(!memcmp(test->stdout_buf, "forty two\n", strlen("forty two\n")));

	talloc_free(test);

	return EXIT_SUCCESS;
}
