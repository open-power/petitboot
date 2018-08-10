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
#ifndef EFIVAR_H
#define EFIVAR_H

#include <linux/magic.h>
#include <stdint.h>

#define EFI_VARIABLE_NON_VOLATILE                           0x00000001
#define EFI_VARIABLE_BOOTSERVICE_ACCESS                     0x00000002
#define EFI_VARIABLE_RUNTIME_ACCESS                         0x00000004
#define EFI_VARIABLE_HARDWARE_ERROR_RECORD                  0x00000008
#define EFI_VARIABLE_AUTHENTICATED_WRITE_ACCESS             0x00000010
#define EFI_VARIABLE_TIME_BASED_AUTHENTICATED_WRITE_ACCESS  0x00000020
#define EFI_VARIABLE_APPEND_WRITE                           0x00000040

#define EFI_DEFALT_ATTRIBUTES ( \
	EFI_VARIABLE_NON_VOLATILE | \
	EFI_VARIABLE_RUNTIME_ACCESS | \
	EFI_VARIABLE_BOOTSERVICE_ACCESS \
)

#ifndef EFIVARFS_MAGIC
#define EFIVARFS_MAGIC 0xde5e81e4
#endif

struct efi_data {
	uint32_t attributes;
	size_t data_size;
	void *data;
	uint8_t fill[0];
};

void set_efivarfs_path(const char *path);
const char *get_efivarfs_path(void);

int efi_get_variable(void *ctx, const char *guidstr, const char *name,
		struct efi_data **efi_data);
int efi_set_variable(const char *guidstr, const char *name,
		const struct efi_data *efi_data);
int efi_del_variable(const char *guidstr, const char *name);

#endif /* EFIVAR_H */
