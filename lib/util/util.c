/*
 *  Copyright (C) 2013 IBM Corporation
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

#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <util/util.h>

void mac_str(uint8_t *mac, unsigned int maclen, char *buf, unsigned int buflen)
{
	unsigned int i;
	char *pos;

	assert(buflen > sizeof("unknown"));

	if (!maclen || maclen * 3 + 1 > buflen) {
		strcpy(buf, "unknown");
		return;
	}

	pos = buf;

	for (i = 0; i < maclen; i++) {
		snprintf(pos, 4, "%02x:", mac[i]);
		pos += 3;
	}

	*(pos - 1) = '\0';

	return;
}
