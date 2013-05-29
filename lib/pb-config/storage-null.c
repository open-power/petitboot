
#include <stdlib.h>

#include "storage.h"

static int load(struct config_storage *st __attribute__((unused)),
		struct config *config __attribute__((unused)))
{
	return 0;
}

static struct config_storage st = {
	.load  = load,
};

struct config_storage *create_null_storage(void *ctx __attribute__((unused)))
{
	return &st;
}
