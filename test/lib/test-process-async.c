
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <process/process.h>
#include <waiter/waiter.h>
#include <talloc/talloc.h>

static int do_child(void)
{
	return 42;
}

static bool exited;
static int exit_status;

static void exit_cb(struct process *process)
{
	exited = true;
	exit_status = process->exit_status;
}

int main(int argc, char **argv)
{
	struct waitset *waitset;
	struct process *process;
	const char *child_argv[3];
	void *ctx;

	if (argc == 2 && !strcmp(argv[1], "child"))
		return do_child();

	ctx = talloc_new(NULL);

	waitset = waitset_create(ctx);

	process_init(ctx, waitset);

	child_argv[0] = argv[0];
	child_argv[1] = "child";
	child_argv[2] = NULL;

	process = process_create(ctx);
	process->path = child_argv[0];
	process->argv = child_argv;
	process->exit_cb = exit_cb;

	exited = false;

	process_run_async(process);

	for (;;) {
		waiter_poll(waitset);

		if (exited)
			break;
	}

	assert(WIFEXITED(exit_status));
	assert(WEXITSTATUS(exit_status) == 42);

	talloc_free(ctx);

	return EXIT_SUCCESS;
}
