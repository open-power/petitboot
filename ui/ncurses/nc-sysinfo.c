/*
 *  Copyright (C) 2013 IBM Corporation
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

#include <string.h>

#include <talloc/talloc.h>
#include <types/types.h>
#include <log/log.h>
#include <util/util.h>
#include <i18n/i18n.h>

#include "nc-cui.h"
#include "nc-textscreen.h"
#include "nc-sysinfo.h"

struct sysinfo_screen {
	struct text_screen text_scr;
};

extern const struct help_text sysinfo_help_text;

struct nc_scr *sysinfo_screen_scr(struct sysinfo_screen *screen)
{
	return text_screen_scr(&screen->text_scr);
}

static void if_info_mac_str(struct interface_info *info,
		char *buf, unsigned int buflen)
{
	return mac_str(info->hwaddr, info->hwaddr_size, buf, buflen);
}

static void sysinfo_screen_populate(struct sysinfo_screen *screen,
		const struct system_info *sysinfo)
{
	unsigned int i;

	text_screen_clear(&screen->text_scr);

#define line(...) text_screen_append_line(&screen->text_scr, __VA_ARGS__)
	if (!sysinfo) {
		line(_("Waiting for system information..."));
		return;
	}

	line("%-12s %s", _("System type:"), sysinfo->type ?: "");
	line("%-12s %s", _("System id:"),   sysinfo->identifier ?: "");

	if (sysinfo->n_blockdevs) {
		line(NULL);
		line(_("Storage devices"));
	}

	for (i = 0; i < sysinfo->n_blockdevs; i++) {
		struct blockdev_info *info = sysinfo->blockdevs[i];

		line("%s:", info->name);
		line(_(" UUID:       %s"), info->uuid);
		line(_(" mounted at: %s"), info->mountpoint);
		line(NULL);
	}

	if (sysinfo->n_interfaces) {
		line(NULL);
		line("Network interfaces");
	}

	for (i = 0; i < sysinfo->n_interfaces; i++) {
		struct interface_info *info = sysinfo->interfaces[i];
		char macbuf[32];

		if_info_mac_str(info, macbuf, sizeof(macbuf));

		line("%s:", info->name);
		line(_(" MAC:  %s"), macbuf);
		line(_(" link: %s"), info->link ? "up" : "down");
		line(NULL);
	}

#undef line
}

void sysinfo_screen_update(struct sysinfo_screen *screen,
		const struct system_info *sysinfo)
{
	sysinfo_screen_populate(screen, sysinfo);
	text_screen_draw(&screen->text_scr);
}

struct sysinfo_screen *sysinfo_screen_init(struct cui *cui,
		const struct system_info *sysinfo,
		void (*on_exit)(struct cui *))
{
	struct sysinfo_screen *screen;

	screen = talloc_zero(cui, struct sysinfo_screen);
	text_screen_init(&screen->text_scr, cui,
			_("Petitboot System Information"), on_exit);
	text_screen_set_help(&screen->text_scr,
			_("System Information"), &sysinfo_help_text);

	sysinfo_screen_update(screen, sysinfo);

	return screen;
}
