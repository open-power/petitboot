/*
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
 *
 *  Copyright (C) 2018 Huaxintong Semiconductor Technology Co.,Ltd. All rights
 *  reserved.
 *  Author: Ge Song <ge.song@hxt-semitech.com>
 */

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "efi/efivar.h"
#include "file/file.h"
#include "log/log.h"
#include <process/process.h>
#include <system/system.h>
#include "talloc/talloc.h"
#include "types/types.h"

#include "ipmi.h"
#include "platform.h"

struct platform_arm64 {
	const struct efi_mount *efi_mount;
	struct param_list *params;
	struct ipmi *ipmi;
};

static inline struct platform_arm64 *to_platform_arm64(struct platform *p)
{
	return (struct platform_arm64 *)(p->platform_data);
}

static void read_efivars(const struct efi_mount *efi_mount,
	struct param_list *pl)
{
	const char** known;

	if (!efi_mount)
		return;

	param_list_for_each_known_param(pl, known) {
		struct efi_data *efi_data;

		if (efi_get_variable(NULL, efi_mount, *known, &efi_data))
			continue;

		param_list_set(pl, *known, efi_data->data, true);
		talloc_free(efi_data);
	}
}

static void write_efivars(const struct efi_mount *efi_mount,
	const struct param_list *pl)
{
	struct efi_data efi_data;
	struct param *param;

	if (!efi_mount)
		return;

	efi_data.attributes = EFI_DEFALT_ATTRIBUTES;

	param_list_for_each(pl, param) {
		if (!param->modified)
			continue;

		efi_data.data = param->value;
		efi_data.data_size = strlen(param->value) + 1;
		efi_set_variable(efi_mount, param->name, &efi_data);
	}
}

static void get_active_consoles(struct config *config)
{
	config->n_consoles = 0;
	config->consoles = talloc_array(config, char *, 3);
	if (!config->consoles)
		return;

	config->consoles[0] = talloc_asprintf(config,
					"/dev/hvc0 [IPMI / Serial]");
	config->consoles[1] = talloc_asprintf(config,
					"/dev/ttyAMA0 [Serial]");
	config->consoles[2] = talloc_asprintf(config,
					"/dev/tty1 [VGA]");
	config->n_consoles = 3;
}

static int load_config(struct platform *p, struct config *config)
{
	const struct efi_mount *efi_mount = to_platform_arm64(p)->efi_mount;
	struct param_list *pl = to_platform_arm64(p)->params;

	read_efivars(efi_mount, pl);
	config_populate_all(config, pl);
	get_active_consoles(config);

	return 0;
}

static char *stdout_cleaner(struct process_stdout *out)
{
	char *p;
	const char *const end = out->buf + out->len;

	for (p = out->buf; *p && p < end; p++) {
		if (*p == '\n' || *p == '\r') {
			*p = 0;
		} else if (!isprint(*p)) {
			*p = '.';
		} else if (*p == '\t') {
			*p = ' ';
		}
	}
	return out->buf;
}

static int get_sysinfo(struct platform *p, struct system_info *sysinfo)
{
	struct platform_arm64 *platform = to_platform_arm64(p);
	struct process_stdout *out1, *out2;

	/* Use dmidecode to get sysinfo type and identifier. */

	process_get_stdout(NULL, &out1, pb_system_apps.dmidecode,
		"--string=system-manufacturer",
		NULL);
	process_get_stdout(NULL, &out2, pb_system_apps.dmidecode,
		"--string=system-product-name",
		NULL);

	if (out1 || out2)
		sysinfo->type = talloc_asprintf(sysinfo, "%s %s",
			out1 ? stdout_cleaner(out1) : "Unknown",
			out2 ? stdout_cleaner(out2) : "Unknown");
	talloc_free(out1);
	talloc_free(out2);

	process_get_stdout(NULL, &out1, pb_system_apps.dmidecode,
		"--string=system-uuid",
		NULL);

	if (out1)
		sysinfo->identifier = talloc_asprintf(sysinfo, "%s",
			stdout_cleaner(out1));
	talloc_free(out1);

	sysinfo->bmc_mac = talloc_zero_size(sysinfo, HWADDR_SIZE);

	if (platform->ipmi) {
		ipmi_get_bmc_mac(platform->ipmi, sysinfo->bmc_mac);
		ipmi_get_bmc_versions(platform->ipmi, sysinfo);
	}

	pb_debug_fn("type:       '%s'\n", sysinfo->type);
	pb_debug_fn("identifier: '%s'\n", sysinfo->identifier);
	pb_debug_fn("bmc_mac:    '%s'\n", sysinfo->bmc_mac);

	return 0;
}

static void params_update_all(struct param_list *pl,
	const struct config *config, const struct config *defaults)
{
	char *tmp = NULL;
	const char *val;

	if (config->autoboot_enabled == defaults->autoboot_enabled)
		val = "";
	else
		val = config->autoboot_enabled ? "true" : "false";

	param_list_set_non_empty(pl, "auto-boot?", val, true);

	if (config->autoboot_timeout_sec == defaults->autoboot_timeout_sec)
		val = "";
	else
		val = tmp = talloc_asprintf(pl, "%d",
			config->autoboot_timeout_sec);

	param_list_set_non_empty(pl, "petitboot,timeout", val, true);
	if (tmp)
		talloc_free(tmp);

	val = config->lang ?: "";
	param_list_set_non_empty(pl, "petitboot,language", val, true);

	if (config->allow_writes == defaults->allow_writes)
		val = "";
	else
		val = config->allow_writes ? "true" : "false";
	param_list_set_non_empty(pl, "petitboot,write?", val, true);

	if (!config->manual_console) {
		val = config->boot_console ?: "";
		param_list_set_non_empty(pl, "petitboot,console", val, true);
	}

	val = config->http_proxy ?: "";
	param_list_set_non_empty(pl, "petitboot,http_proxy", val, true);
	val = config->https_proxy ?: "";
	param_list_set_non_empty(pl, "petitboot,https_proxy", val, true);

	params_update_network_values(pl, "petitboot,network", config);
	params_update_bootdev_values(pl, "petitboot,bootdevs", config);
}

static int save_config(struct platform *p, struct config *config)
{
	const struct efi_mount *efi_mount = to_platform_arm64(p)->efi_mount;
	struct param_list *pl = to_platform_arm64(p)->params;
	struct config *defaults;

	defaults = talloc_zero(NULL, struct config);
	config_set_defaults(defaults);

	params_update_all(pl, config, defaults);
	talloc_free(defaults);

	write_efivars(efi_mount, pl);
	
	return 0;
}

static bool probe(struct platform *p, void *ctx)
{
	static const struct efi_mount efi_mount = {
		.path = "/sys/firmware/efi/efivars",
		.guid = "fb78ab4b-bd43-41a0-99a2-4e74bef9169b",
	};
	struct platform_arm64 *platform;

	platform = talloc_zero(ctx, struct platform_arm64);
	platform->params = talloc_zero(platform, struct param_list);
	param_list_init(platform->params, common_known_params());

	if (efi_check_mount(&efi_mount))
		platform->efi_mount = &efi_mount;

	p->platform_data = platform;

	return true;
}

static struct platform platform_arm64 = {
	.name			= "arm64",
	.dhcp_arch_id		= 0x000d,
	.probe			= probe,
	.load_config		= load_config,
	.save_config		= save_config,
	.get_sysinfo		= get_sysinfo,
};

register_platform(platform_arm64);
