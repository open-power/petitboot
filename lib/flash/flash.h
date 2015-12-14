/*
 *  Copyright (C) 2015 IBM Corporation
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

#ifndef FLASH_H
#define FLASH_H

#include <flash/config.h>
#include <types/types.h>

int flash_parse_version(void *ctx, char ***versions, bool current);

#endif /* FLASH_H */
