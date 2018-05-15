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

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

#include <url/url.h>

#include "security.h"

int lockdown_status(void) { return PB_LOCKDOWN_NONE; }

struct pb_url * get_signature_url(void *ctx __attribute__((unused)),
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

int decrypt_file(const char * filename __attribute__((unused)),
    FILE * authorized_signatures_handle __attribute__((unused)),
    const char * keyring_path __attribute__((unused)))
{
	return -1;
}

int validate_boot_files(struct boot_task *boot_task __attribute__((unused)))
{
	return 0;
}

void validate_boot_files_cleanup(struct boot_task *boot_task __attribute__((unused)))
{}

