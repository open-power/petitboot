#include "platform.h"

static bool probe(struct platform *p, void *ctx)
{
	(void)p;
	(void)ctx;

	return false;
}

static struct platform dummy = {
	.name			= "dummy",
	.probe			= probe,
};

register_platform(dummy);
