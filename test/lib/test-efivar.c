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
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <unistd.h>

#include "efi/efivar.h"
#include "talloc/talloc.h"

#define DEF_ATTR	(EFI_VARIABLE_NON_VOLATILE | \
	EFI_VARIABLE_RUNTIME_ACCESS | EFI_VARIABLE_BOOTSERVICE_ACCESS)

static const char *test_efivar_guid = "c9c07add-256e-4452-b911-f8d0d35a1ac7";
static const char *test_varname = "efivartest";
static const char *test_data = "petitboot";

static char* find_efitest_path(void)
{
	static char dir[PATH_MAX] = {0};
	static bool run = false;
	char *rest_path = "/efivarfs_data/";
	char *pos = NULL;

	if (run)
		return dir;

	readlink("/proc/self/exe", dir, PATH_MAX);

	pos = strrchr(dir, '/');
	*pos = '\0';

	strcat(dir, rest_path);
	run = true;

	return dir;
}

static bool probe(void)
{
	char *path;
	int rc;

	path = find_efitest_path();

	rc = access(path, F_OK);
	if (rc) {
		if (errno == ENOENT) {
			rc = mkdir(path, 0755);
			if(rc)
				return false;
		} else {
			return false;
		}
	}

	set_efivarfs_path(path);

	return true;
}

int main(void)
{
	void *ctx = NULL;
	int rc, errno_value;
	uint32_t attr = DEF_ATTR;
	char *path = NULL;
	struct efi_data *efi_data;

	if(!probe())
		return ENOENT;

	talloc_new(ctx);

	efi_data = talloc_zero(ctx, struct efi_data);
	efi_data->attributes = attr;
	efi_data->data = talloc_strdup(efi_data, test_data);
	efi_data->data_size = strlen(test_data) + 1;

	rc = efi_set_variable(test_efivar_guid, test_varname,
				efi_data);

	talloc_free(efi_data);
	rc = efi_get_variable(ctx, test_efivar_guid, test_varname,
				&efi_data);

	assert(efi_data->data != NULL);
	rc = strcmp((char *)efi_data->data, test_data);
	if (rc) {
		talloc_free(ctx);
		assert(0);
	}

	rc = efi_del_variable(test_efivar_guid, test_varname);

	rc = efi_get_variable(ctx, test_efivar_guid, test_varname,
				&efi_data);

	errno_value = errno;
	talloc_free(ctx);

	assert(errno_value == ENOENT);

	path = find_efitest_path();
	rmdir(path);

	return EXIT_SUCCESS;
}
