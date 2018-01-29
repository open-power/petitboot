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

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <pb-config/pb-config.h>
#include <talloc/talloc.h>
#include <types/types.h>
#include <log/log.h>
#include <i18n/i18n.h>

#include "nc-cui.h"
#include "nc-config.h"
#include "nc-widgets.h"

#define N_FIELDS	48

extern struct help_text config_help_text;

enum net_conf_type {
	NET_CONF_TYPE_DHCP_ALL,
	NET_CONF_TYPE_DHCP_ONE,
	NET_CONF_TYPE_STATIC,
};

struct config_screen {
	struct nc_scr		scr;
	struct cui		*cui;
	struct nc_widgetset	*widgetset;
	WINDOW			*pad;

	bool			exit;
	bool			show_help;
	bool			show_subset;
	bool			need_redraw;
	bool			need_update;

	void			(*on_exit)(struct cui *);

	int			scroll_y;

	int			label_x;
	int			field_x;
	int			network_config_y;

	enum net_conf_type	net_conf_type;

	bool			autoboot_enabled;
	bool			ipmi_override;
	bool			net_override;

	struct {
		struct nc_widget_label		*autoboot_l;
		struct nc_widget_select		*autoboot_f;
		struct nc_widget_label		*boot_order_l;
		struct nc_widget_subset		*boot_order_f;
		struct nc_widget_label		*boot_empty_l;
		struct nc_widget_button		*boot_add_b;
		struct nc_widget_button		*boot_none_b;
		struct nc_widget_button		*boot_any_b;
		struct nc_widget_textbox	*timeout_f;
		struct nc_widget_label		*timeout_l;
		struct nc_widget_label		*timeout_help_l;

		struct nc_widget_label		*ipmi_type_l;
		struct nc_widget_label		*ipmi_clear_l;
		struct nc_widget_button		*ipmi_clear_b;

		struct nc_widget_label		*network_l;
		struct nc_widget_select		*network_f;

		struct nc_widget_label		*iface_l;
		struct nc_widget_select		*iface_f;
		struct nc_widget_label		*ip_addr_l;
		struct nc_widget_textbox	*ip_addr_f;
		struct nc_widget_label		*ip_mask_l;
		struct nc_widget_textbox	*ip_mask_f;
		struct nc_widget_label		*ip_addr_mask_help_l;
		struct nc_widget_label		*gateway_l;
		struct nc_widget_textbox	*gateway_f;
		struct nc_widget_label		*gateway_help_l;
		struct nc_widget_label		*url_l;
		struct nc_widget_textbox	*url_f;
		struct nc_widget_label		*url_help_l;
		struct nc_widget_label		*dns_l;
		struct nc_widget_textbox	*dns_f;
		struct nc_widget_label		*dns_dhcp_help_l;
		struct nc_widget_label		*dns_help_l;
		struct nc_widget_label		*http_proxy_l;
		struct nc_widget_textbox	*http_proxy_f;
		struct nc_widget_label		*https_proxy_l;
		struct nc_widget_textbox	*https_proxy_f;

		struct nc_widget_label		*allow_write_l;
		struct nc_widget_select		*allow_write_f;
		struct nc_widget_label		*boot_console_l;
		struct nc_widget_select		*boot_console_f;
		struct nc_widget_label		*manual_console_l;
		struct nc_widget_label		*current_console_l;

		struct nc_widget_label		*net_override_l;
		struct nc_widget_label		*safe_mode;
		struct nc_widget_button		*ok_b;
		struct nc_widget_button		*help_b;
		struct nc_widget_button		*cancel_b;
	} widgets;
};

static struct config_screen *config_screen_from_scr(struct nc_scr *scr)
{
	struct config_screen *config_screen;

	assert(scr->sig == pb_config_screen_sig);
	config_screen = (struct config_screen *)
		((char *)scr - (size_t)&((struct config_screen *)0)->scr);
	assert(config_screen->scr.sig == pb_config_screen_sig);
	return config_screen;
}

static void pad_refresh(struct config_screen *screen)
{
	int y, x, rows, cols;

	getmaxyx(screen->scr.sub_ncw, rows, cols);
	getbegyx(screen->scr.sub_ncw, y, x);

	prefresh(screen->pad, screen->scroll_y, 0, y, x, rows, cols);
}

static void config_screen_process_key(struct nc_scr *scr, int key)
{
	struct config_screen *screen = config_screen_from_scr(scr);
	bool handled;

	handled = widgetset_process_key(screen->widgetset, key);

	if (!handled) {
		switch (key) {
		case 'x':
		case 27: /* esc */
			screen->exit = true;
			break;
		case 'h':
			screen->show_help = true;
			break;
		}
	}

	if (screen->exit) {
		screen->on_exit(screen->cui);

	} else if (screen->show_help) {
		screen->show_help = false;
		screen->need_redraw = true;
		cui_show_help(screen->cui, _("System Configuration"),
				&config_help_text);

	} else if (handled && !screen->show_subset) {
		pad_refresh(screen);
	}
}

static void config_screen_resize(struct nc_scr *scr)
{
	struct config_screen *screen = config_screen_from_scr(scr);
	(void)screen;
}

static int config_screen_unpost(struct nc_scr *scr)
{
	struct config_screen *screen = config_screen_from_scr(scr);
	widgetset_unpost(screen->widgetset);
	return 0;
}

struct nc_scr *config_screen_scr(struct config_screen *screen)
{
	return &screen->scr;
}

static int screen_process_form(struct config_screen *screen)
{
	const struct system_info *sysinfo = screen->cui->sysinfo;
	enum net_conf_type net_conf_type;
	struct interface_config *iface;
	bool allow_write;
	char *str, *end;
	struct config *config;
	int i, n_boot_opts, rc;
	unsigned int *order, idx;
	char mac[20];

	config = config_copy(screen, screen->cui->config);

	talloc_free(config->autoboot_opts);
	config->n_autoboot_opts = 0;

	n_boot_opts = widget_subset_get_order(config, &order,
					      screen->widgets.boot_order_f);

	config->autoboot_enabled = widget_select_get_value(
						screen->widgets.autoboot_f);

	config->n_autoboot_opts = n_boot_opts;
	config->autoboot_opts = talloc_array(config, struct autoboot_option,
					     n_boot_opts);

	for (i = 0; i < n_boot_opts; i++) {
		if (order[i] < sysinfo->n_blockdevs) {
			/* disk uuid */
			config->autoboot_opts[i].boot_type = BOOT_DEVICE_UUID;
			config->autoboot_opts[i].uuid = talloc_strdup(config,
							sysinfo->blockdevs[order[i]]->uuid);
		} else if(order[i] < (sysinfo->n_blockdevs + sysinfo->n_interfaces)) {
			/* net uuid */
			order[i] -= sysinfo->n_blockdevs;
			config->autoboot_opts[i].boot_type = BOOT_DEVICE_UUID;
			mac_str(sysinfo->interfaces[order[i]]->hwaddr,
				sysinfo->interfaces[order[i]]->hwaddr_size,
				mac, sizeof(mac));
			config->autoboot_opts[i].uuid = talloc_strdup(config, mac);
		} else {
			/* device type */
			order[i] -= (sysinfo->n_blockdevs + sysinfo->n_interfaces);
			config->autoboot_opts[i].boot_type = BOOT_DEVICE_TYPE;
			config->autoboot_opts[i].type = order[i];
		}
	}

	str = widget_textbox_get_value(screen->widgets.timeout_f);
	if (str) {
		unsigned long x;
		errno = 0;
		x = strtoul(str, &end, 10);
		if (!errno && end != str)
			config->autoboot_timeout_sec = x;
	}

	net_conf_type = widget_select_get_value(screen->widgets.network_f);

	/* if we don't have any network interfaces, prevent per-interface
	 * configuration */
	if (sysinfo->n_interfaces == 0)
		net_conf_type = NET_CONF_TYPE_DHCP_ALL;

	if (net_conf_type == NET_CONF_TYPE_DHCP_ALL) {
		config->network.n_interfaces = 0;

	} else {
		iface = talloc_zero(config, struct interface_config);
		config->network.n_interfaces = 1;
		config->network.interfaces = talloc_array(config,
				struct interface_config *, 1);
		config->network.interfaces[0] = iface;

		/* copy hwaddr (from the sysinfo interface data) to
		 * the configuration */
		idx = widget_select_get_value(screen->widgets.iface_f);
		memcpy(iface->hwaddr, sysinfo->interfaces[idx]->hwaddr,
				sizeof(iface->hwaddr));
	}

	if (net_conf_type == NET_CONF_TYPE_DHCP_ONE) {
		iface->method = CONFIG_METHOD_DHCP;
	}

	if (net_conf_type == NET_CONF_TYPE_STATIC) {
		char *ip, *mask, *gateway, *url;

		ip = widget_textbox_get_value(screen->widgets.ip_addr_f);
		mask = widget_textbox_get_value(screen->widgets.ip_mask_f);
		gateway = widget_textbox_get_value(screen->widgets.gateway_f);
		url = widget_textbox_get_value(screen->widgets.url_f);

		if (!ip || !*ip || !mask || !*mask) {
			screen->scr.frame.status =
				_("No IP / mask values are set");
			nc_scr_frame_draw(&screen->scr);
			talloc_free(config);
			return -1;
		}

		iface->method = CONFIG_METHOD_STATIC;
		iface->static_config.address = talloc_asprintf(iface, "%s/%s",
				ip, mask);
		iface->static_config.gateway = talloc_strdup(iface, gateway);
		iface->static_config.url = talloc_strdup(iface, url);
	}

	str = widget_textbox_get_value(screen->widgets.dns_f);
	talloc_free(config->network.dns_servers);
	config->network.dns_servers = NULL;
	config->network.n_dns_servers = 0;

	if (str && strlen(str)) {
		char *dns, *tmp;
		int i;

		for (;;) {
			dns = strtok_r(str, " \t", &tmp);

			if (!dns)
				break;

			i = config->network.n_dns_servers++;
			config->network.dns_servers = talloc_realloc(config,
					config->network.dns_servers,
					const char *,
					config->network.n_dns_servers);
			config->network.dns_servers[i] =
				talloc_strdup(config, dns);

			str = NULL;
		}
	}

	talloc_free(config->http_proxy);
	talloc_free(config->https_proxy);
	str = widget_textbox_get_value(screen->widgets.http_proxy_f);
	config->http_proxy = talloc_strdup(config, str);
	str = widget_textbox_get_value(screen->widgets.https_proxy_f);
	config->https_proxy = talloc_strdup(config, str);

	allow_write = widget_select_get_value(screen->widgets.allow_write_f);
	if (allow_write != config->allow_writes)
		config->allow_writes = allow_write;

	if (config->n_consoles && !config->manual_console) {
		idx = widget_select_get_value(screen->widgets.boot_console_f);
		if (!config->boot_console) {
			config->boot_console = talloc_strdup(config,
							config->consoles[idx]);
		} else if (strncmp(config->boot_console, config->consoles[idx],
				strlen(config->boot_console)) != 0) {
			talloc_free(config->boot_console);
			config->boot_console = talloc_strdup(config,
							config->consoles[idx]);
		}
	}

	config->safe_mode = false;
	rc = cui_send_config(screen->cui, config);
	talloc_free(config);

	if (rc)
		pb_log("cui_send_config failed!\n");
	else
		pb_debug("config sent!\n");

	return 0;
}

static void ok_click(void *arg)
{
	struct config_screen *screen = arg;
	if (screen_process_form(screen))
		/* errors are written to the status line, so we'll need
		 * to refresh */
		wrefresh(screen->scr.main_ncw);
	else
		screen->exit = true;
}

static void help_click(void *arg)
{
	struct config_screen *screen = arg;
	screen->show_help = true;
}

static void cancel_click(void *arg)
{
	struct config_screen *screen = arg;
	screen->exit = true;
}

static void ipmi_clear_click(void *arg)
{
	struct config_screen *screen = arg;
	struct config *config;
	int rc;

	config = config_copy(screen, screen->cui->config);
	config->ipmi_bootdev = IPMI_BOOTDEV_INVALID;
	config->safe_mode = false;

	rc = cui_send_config(screen->cui, config);
	talloc_free(config);

	if (rc)
		pb_log("cui_send_config failed!\n");
	else
		pb_debug("config sent!\n");
	screen->exit = true;
}

static int layout_pair(struct config_screen *screen, int y,
		struct nc_widget_label *label,
		struct nc_widget *field)
{
	struct nc_widget *label_w = widget_label_base(label);
	widget_move(label_w, y, screen->label_x);
	widget_move(field, y, screen->field_x);
	return max(widget_height(label_w), widget_height(field));
}

static void config_screen_layout_widgets(struct config_screen *screen)
{
	struct nc_widget *wl, *wf, *wh;
	int y, x, help_x;
	bool show;

	y = 1;
	/* currently, the longest label we have is the DNS-servers
	 * widget, so layout our screen based on that */
	help_x = screen->field_x + 2 +
		widget_width(widget_textbox_base(screen->widgets.dns_f));

	wl = widget_label_base(screen->widgets.autoboot_l);
	widget_set_visible(wl, true);
	widget_move(wl, y, screen->label_x);

	wf = widget_select_base(screen->widgets.autoboot_f);
	widget_set_visible(wf, true);
	widget_move(wf, y, screen->field_x);
	y += widget_height(wf);

	show = screen->autoboot_enabled;

	if (show)
		y += 1;

	wl = widget_label_base(screen->widgets.boot_order_l);
	widget_set_visible(wl, show);
	widget_move(wl, y, screen->label_x);

	wf = widget_subset_base(screen->widgets.boot_order_f);
	widget_move(wf, y, screen->field_x);
	wl = widget_label_base(screen->widgets.boot_empty_l);
	widget_move(wl, y, screen->field_x);

	if (widget_subset_height(screen->widgets.boot_order_f)) {
		widget_set_visible(wl, false);
		widget_set_visible(wf, show);
		y += show ? widget_height(wf) : 0;
	} else {
		widget_set_visible(wl, show);
		widget_set_visible(wf, false);
		y += show ? 1 : 0;
	}

	if (show) {
		y += 1;
		widget_move(widget_button_base(screen->widgets.boot_add_b),
				y++, screen->field_x);
		widget_move(widget_button_base(screen->widgets.boot_any_b),
				y++, screen->field_x);
		widget_move(widget_button_base(screen->widgets.boot_none_b),
				y, screen->field_x);
	}

	wf = widget_button_base(screen->widgets.boot_add_b);
	if (widget_subset_n_inactive(screen->widgets.boot_order_f) && show)
		widget_set_visible(wf, true);
	else
		widget_set_visible(wf, false);

	if (show)
		y += 2;

	widget_set_visible(widget_button_base(screen->widgets.boot_any_b), show);
	widget_set_visible(widget_button_base(screen->widgets.boot_none_b), show);

	wf = widget_textbox_base(screen->widgets.timeout_f);
	wl = widget_label_base(screen->widgets.timeout_l);
	wh = widget_label_base(screen->widgets.timeout_help_l);
	widget_set_visible(wl, screen->autoboot_enabled);
	widget_set_visible(wf, screen->autoboot_enabled);
	widget_set_visible(wh, screen->autoboot_enabled);
	if (screen->autoboot_enabled) {
		widget_set_visible(wh, screen->autoboot_enabled);
		widget_move(wl, y, screen->label_x);
		widget_move(wf, y, screen->field_x);
		widget_move(wh, y, screen->field_x + widget_width(wf) + 1);
		y += 2;
	} else
		y += 1;

	if (screen->ipmi_override) {
		wl = widget_label_base(screen->widgets.ipmi_type_l);
		widget_set_visible(wl, true);
		widget_move(wl, y, screen->label_x);
		y += 1;

		wl = widget_label_base(screen->widgets.ipmi_clear_l);
		wf = widget_button_base(screen->widgets.ipmi_clear_b);
		widget_set_visible(wl, true);
		widget_set_visible(wf, true);
		widget_move(wl, y, screen->label_x);
		widget_move(wf, y, screen->field_x);
		y += 1;
	}

	y += 1;

	y += layout_pair(screen, y, screen->widgets.network_l,
			widget_select_base(screen->widgets.network_f));

	y += 1;

	/* conditionally show iface select */
	wl = widget_label_base(screen->widgets.iface_l);
	wf = widget_select_base(screen->widgets.iface_f);

	show = screen->net_conf_type == NET_CONF_TYPE_DHCP_ONE ||
		screen->net_conf_type == NET_CONF_TYPE_STATIC;

	widget_set_visible(wl, show);
	widget_set_visible(wf, show);

	if (show)
		y += layout_pair(screen, y, screen->widgets.iface_l, wf) + 1;

	/* conditionally show static IP params */
	show = screen->net_conf_type == NET_CONF_TYPE_STATIC;

	wl = widget_label_base(screen->widgets.ip_addr_l);
	wf = widget_textbox_base(screen->widgets.ip_addr_f);
	widget_set_visible(wl, show);
	widget_set_visible(wf, show);
	x = screen->field_x + widget_width(wf) + 1;

	if (show)
		layout_pair(screen, y, screen->widgets.ip_addr_l, wf);

	wl = widget_label_base(screen->widgets.ip_mask_l);
	wf = widget_textbox_base(screen->widgets.ip_mask_f);
	widget_set_visible(wl, show);
	widget_set_visible(wf, show);

	if (show) {
		widget_move(wl, y, x);
		widget_move(wf, y, x + 2);
	}

	/* help for IP/mask */
	wh = widget_label_base(screen->widgets.ip_addr_mask_help_l);
	widget_set_visible(wh, show);
	if (show) {
		widget_move(wh, y, help_x);
		y++;
	}

	wl = widget_label_base(screen->widgets.gateway_l);
	wf = widget_textbox_base(screen->widgets.gateway_f);
	wh = widget_label_base(screen->widgets.gateway_help_l);
	widget_set_visible(wl, show);
	widget_set_visible(wf, show);
	widget_set_visible(wh, show);

	if (show) {
		layout_pair(screen, y, screen->widgets.gateway_l, wf);
		widget_move(wh, y, help_x);
		y++;
	}

	wl = widget_label_base(screen->widgets.url_l);
	wf = widget_textbox_base(screen->widgets.url_f);
	wh = widget_label_base(screen->widgets.url_help_l);
	widget_set_visible(wl, show);
	widget_set_visible(wf, show);
	widget_set_visible(wh, show);

	if (show) {
		layout_pair(screen, y, screen->widgets.url_l, wf);
		widget_move(wh, y, help_x);
		y++;
	}

	wh = widget_label_base(screen->widgets.dns_help_l);
	layout_pair(screen, y, screen->widgets.dns_l,
			widget_textbox_base(screen->widgets.dns_f));
	widget_move(wh, y, help_x);
	y++;

	/* we show the DNS/DHCP help if we're configuring DHCP */
	show = screen->net_conf_type != NET_CONF_TYPE_STATIC;
	wl = widget_label_base(screen->widgets.dns_dhcp_help_l);
	widget_set_visible(wl, show);
	if (show) {
		widget_move(wl, y, screen->field_x);
		y += 1;
	}

	wf = widget_textbox_base(screen->widgets.http_proxy_f);
	layout_pair(screen, y, screen->widgets.http_proxy_l, wf);
	y++;
	wf = widget_textbox_base(screen->widgets.https_proxy_f);
	layout_pair(screen, y, screen->widgets.https_proxy_l, wf);
	y++;

	y += 1;

	layout_pair(screen, y, screen->widgets.allow_write_l,
		    widget_select_base(screen->widgets.allow_write_f));
	y += widget_height(widget_select_base(screen->widgets.allow_write_f));

	y += 1;

	if (screen->widgets.manual_console_l) {
		layout_pair(screen, y++, screen->widgets.boot_console_l,
			widget_label_base(screen->widgets.manual_console_l));
		widget_move(widget_label_base(screen->widgets.current_console_l),
			y, screen->field_x);
		widget_set_visible(widget_select_base(
			screen->widgets.boot_console_f), false);
		y += 2;
	} else if (widget_height(widget_select_base(screen->widgets.boot_console_f))) {
		layout_pair(screen, y, screen->widgets.boot_console_l,
			    widget_select_base(screen->widgets.boot_console_f));
		y += widget_height(widget_select_base(screen->widgets.boot_console_f));
		widget_move(widget_label_base(screen->widgets.current_console_l),
			y, screen->field_x);
		y += 2;
	} else {
		widget_set_visible(widget_label_base(
					screen->widgets.boot_console_l), false);
		widget_set_visible(widget_select_base(
					screen->widgets.boot_console_f), false);
		widget_set_visible(widget_label_base(
					screen->widgets.current_console_l), false);
	}

	if (screen->net_override) {
		widget_move(widget_label_base(screen->widgets.net_override_l),
				y, screen->label_x);
		widget_set_visible(widget_label_base(screen->widgets.net_override_l),
					true);
		y += 1;
	}

	if (screen->cui->config->safe_mode) {
		widget_move(widget_label_base(screen->widgets.safe_mode),
			y, screen->label_x);
		widget_set_visible(widget_label_base(screen->widgets.safe_mode),
					true);
		y += 1;
	}

	widget_move(widget_button_base(screen->widgets.ok_b),
			y, screen->field_x);
	widget_move(widget_button_base(screen->widgets.help_b),
			y, screen->field_x + 14);
	widget_move(widget_button_base(screen->widgets.cancel_b),
			y, screen->field_x + 28);
}

static void config_screen_network_change(void *arg, int value)
{
	struct config_screen *screen = arg;
	screen->net_conf_type = value;
	widgetset_unpost(screen->widgetset);
	config_screen_layout_widgets(screen);
	widgetset_post(screen->widgetset);
}

static void config_screen_boot_order_change(void *arg, int value)
{
	(void)value;
	struct config_screen *screen = arg;
	widgetset_unpost(screen->widgetset);
	config_screen_layout_widgets(screen);
	widgetset_post(screen->widgetset);
}

static void config_screen_autoboot_change(void *arg, int value)
{
	struct config_screen *screen = arg;
	screen->autoboot_enabled = !!value;
	widgetset_unpost(screen->widgetset);
	config_screen_layout_widgets(screen);
	widgetset_post(screen->widgetset);
}

static void config_screen_add_device(void *arg)
{
	struct config_screen *screen = arg;

	screen->show_subset = true;
	cui_show_subset(screen->cui, _("Select a boot device to add"),
			screen->widgets.boot_order_f);
}

static void config_screen_autoboot_none(void *arg)
{
	struct config_screen *screen = arg;
	struct nc_widget_subset *subset = screen->widgets.boot_order_f;

	widget_subset_clear_active(subset);

	widgetset_unpost(screen->widgetset);
	config_screen_layout_widgets(screen);
	widgetset_post(screen->widgetset);
}

static void config_screen_autoboot_any(void *arg)
{
	struct config_screen *screen = arg;
	const struct system_info *sysinfo = screen->cui->sysinfo;
	struct nc_widget_subset *subset = screen->widgets.boot_order_f;
	int idx;

	widget_subset_clear_active(subset);

	idx = sysinfo->n_blockdevs + sysinfo->n_interfaces + DEVICE_TYPE_ANY;

	widget_subset_make_active(screen->widgets.boot_order_f, idx);

	screen->autoboot_enabled = true;

	widgetset_unpost(screen->widgetset);
	config_screen_layout_widgets(screen);
	widgetset_post(screen->widgetset);
}

static void config_screen_update_subset(void *arg,
			struct nc_widget_subset *subset, int idx)
{
	struct config_screen *screen = arg;

	if (idx >= 0)
		widget_subset_make_active(subset, idx);
	if (!screen->autoboot_enabled)
		screen->autoboot_enabled = true;
	config_screen_layout_widgets(screen);
}

static struct interface_config *first_active_interface(
		const struct config *config)
{
	unsigned int i;

	for (i = 0; i < config->network.n_interfaces; i++) {
		if (config->network.interfaces[i]->ignore)
			continue;
		return config->network.interfaces[i];
	}
	return NULL;
}

static enum net_conf_type find_net_conf_type(const struct config *config)
{
	struct interface_config *ifcfg;

	ifcfg = first_active_interface(config);

	if (!ifcfg)
		return NET_CONF_TYPE_DHCP_ALL;

	else if (ifcfg->method == CONFIG_METHOD_DHCP)
		return NET_CONF_TYPE_DHCP_ONE;

	else if (ifcfg->method == CONFIG_METHOD_STATIC)
		return NET_CONF_TYPE_STATIC;

	assert(0);
	return NET_CONF_TYPE_DHCP_ALL;
}

static void config_screen_setup_empty(struct config_screen *screen)
{
	widget_new_label(screen->widgetset, 2, screen->field_x,
			_("Waiting for configuration data..."));
	screen->widgets.cancel_b = widget_new_button(screen->widgetset,
			4, screen->field_x, 9, _("Cancel"),
			cancel_click, screen);
}

static int find_autoboot_idx(const struct system_info *sysinfo,
		struct autoboot_option *opt)
{
	unsigned int i;

	if (opt->boot_type == BOOT_DEVICE_TYPE)
		return sysinfo->n_blockdevs + sysinfo->n_interfaces + opt->type;

	for (i = 0; i < sysinfo->n_blockdevs; i++) {
		if (!strcmp(sysinfo->blockdevs[i]->uuid, opt->uuid))
			return i;
	}

	for (i = 0; i < sysinfo->n_interfaces; i++) {
		struct interface_info *info = sysinfo->interfaces[i];
		char mac[20];

		mac_str(info->hwaddr, info->hwaddr_size, mac, sizeof(mac));

		if (!strcmp(mac, opt->uuid))
			return sysinfo->n_blockdevs + i;
	}

	return -1;
}

static void config_screen_setup_widgets(struct config_screen *screen,
		const struct config *config,
		const struct system_info *sysinfo)
{
	struct nc_widgetset *set = screen->widgetset;
	struct interface_config *ifcfg;
	char *str, *ip, *mask, *gw, *url, *tty, *label;
	enum net_conf_type type;
	unsigned int i;
	int add_len, clear_len, any_len, min_len = 20;
	bool found;

	build_assert(sizeof(screen->widgets) / sizeof(struct widget *)
			== N_FIELDS);

	type = screen->net_conf_type;
	ifcfg = first_active_interface(config);

	screen->autoboot_enabled = config->autoboot_enabled;

	screen->widgets.autoboot_l = widget_new_label(set, 0, 0,
					_("Autoboot:"));
	screen->widgets.autoboot_f = widget_new_select(set, 0, 0,
					COLS - screen->field_x - 1);

	widget_select_add_option(screen->widgets.autoboot_f, 0, _("Disabled"),
				 !screen->autoboot_enabled);
	widget_select_add_option(screen->widgets.autoboot_f, 1, _("Enabled"),
				 screen->autoboot_enabled);

	widget_select_on_change(screen->widgets.autoboot_f,
			config_screen_autoboot_change, screen);

	add_len = max(min_len, strncols(_("Add Device")));
	clear_len = max(min_len, strncols(_("Clear")));
	any_len = max(min_len, strncols(_("Clear & Boot Any")));

	screen->widgets.boot_add_b = widget_new_button(set, 0, 0, add_len,
					_("Add Device"),
					config_screen_add_device, screen);

	screen->widgets.boot_none_b = widget_new_button(set, 0, 0, clear_len,
					_("Clear"),
					config_screen_autoboot_none, screen);

	screen->widgets.boot_any_b = widget_new_button(set, 0, 0, any_len,
					_("Clear & Boot Any"),
					config_screen_autoboot_any, screen);

	screen->widgets.boot_order_l = widget_new_label(set, 0, 0,
					_("Boot Order:"));
	screen->widgets.boot_order_f = widget_new_subset(set, 0, 0,
					COLS - screen->field_x,
					config_screen_update_subset);
	screen->widgets.boot_empty_l = widget_new_label(set, 0, 0,
					_("(None)"));

	widget_subset_on_change(screen->widgets.boot_order_f,
			config_screen_boot_order_change, screen);

	for (i = 0; i < sysinfo->n_blockdevs; i++) {
		struct blockdev_info *bd = sysinfo->blockdevs[i];

		label = talloc_asprintf(screen, _("disk: %s [uuid: %s]"),
				bd->name, bd->uuid);

		widget_subset_add_option(screen->widgets.boot_order_f, label);
	}

	for (i = 0; i < sysinfo->n_interfaces; i++) {
		struct interface_info *info = sysinfo->interfaces[i];
		char mac[20];

		mac_str(info->hwaddr, info->hwaddr_size, mac, sizeof(mac));

		label = talloc_asprintf(screen, _("net:  %s [mac: %s]"),
				info->name, mac);

		widget_subset_add_option(screen->widgets.boot_order_f, label);
	}

	for (i = DEVICE_TYPE_NETWORK; i < DEVICE_TYPE_UNKNOWN; i++) {

		if (i == DEVICE_TYPE_ANY)
			label = talloc_asprintf(screen, _("Any Device"));
		else
			label = talloc_asprintf(screen, _("Any %s device"),
						device_type_display_name(i));

		widget_subset_add_option(screen->widgets.boot_order_f, label);
	}

	for (i = 0; i < config->n_autoboot_opts; i++) {
		struct autoboot_option *opt = &config->autoboot_opts[i];
		int idx;

		idx = find_autoboot_idx(sysinfo, opt);

		if (idx >= 0) {
			widget_subset_make_active(screen->widgets.boot_order_f,
						  idx);
		} else {
			if (opt->boot_type == BOOT_DEVICE_TYPE)
				pb_log("%s: Unknown autoboot option: %d\n",
				       __func__, opt->type);
			else
				pb_log("%s: Unknown autoboot UUID: %s\n",
				       __func__, opt->uuid);
		}
	}


	str = talloc_asprintf(screen, "%d", config->autoboot_timeout_sec);
	screen->widgets.timeout_l = widget_new_label(set, 0, 0, _("Timeout:"));
	screen->widgets.timeout_f = widget_new_textbox(set, 0, 0, 5, str);
	screen->widgets.timeout_help_l = widget_new_label(set, 0, 0,
					_("seconds"));

	widget_textbox_set_fixed_size(screen->widgets.timeout_f);
	widget_textbox_set_validator_integer(screen->widgets.timeout_f, 0, 999);

	if (config->ipmi_bootdev) {
		label = talloc_asprintf(screen,
				_("%s IPMI boot option: %s"),
				config->ipmi_bootdev_persistent ?
				_("Persistent") : _("Temporary"),
				ipmi_bootdev_display_name(config->ipmi_bootdev));
		screen->widgets.ipmi_type_l = widget_new_label(set, 0, 0,
							label);
		screen->widgets.ipmi_clear_l = widget_new_label(set, 0, 0,
							_("Clear option:"));
		screen->widgets.ipmi_clear_b = widget_new_button(set, 0, 0,
				strncols(_("Clear IPMI override now")) + 10,
				_("Clear IPMI override now"),
				ipmi_clear_click, screen);
		screen->ipmi_override = true;
	}

	screen->widgets.network_l = widget_new_label(set, 0, 0, _("Network:"));
	screen->widgets.network_f = widget_new_select(set, 0, 0,
						COLS - screen->field_x - 1);

	widget_select_add_option(screen->widgets.network_f,
					NET_CONF_TYPE_DHCP_ALL,
					_("DHCP on all active interfaces"),
					type == NET_CONF_TYPE_DHCP_ALL);
	widget_select_add_option(screen->widgets.network_f,
					NET_CONF_TYPE_DHCP_ONE,
					_("DHCP on a specific interface"),
					type == NET_CONF_TYPE_DHCP_ONE);
	widget_select_add_option(screen->widgets.network_f,
					NET_CONF_TYPE_STATIC,
					_("Static IP configuration"),
					type == NET_CONF_TYPE_STATIC);

	widget_select_on_change(screen->widgets.network_f,
			config_screen_network_change, screen);

	screen->widgets.iface_l = widget_new_label(set, 0, 0, _("Device:"));
	screen->widgets.iface_f = widget_new_select(set, 0, 0, 50);

	for (i = 0; i < sysinfo->n_interfaces; i++) {
		struct interface_info *info = sysinfo->interfaces[i];
		char str[50], mac[20];
		bool is_default;

		is_default = ifcfg && !memcmp(ifcfg->hwaddr, info->hwaddr,
					sizeof(ifcfg->hwaddr));

		mac_str(info->hwaddr, info->hwaddr_size, mac, sizeof(mac));
		snprintf(str, sizeof(str), "%s [%s, %s]", info->name, mac,
				info->link ? _("link up") : _("link down"));

		widget_select_add_option(screen->widgets.iface_f,
						i, str, is_default);
	}

	url = gw = ip = mask = NULL;
	if (ifcfg && ifcfg->method == CONFIG_METHOD_STATIC) {
		char *sep;

		str = talloc_strdup(screen, ifcfg->static_config.address);
		sep = strchr(str, '/');
		ip = str;

		if (sep) {
			*sep = '\0';
			mask = sep + 1;
		}
		gw = ifcfg->static_config.gateway;
		url = ifcfg->static_config.url;
	}

	screen->net_override = ifcfg && ifcfg->override;
	if (screen->net_override) {
		screen->widgets.net_override_l = widget_new_label(set, 0, 0,
			_("Network Override Active! 'OK' will overwrite interface config"));
	}

	screen->widgets.ip_addr_l = widget_new_label(set, 0, 0, _("IP/mask:"));
	screen->widgets.ip_addr_f = widget_new_textbox(set, 0, 0, 16, ip);
	screen->widgets.ip_mask_l = widget_new_label(set, 0, 0, "/");
	screen->widgets.ip_mask_f = widget_new_textbox(set, 0, 0, 3, mask);
	screen->widgets.ip_addr_mask_help_l =
		widget_new_label(set, 0, 0, _("(eg. 192.168.0.10 / 24)"));

	widget_textbox_set_fixed_size(screen->widgets.ip_addr_f);
	widget_textbox_set_fixed_size(screen->widgets.ip_mask_f);
	widget_textbox_set_validator_ipv4(screen->widgets.ip_addr_f);
	widget_textbox_set_validator_integer(screen->widgets.ip_mask_f, 1, 31);

	screen->widgets.gateway_l = widget_new_label(set, 0, 0, _("Gateway:"));
	screen->widgets.gateway_f = widget_new_textbox(set, 0, 0, 16, gw);
	screen->widgets.gateway_help_l =
		widget_new_label(set, 0, 0, _("(eg. 192.168.0.1)"));

	widget_textbox_set_fixed_size(screen->widgets.gateway_f);
	widget_textbox_set_validator_ipv4(screen->widgets.gateway_f);

	screen->widgets.url_l = widget_new_label(set, 0, 0, _("URL:"));
	screen->widgets.url_f = widget_new_textbox(set, 0, 0, 32, url);
	screen->widgets.url_help_l =
		widget_new_label(set, 0, 0, _("(eg. tftp://)"));

	str = talloc_strdup(screen, "");
	for (i = 0; i < config->network.n_dns_servers; i++) {
		str = talloc_asprintf_append(str, "%s%s",
				(i == 0) ? "" : " ",
				config->network.dns_servers[i]);
	}

	screen->widgets.dns_l = widget_new_label(set, 0, 0,
					_("DNS Server(s):"));
	screen->widgets.dns_f = widget_new_textbox(set, 0, 0, 32, str);
	screen->widgets.dns_help_l =
		widget_new_label(set, 0, 0, _("(eg. 192.168.0.2)"));

	widget_textbox_set_validator_ipv4_multi(screen->widgets.dns_f);

	screen->widgets.dns_dhcp_help_l = widget_new_label(set, 0, 0,
			_("(if not provided by DHCP server)"));

	screen->widgets.http_proxy_l = widget_new_label(set, 0, 0,
					_("HTTP Proxy:"));
	screen->widgets.http_proxy_f = widget_new_textbox(set, 0, 0, 32,
						config->http_proxy);
	screen->widgets.https_proxy_l = widget_new_label(set, 0, 0,
					_("HTTPS Proxy:"));
	screen->widgets.https_proxy_f = widget_new_textbox(set, 0, 0, 32,
						config->https_proxy);

	if (config->safe_mode)
		screen->widgets.safe_mode = widget_new_label(set, 0, 0,
			 _("Selecting 'OK' will exit safe mode"));

	screen->widgets.allow_write_l = widget_new_label(set, 0, 0,
			_("Disk R/W:"));
	screen->widgets.allow_write_f = widget_new_select(set, 0, 0,
						COLS - screen->field_x - 1);

	widget_select_add_option(screen->widgets.allow_write_f, 0,
				_("Prevent all writes to disk"),
				!config->allow_writes);

	widget_select_add_option(screen->widgets.allow_write_f, 1,
				_("Allow bootloader scripts to modify disks"),
				config->allow_writes);

	screen->widgets.boot_console_l = widget_new_label(set, 0, 0,
			_("Boot console:"));
	screen->widgets.boot_console_f = widget_new_select(set, 0, 0,
						COLS - screen->field_x - 1);

	for (i = 0; i < config->n_consoles; i++){
		found = config->boot_console &&
			strncmp(config->boot_console, config->consoles[i],
				strlen(config->boot_console)) == 0;
		widget_select_add_option(screen->widgets.boot_console_f, i,
					config->consoles[i], found);
	}

	if (config->manual_console) {
		label = talloc_asprintf(screen, _("Manually set: '%s'"),
					config->boot_console);
		screen->widgets.manual_console_l = widget_new_label(set, 0, 0, label);
	}

	tty = talloc_asprintf(screen, _("Current interface: %s"),
				ttyname(STDIN_FILENO));
	screen->widgets.current_console_l = widget_new_label(set, 0 , 0, tty);

	screen->widgets.ok_b = widget_new_button(set, 0, 0, 10, _("OK"),
			ok_click, screen);
	screen->widgets.help_b = widget_new_button(set, 0, 0, 10, _("Help"),
			help_click, screen);
	screen->widgets.cancel_b = widget_new_button(set, 0, 0, 10, _("Cancel"),
			cancel_click, screen);
}

static void config_screen_widget_focus(struct nc_widget *widget, void *arg)
{
	struct config_screen *screen = arg;
	int w_y, w_height, w_focus, s_max, adjust;

	w_height = widget_height(widget);
	w_focus = widget_focus_y(widget);
	w_y = widget_y(widget) + w_focus;
	s_max = getmaxy(screen->scr.sub_ncw) - 1;

	if (w_y < screen->scroll_y)
		screen->scroll_y = w_y;

	else if (w_y + screen->scroll_y + 1 > s_max) {
		/* Fit as much of the widget into the screen as possible */
		adjust = min(s_max - 1, w_height - w_focus);
		if (w_y + adjust >= screen->scroll_y + s_max)
			screen->scroll_y = max(0, 1 + w_y + adjust - s_max);
	} else
		return;

	pad_refresh(screen);
}

static void config_screen_draw(struct config_screen *screen,
		const struct config *config,
		const struct system_info *sysinfo)
{
	bool repost = false;
	int height;

	/* The size of the pad we'll need depends on the number of interfaces.
	 *
	 * We use N_FIELDS (which is quite conservative, as some fields share
	 * a line) as a base, then:
	 * - add 6 (as the network select & boot device select fields take 3
	 *   lines each),
	 * - add n_interfaces for every field in the network select field, and
	 * - add (n_blockdevs + n_interfaces) for every field in the boot device
	 *   select field
	 */
	height = N_FIELDS + 6;
	if (sysinfo) {
		height += sysinfo->n_interfaces;
		height += (sysinfo->n_blockdevs + sysinfo->n_interfaces);
	}
	if (!screen->pad || getmaxy(screen->pad) < height) {
		if (screen->pad)
			delwin(screen->pad);
		screen->pad = newpad(height, COLS + 10);
	}

	if (screen->widgetset) {
		widgetset_unpost(screen->widgetset);
		talloc_free(screen->widgetset);
		repost = true;
	}

	screen->widgetset = widgetset_create(screen, screen->scr.main_ncw,
			screen->pad);
	widgetset_set_widget_focus(screen->widgetset,
			config_screen_widget_focus, screen);

	if (!config || !sysinfo) {
		config_screen_setup_empty(screen);
	} else {
		screen->net_conf_type = find_net_conf_type(config);
		config_screen_setup_widgets(screen, config, sysinfo);
		config_screen_layout_widgets(screen);
	}

	if (repost)
		widgetset_post(screen->widgetset);
}

void config_screen_update(struct config_screen *screen,
		const struct config *config,
		const struct system_info *sysinfo)
{
	if (screen->cui->current != config_screen_scr(screen)) {
		screen->need_update = true;
		return;
	}

	config_screen_draw(screen, config, sysinfo);
	pad_refresh(screen);
}

static int config_screen_post(struct nc_scr *scr)
{
	struct config_screen *screen = config_screen_from_scr(scr);
	screen->show_subset = false;

	if (screen->need_update) {
		config_screen_draw(screen, screen->cui->config,
				   screen->cui->sysinfo);
		screen->need_update = false;
	} else {
		widgetset_post(screen->widgetset);
	}

	nc_scr_frame_draw(scr);
	if (screen->need_redraw) {
		redrawwin(scr->main_ncw);
		screen->need_redraw = false;
	}
	wrefresh(screen->scr.main_ncw);
	pad_refresh(screen);
	return 0;
}

static int config_screen_destroy(void *arg)
{
	struct config_screen *screen = arg;
	if (screen->pad)
		delwin(screen->pad);
	return 0;
}

struct config_screen *config_screen_init(struct cui *cui,
		const struct config *config,
		const struct system_info *sysinfo,
		void (*on_exit)(struct cui *))
{
	struct config_screen *screen;

	screen = talloc_zero(cui, struct config_screen);
	talloc_set_destructor(screen, config_screen_destroy);
	nc_scr_init(&screen->scr, pb_config_screen_sig, 0,
			cui, config_screen_process_key,
			config_screen_post, config_screen_unpost,
			config_screen_resize);

	screen->cui = cui;
	screen->on_exit = on_exit;
	screen->need_redraw = false;
	screen->need_update = false;
	screen->label_x = 2;
	screen->field_x = 17;

	screen->ipmi_override = false;
	screen->show_subset = false;

	screen->scr.frame.ltitle = talloc_strdup(screen,
			_("Petitboot System Configuration"));
	screen->scr.frame.rtitle = NULL;
	screen->scr.frame.help = talloc_strdup(screen,
			_("tab=next, shift+tab=previous, x=exit, h=help"));
	nc_scr_frame_draw(&screen->scr);

	scrollok(screen->scr.sub_ncw, true);

	config_screen_draw(screen, config, sysinfo);

	return screen;
}
