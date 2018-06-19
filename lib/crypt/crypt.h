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
#ifndef CRYPT_H
#define CRYPT_H

#include "config.h"

#ifdef CRYPT_SUPPORT

char *crypt_get_hash(void *ctx);
bool crypt_check_password(const char *password);
int crypt_set_password(void *ctx, const char *password);
int crypt_set_password_hash(void *ctx, const char *hash);

#else

static inline char *crypt_get_hash(void *ctx __attribute__((unused)))
{
	return NULL;
}
static inline bool crypt_check_password(
		const char *password __attribute__((unused)))
{
	return false;
}
static inline int crypt_set_password(void *ctx __attribute__((unused)),
		const char *password __attribute__((unused)))
{
	return -1;
}
static inline int crypt_set_password_hash(void *ctx __attribute__((unused)),
		const char *hash __attribute__((unused)))
{
	return -1;
}

#endif
#endif /* CRYPT_H */
