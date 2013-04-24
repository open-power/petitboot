/*
 *  Copyright Geoff Levand <geoff@infradead.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>

#include <list/list.h>


int main(void)
{
	struct item {
		struct list_item list;
		int value;
	};
	STATIC_LIST(tester);
	struct item *item;
	struct item *tmp;
	int i;

	for (i = 0; i < 5; i++) {
		struct item *item = malloc(sizeof(struct item));

		item->value = i;

		list_add_tail(&tester, &item->list);
	}
	
	i = 0;
	fprintf(stderr, "-- list_for_each_entry --\n");
	list_for_each_entry(&tester, item, list) {
		fprintf(stderr, "%d: %d: %p -> %p\n", i++, item->value, item, item->list.next);
	}
	
	i = 0;
	fprintf(stderr, "-- list_for_each_entry_safe --\n");
	list_for_each_entry_safe(&tester, item, tmp, list) {
		fprintf(stderr, "pos: %d: %d: %p -> %p\n", i++, item->value, item, item->list.next);
		fprintf(stderr, "tmp:       %p -> %p\n", tmp, (tmp ? tmp->list.next : NULL));
		list_remove(&item->list);
	}

	i = 0;
	fprintf(stderr, "-- list_for_each_entry --\n");
	list_for_each_entry(&tester, item, list) {
		fprintf(stderr, "%d: %d: %p -> %p\n", i++, item->value, item, item->list.next);
	}

	fprintf(stderr, "-- done --\n");
	return -1;
}
