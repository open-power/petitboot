/*
 *  Copyright (C) 2018 IBM Corporation
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
 */
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <shadow.h>
#include <crypt.h>
#include <errno.h>

#include <talloc/talloc.h>
#include <log/log.h>

#include "crypt.h"

int crypt_set_password_hash(void *ctx, const char *hash)
{
	struct spwd **shadow, *entry;
	bool found_root;
	int rc, i, n;
	FILE *fp;

	if (lckpwdf()) {
		pb_log("Could not obtain access to shadow file\n");
		return -1;
	}
	setspent();

	found_root = false;
	shadow = NULL;
	n = 0;

	/* Read all entries and modify the root entry */
	errno = 0;
	fp = fopen("/etc/shadow", "r");
	if (!fp) {
		pb_log("Could not open shadow file, %m\n");
		rc = -1;
		goto out;
	}

	entry = fgetspent(fp);
	while (entry) {
		shadow = talloc_realloc(ctx, shadow, struct spwd *, n + 1);
		if (!shadow) {
			pb_log("Failed to allocate shadow struct\n");
			rc = -1;
			goto out;
		}

		shadow[n] = talloc_memdup(shadow, entry, sizeof(struct spwd));
		if (!shadow[n]) {
			pb_log("Could not duplicate entry for %s\n",
					entry->sp_namp);
			rc = -1;
			goto out;
		}

		shadow[n]->sp_namp = talloc_strdup(shadow, entry->sp_namp);
		if (strncmp(shadow[n]->sp_namp, "root", strlen("root")) == 0) {
			shadow[n]->sp_pwdp = talloc_strdup(shadow, hash);
			found_root = true;
		} else {
			shadow[n]->sp_pwdp = talloc_strdup(shadow,
					entry->sp_pwdp);
		}

		if (!shadow[n]->sp_namp || !shadow[n]->sp_pwdp) {
			pb_log("Failed to allocate new fields for %s\n",
					entry->sp_namp);
			rc = -1;
			goto out;
		}

		n++;
		entry = fgetspent(fp);
	}

	if (n == 0)
		pb_debug_fn("No entries found\n");

	fclose(fp);

	if (!found_root) {
		/* Make our own */
		pb_debug_fn("No root user found, creating entry\n");
		shadow = talloc_realloc(ctx, shadow, struct spwd *, n + 1);
		if (!shadow) {
			pb_log("Failed to allocate shadow struct\n");
			rc = -1;
			goto out;
		}

		shadow[n] = talloc_zero(shadow, struct spwd);
		shadow[n]->sp_namp = talloc_asprintf(shadow, "root");
		shadow[n]->sp_pwdp = talloc_strdup(shadow, hash);
		if (!shadow[n]->sp_namp || !shadow[n]->sp_pwdp) {
			pb_log("Failed to allocate new fields for root entry\n");
			rc = -1;
			goto out;
		}
		n++;
	}

	errno = 0;
	fp = fopen("/etc/shadow", "w");
	if (!fp) {
		pb_log("Could not open shadow file, %m\n");
		rc = -1;
		goto out;
	}

	/* Write each entry back to keep the same format in /etc/shadow */
	for (i = 0; i < n; i++) {
		rc = putspent(shadow[i], fp);
		if (rc)
			pb_log("Failed to write back shadow entry for %s!\n",
					shadow[i]->sp_namp);
	}

	rc = 0;
out:
	if (fp)
		fclose(fp);
	talloc_free(shadow);
	endspent();
	ulckpwdf();
	return rc;
}

static const char *crypt_hash_password(const char *password)
{
	struct spwd *shadow;
	char *hash, *salt;
	char new_salt[17];
	int i;

	shadow = getspnam("root");
	if (!shadow) {
		pb_log("Could not find root shadow\n");
		return NULL;
	}

	if (shadow->sp_pwdp && strlen(shadow->sp_pwdp)) {
		salt = shadow->sp_pwdp;
	} else {
		for (i = 0; i < 16; i++)
			new_salt[i] = random() % 94 + 32;
		new_salt[i] = '\0';
		salt = talloc_asprintf(password, "$6$%s", new_salt);
	}

	hash = crypt(password ?: "", salt);
	if (!hash)
		pb_log("Could not create hash, %m\n");


	return hash;
}


int crypt_set_password(void *ctx, const char *password)
{
	const char *hash;

	if (!password || !strlen(password))
		return crypt_set_password_hash(ctx, "");

	hash = crypt_hash_password(password);
	if (!hash)
		return -1;

	return crypt_set_password_hash(ctx, hash);
}

char *crypt_get_hash(void *ctx)
{
	struct spwd *shadow;

	shadow = getspnam("root");
	if (!shadow) {
		pb_log("Could not find root shadow\n");
		return false;
	}

	return talloc_strdup(ctx, shadow->sp_pwdp);
}

bool crypt_check_password(const char *password)
{
	struct spwd *shadow;
	char *hash;

	shadow = getspnam("root");
	if (!shadow) {
		pb_log("Could not find root shadow\n");
		return false;
	}

	hash = crypt(password ? : "", shadow->sp_pwdp);
	if (!hash) {
		pb_log("Could not create hash, %m\n");
		return false;
	}

	return strncmp(shadow->sp_pwdp, hash, strlen(shadow->sp_pwdp)) == 0;
}
