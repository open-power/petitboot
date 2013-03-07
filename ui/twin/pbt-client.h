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

#if !defined(_PBT_CLIENT_H)
#define _PBT_CLIENT_H

#include "ui/common/ui-system.h"
#include "pbt-menu.h"
#include "pbt-scr.h"

/**
 * struct pbt_decor - Provides title, help and status bars.
 */

struct pbt_decor {
	twin_label_t *title;
	twin_label_t *help;
	twin_label_t *status;
};

struct pbt_frame {
	struct pbt_scr *scr;
	struct pbt_menu *top_menu;
	struct pbt_decor decor;
};

void pbt_frame_status_printf(struct pbt_frame *frame, const char *format, ...);

struct pbt_client {
	const char *sig;
	struct pb_signal_data signal_data;
	void *client_data;
	struct pbt_frame frame;
	struct discover_client *discover_client;
	struct waitset *waitset;
};

struct pbt_client *pbt_client_init(enum pbt_twin_backend backend,
	unsigned int width, unsigned int height, int start_deamon);
void pbt_client_destroy(struct pbt_client *client);
void pbt_client_resize(struct pbt_client *client);

#endif
