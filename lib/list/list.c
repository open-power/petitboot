
#include "list/list.h"

void list_init(struct list *list)
{
	list->head.next = &list->head;
	list->head.prev = &list->head;
}

void list_insert_before(struct list_item *next, struct list_item *item)
{
	item->next = next;
	item->prev = next->prev;
	next->prev->next = item;
	next->prev = item;
}

void list_insert_after(struct list_item *prev, struct list_item *item)
{
	item->next = prev->next;
	item->prev = prev;
	prev->next->prev = item;
	prev->next = item;
}

void list_remove(struct list_item *item)
{
	item->next->prev = item->prev;
	item->prev->next = item->next;
}
