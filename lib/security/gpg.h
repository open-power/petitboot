/*
 *  Copyright (C) 2016 Raptor Engineering, LLC
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

#ifndef _PB_GPG_H
#define _PB_GPG_H

#include <discover/boot.h>

enum {
	PB_LOCKDOWN_NONE	= 0,
	PB_LOCKDOWN_SIGN	= 1,
};

#if defined(HAVE_LIBGPGME)
#include <gpgme.h>
#endif /* HAVE_LIBGPGME */

int lockdown_status(void);

struct pb_url * gpg_get_signature_url(void *ctx, struct pb_url *base_file);

int verify_file_signature(const char *plaintext_filename,
	const char *signature_filename, FILE *authorized_signatures_handle,
	const char *keyring_path);

int gpg_validate_boot_files(struct boot_task *boot_task);

void gpg_validate_boot_files_cleanup(struct boot_task *boot_task);

#if !defined(HAVE_LIBGPGME)

int lockdown_status(void) { return PB_LOCKDOWN_NONE; }

struct pb_url * gpg_get_signature_url(void *ctx __attribute__((unused)),
			struct pb_url *base_file __attribute__((unused)))
{
	return NULL;
}

int verify_file_signature(const char *plaintext_filename __attribute__((unused)),
	const char *signature_filename __attribute__((unused)),
	FILE *authorized_signatures_handle __attribute__((unused)),
	const char *keyring_path __attribute__((unused)))
{
	return -1;
}

int gpg_validate_boot_files(struct boot_task *boot_task __attribute__((unused)))
{
	return 0;
}

void gpg_validate_boot_files_cleanup(struct boot_task *boot_task __attribute__((unused)))
{}

#endif /* HAVE_LIBGPGME */

#endif /* _PB_GPG_H */