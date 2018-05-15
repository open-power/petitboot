/*
 *  Copyright (C) 2016 Raptor Engineering, LLC
 *  Copyright (C) 2018 Opengear, Inc
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
#ifndef _PB_SECURITY_H
#define _PB_SECURITY_H

#include <discover/boot.h>

enum {
	PB_LOCKDOWN_NONE	= 0,
	PB_LOCKDOWN_SIGN	= 1,
	PB_LOCKDOWN_DECRYPT	= 2
};


int lockdown_status(void);

struct pb_url * get_signature_url(void *ctx, struct pb_url *base_file);

int verify_file_signature(const char *plaintext_filename,
	const char *signature_filename, FILE *authorized_signatures_handle,
	const char *keyring_path);

int decrypt_file(const char *filename,
	FILE * authorized_signatures_handle, const char * keyring_path);

int validate_boot_files(struct boot_task *boot_task);

void validate_boot_files_cleanup(struct boot_task *boot_task);

#endif // _PB_SECURITY_H

