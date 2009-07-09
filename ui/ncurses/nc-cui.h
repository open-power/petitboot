/*
 *  Copyright (C) 2009 Sony Computer Entertainment Inc.
 *  Copyright 2009 Sony Corp.
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

#if !defined(_PB_NC_CUI_H)
#define _PB_NC_CUI_H

#include <signal.h>

#include "ui/common/timer.h"
#include "nc-menu.h"
#include "nc-ked.h"

struct cui_opt_data {
	const struct device *dev;
	const struct boot_option *opt;
	uint32_t opt_hash;
	struct pb_kexec_data *kd;
};

/**
 * struct cui - Data structure defining a cui state machine.
 * @c_sig: Signature for callback type checking, should be cui_sig.
 * @abort: When set to true signals the state machine to exit.
 * @current: Pointer to the active nc object.
 * @main: Pointer to the user supplied main menu.
 *
 * Device boot_options are dynamically added and removed from the @main
 * menu.
 */

struct cui {
	enum pb_nc_sig c_sig;
	sig_atomic_t abort;
	sig_atomic_t resize;
	struct nc_scr *current;
	struct pmenu *main;
	struct ui_timer timer;
	void *platform_info;
	unsigned int default_item;
	int (*on_kexec)(struct cui *cui, struct cui_opt_data *cod);
};

struct cui *cui_init(void* platform_info,
	int (*on_kexec)(struct cui *, struct cui_opt_data *));
struct nc_scr *cui_set_current(struct cui *cui, struct nc_scr *scr);
int cui_run(struct cui *cui, struct pmenu *main, unsigned int default_item);
int cui_ked_run(struct pmenu_item *item);

/* convenience routines */

void cui_abort(struct cui *cui);
void cui_resize(struct cui *cui);
void cui_on_exit(struct pmenu *menu);
int cui_run_cmd(struct pmenu_item *item);

static inline struct cui *cui_from_arg(void *arg)
{
	struct cui *cui = (struct cui *)arg;

	assert(cui->c_sig == pb_cui_sig);
	return cui;
}

static inline struct cui *cui_from_pmenu(struct pmenu *menu)
{
	return cui_from_arg(menu->scr.ui_ctx);
}

static inline struct cui *cui_from_item(struct pmenu_item *item)
{
	return cui_from_pmenu(item->pmenu);
}

static inline struct cui *cui_from_timer(struct ui_timer *timer)
{
	struct cui *cui;

	cui = (struct cui *)((char *)timer
		- (size_t)&((struct cui *)0)->timer);
	assert(cui->c_sig == pb_cui_sig);

	return cui;
}

#endif
