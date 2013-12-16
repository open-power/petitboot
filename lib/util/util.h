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

#ifndef UTIL_H
#define UTIL_H

#include <stdint.h>

#ifndef container_of
#define container_of(_ptr, _type, _member) ({ \
	const typeof( ((_type *)0)->_member ) *__mptr = (_ptr); \
	(_type *)( (char *)__mptr - offsetof(_type,_member) );})
#endif

#ifndef offsetof
#define offsetof(_type, _member) ((size_t) &((_type *)0)->_member)
#endif

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

#define min(a,b) ({					\
		typeof(a) _min_a = (a);			\
		typeof(b) _min_b = (b);			\
		(void)(&_min_a == &_min_b);		\
		_min_a < _min_b ? _min_a : _min_b;	\
		})

#define max(a,b) ({					\
		typeof(a) _max_a = (a);			\
		typeof(b) _max_b = (b);			\
		(void)(&_max_a == &_max_b);		\
		_max_a > _max_b ? _max_a : _max_b;	\
		})

#define build_assert(x) \
	do { (void)sizeof(char[(x)?1:-1]); } while (0)

void mac_str(uint8_t *mac, unsigned int maclen, char *buf, unsigned int buflen);

#endif /* UTIL_H */

