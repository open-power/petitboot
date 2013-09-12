
#include <talloc/talloc.h>

#include "grub2.h"

struct grub2_script *create_script(void *ctx)
{
	struct grub2_script *script;
	script = talloc(ctx, struct grub2_script);
	return script;
}

