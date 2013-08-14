
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <process/process.h>
#include <waiter/waiter.h>
#include <talloc/talloc.h>

int main(void)
{
	struct waitset *waitset;
	int result;
	void *ctx;

	ctx = talloc_new(NULL);

	waitset = waitset_create(ctx);

	process_init(ctx, waitset);

	result = process_run_simple(ctx, "true", NULL);

	assert(WIFEXITED(result));
	assert(WEXITSTATUS(result) == 0);

	talloc_free(ctx);

	return EXIT_SUCCESS;
}
