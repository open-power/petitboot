
#include <stdio.h>
#include <string.h>

#include <talloc/talloc.h>
#include <array-size/array-size.h>

#include "grub2.h"


static int builtin_set(struct grub2_script *script, int argc, char *argv[])
{
	char *name, *value, *p;
	int i;

	if (argc < 2)
		return -1;

	p = strchr(argv[1], '=');
	if (!p)
		return -1;

	name = talloc_strndup(script, argv[1], p - argv[1]);
	value = talloc_strdup(script, p+1);

	for (i = 2; i < argc; i++)
		value = talloc_asprintf_append(value, " %s", argv[i]);

	script_env_set(script, name, value);

	return 0;
}

static struct grub2_command commands[] = {
	{
		.name = "set",
		.exec = builtin_set
	},
};

void register_builtins(struct grub2_script *script)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(commands); i++)
		script_register_command(script, &commands[i]);
}
