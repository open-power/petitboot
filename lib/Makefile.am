#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; version 2 of the License.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
#

AM_CPPFLAGS = -I$(top_srcdir)/lib $(DEFAULT_CPPFLAGS) \
	-DPREFIX='"$(prefix)"'

AM_CFLAGS = $(DEFAULT_CFLAGS)

noinst_LTLIBRARIES = libpbcore.la

libpbcore_la_SOURCES = \
	log/log.h \
	log/log.c \
	list/list.c \
	list/list.h \
	waiter/waiter.c \
	waiter/waiter.h \
	pb-protocol/pb-protocol.c \
	pb-protocol/pb-protocol.h \
	pb-config/pb-config.c \
	pb-config/pb-config.h \
	pb-config/storage.h \
	pb-config/storage-powerpc-nvram.c \
	pb-config/storage-null.c \
	pb-config/storage-test.c \
	process/process.c \
	process/process.h \
	types/types.h \
	talloc/talloc.c \
	talloc/talloc.h \
	system/system.c \
	system/system.h \
	url/url.c \
	url/url.h \
	util/util.h

MAINTAINERCLEANFILES = Makefile.in
