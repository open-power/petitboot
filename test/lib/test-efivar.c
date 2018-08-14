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

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/types.h>

#include "efi/efivar.h"
#include "log/log.h"
#include "talloc/talloc.h"

static const struct efi_mount efi_mount = {
	.path = "/tmp/pb-test-efivar",
	.guid = "c9c07add-256e-4452-b911-f8d0d35a1ac7",
};
static const char v_name[] = "petitboot-test-one";
static const char v_data[] = "petitboot-efi-tester";

void finish(int code)
{
	efi_del_variable(&efi_mount, v_name);
	rmdir(efi_mount.path);
	exit(code);
}

int main(void)
{
	struct efi_data *efi_data;
	int rc;

	__pb_log_init(stderr, true);

	rc = mkdir(efi_mount.path, 0755);
	if (rc) {
		if (errno == EEXIST)
			pb_log("mkdir exists\n");
		else {
			pb_log("mkdir failed: (%d) %s\n", errno,
			       strerror(errno));
			finish(__LINE__);
		}
	}

	if (!efi_check_mount_magic(&efi_mount, false))
		finish(__LINE__);

	efi_data = talloc_zero(NULL, struct efi_data);
	efi_data->attributes = EFI_DEFALT_ATTRIBUTES;
	efi_data->data = talloc_strdup(efi_data, v_data);
	efi_data->data_size = sizeof(v_data);

	if (efi_set_variable(&efi_mount, v_name, efi_data))
		finish(__LINE__);

	talloc_free(efi_data);

	if (efi_get_variable(NULL, &efi_mount, v_name, &efi_data))
		finish(__LINE__);

	if (!efi_data->data) {
		pb_log("No efi_data->data\n");
		finish(__LINE__);
	}

	if (strcmp((char *)efi_data->data, v_data)) {
		pb_log("Bad efi_data->data: '%s' != '%s'\n",
		       (char *)efi_data->data, v_data);
		finish(__LINE__);
	}

	if (efi_del_variable(&efi_mount, v_name))
		finish(__LINE__);

	/* Get after delete should fail. */
	if (!efi_get_variable(NULL, &efi_mount, v_name, &efi_data))
		finish(__LINE__);

	finish(EXIT_SUCCESS);
}
