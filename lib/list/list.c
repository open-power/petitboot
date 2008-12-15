
#include "list/list.h"

void list_init(struct list *list)
{
	list->head.next = &list->head;
	list->head.prev = &list->head;
}

void list_add(struct list *list, struct list_item *new)
{
	new->next = list->head.next;
	new->prev = &list->head;

	list->head.next->prev = new;
	list->head.next = new;
}

void list_remove(struct list_item *item)
{
	item->next->prev = item->prev;
	item->prev->next = item->next;
}

