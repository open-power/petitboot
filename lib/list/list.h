#ifndef _LIST_H
#define _LIST_H

struct list_item {
	struct list_item *prev, *next;
};

struct list {
	struct list_item head;
};

#ifndef container_of
#define container_of(_ptr, _type, _member) ({ \
	const typeof( ((_type *)0)->_member ) *__mptr = (_ptr); \
	(_type *)( (char *)__mptr - offsetof(_type,_member) );})
#endif

#ifndef offsetof
#define offsetof(_type, _member) ((size_t) &((_type *)0)->_member)
#endif

#define list_for_each(_list, _pos) \
	for (_pos = (_list)->head.next; _pos != ((_list)->head); _pos = _pos->next)

#define list_entry(_ptr, _type, _member, _list) \
	(&container_of(_ptr, _type, _member)->_member == &((_list)->head) \
	? NULL \
	: container_of(_ptr, _type, _member))

#define list_prev_entry(_list, _pos, _member) \
	list_entry(_pos->_member.prev, typeof(*_pos), _member, _list)

#define list_next_entry(_list, _pos, _member) \
	list_entry(_pos->_member.next, typeof(*_pos), _member, _list)

#define list_for_each_entry(_list, _pos, _member) \
	for (_pos = list_entry((_list)->head.next, typeof(*_pos), _member, _list); \
		_pos; _pos = list_next_entry(_list, _pos, _member))

#define list_for_each_entry_continue(_list, _pos, _member) \
	for (; _pos; _pos = list_next_entry(_list, _pos, _member))

#define list_for_each_entry_safe(_list, _pos, _tmp, _member) \
	for (_pos = list_entry((_list)->head.next, typeof(*_pos), _member, _list), \
		_tmp = list_entry(_pos->_member.next, typeof(*_pos), _member, _list); \
	_pos; \
	_pos = _tmp, \
	_tmp = _tmp ? list_entry(_tmp->_member.next, typeof(*_pos), _member, _list) : NULL)

#define DEFINE_LIST(_list) struct list _list = { \
	.head = { \
		.next = &_list.head, \
		.prev = &_list.head \
	} \
}

#define STATIC_LIST(_list) static DEFINE_LIST(_list)

void list_init(struct list *list);
void list_insert_before(struct list_item *next, struct list_item *item);
void list_insert_after(struct list_item *prev, struct list_item *item);
void list_remove(struct list_item *item);

static inline void list_add(struct list *list, struct list_item *item)
{
	list_insert_after(&list->head, item);
}
static inline void list_add_tail(struct list *list, struct list_item *item)
{
	list_insert_before(&list->head, item);
}

#endif /* _LIST_H */
