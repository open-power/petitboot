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

#define _XOPEN_SOURCE

#include <string.h>
#include <stdlib.h>
#include <wchar.h>

#include <i18n/i18n.h>

/* Return the number of columns required to display a localised string */
int strncols(const char *str)
{
	int wlen, ncols;
	wchar_t *wstr;

	wlen = mbstowcs(NULL, str, 0);
	if (wlen <= 0)
		return wlen;

	wstr = malloc(sizeof(wchar_t) * wlen + 1);
	if (!wstr)
		return -1;

	wlen = mbstowcs(wstr, str, wlen);
	if (wlen <= 0) {
		free(wstr);
		return wlen;
	}

	ncols = wcswidth(wstr, wlen);

	free(wstr);
	return ncols;
}
