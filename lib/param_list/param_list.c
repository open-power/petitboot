
#define _GNU_SOURCE

#include <assert.h>
#include <string.h>

#include <log/log.h>
#include <talloc/talloc.h>
#include <param_list/param_list.h>

const char **common_known_params(void)
{
	static const char *common[] = {
		"auto-boot?",
		"petitboot,network",
		"petitboot,timeout",
		"petitboot,bootdevs",
		"petitboot,language",
		"petitboot,debug?",
		"petitboot,write?",
		"petitboot,snapshots?",
		"petitboot,console",
		"petitboot,http_proxy",
		"petitboot,https_proxy",
		NULL,
	};

	return common;
}

void param_list_init(struct param_list *pl, const char *known_params[])
{
	assert(known_params);
	list_init(&pl->params);
	pl->known_params = known_params;
}

bool param_list_is_known_n(const struct param_list *pl, const char *name,
	unsigned int name_len)
{
	const char **known;

	assert(pl->known_params);

	param_list_for_each_known_param(pl, known) {
		if (name_len == strlen(*known) && !strncmp(name, *known, name_len)) {
			return true;
		}
	}

	return false;
}

bool param_list_is_known(const struct param_list *pl, const char *name)
{
	const char **known;

	assert(pl->known_params);

	param_list_for_each_known_param(pl, known) {
		if (!strcmp(name, *known)) {
			return true;
		}
	}

	return false;
}

struct param *param_list_get_param(struct param_list *pl, const char *name)
{
	struct param *param;

	param_list_for_each(pl, param) {
		if (!strcmp(param->name, name))
			return param;
	}
	return NULL;
}

void param_list_set(struct param_list *pl, const char *name, const char *value,
	bool modified_on_create)
{
	struct param *param;

	param_list_for_each(pl, param) {
		if (strcmp(param->name, name))
			continue;

		if (!strcmp(param->value, value))
			return;

		/* Update existing list entry. */
		talloc_free(param->value);
		param->value = talloc_strdup(param, value);
		param->modified = true;
		pb_debug_fn("Updated: %s:%s\n", name, value);
		return;
	}

	/* Add new entry to list. */
	param = talloc(pl, struct param);
	param->modified = modified_on_create;
	param->name = talloc_strdup(pl, name);
	param->value = talloc_strdup(pl, value);
	list_add(&pl->params, &param->list);
	pb_debug_fn("Created: %s:%s\n", name, value);
}

void param_list_set_non_empty(struct param_list *pl, const char *name, const char *value,
	bool modified_on_create)
{
	if (!param_list_get_value(pl, name) && !strlen(value)) {
		return;
	}

	param_list_set(pl, name, value, modified_on_create);
}

