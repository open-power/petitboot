
#include "list/list.h"

void list_init(struct list *list)
{
	list->head.next = &list->head;
	list->head.prev = &list->head;
}

void list_insert_before(struct list_item *next, struct list_item *new)
{
	new->next = next;
	new->prev = next->prev;
	next->prev->next = new;
	next->prev = new;
}

void list_insert_after(struct list_item *prev, struct list_item *new)
{
	new->next = prev->next;
	new->prev = prev;
	prev->next->prev = new;
	prev->next = new;
}

void list_remove(struct list_item *item)
{
	item->next->prev = item->prev;
	item->prev->next = item->next;
}
