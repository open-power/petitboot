#ifndef PARAM_LIST_H
#define PARAM_LIST_H

#include <stddef.h>

#include <list/list.h>

/* struct param - Name/value pairs of zero terminated strings. */
struct param {
	char *name;
	char *value;
	bool modified;
	struct list_item list;
};

struct param_list {
	struct list params;
	const char **known_params;
};

#define param_list_for_each(_pl_ptr, _pos) \
	list_for_each_entry(&(_pl_ptr)->params, _pos, list)

#define param_list_for_each_known_param(_pl_ptr, _pos) \
	for (_pos = (_pl_ptr)->known_params; *_pos; _pos++)

const char **common_known_params(void);

void param_list_init(struct param_list *pl, const char *known_params[]);
bool param_list_is_known(const struct param_list *pl, const char *name);
bool param_list_is_known_n(const struct param_list *pl, const char *name,
	unsigned int name_len);
struct param *param_list_get_param(struct param_list *pl, const char *name);
static inline const char *param_list_get_value(const struct param_list *pl,
	const char *name)
{
	const struct param *param =
		param_list_get_param((struct param_list *)pl, name);
	return param ? param->value : NULL;
}
void param_list_set(struct param_list *pl, const char *name, const char *value,
	bool modified_on_create);

/* param_list_set_non_empty - Won't create a new parameter that would be empty. */
void param_list_set_non_empty(struct param_list *pl, const char *name,
	const char *value, bool modified_on_create);

#endif /* PARAM_LIST_H */

