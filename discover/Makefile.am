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

AM_CPPFLAGS = -I$(top_srcdir)/lib $(DEFAULT_CPPFLAGS)

AM_CFLAGS = $(DEFAULT_CFLAGS)  \
	-DPREFIX='"$(prefix)"' \
	-DPKG_SHARE_DIR='"$(pkgdatadir)"' \
	-DLOCAL_STATE_DIR='"$(localstatedir)"'

noinst_LTLIBRARIES = libparser.la

libparser_la_SOURCES = \
	parser.c \
	parser.h \
	parser-conf.c \
	parser-conf.h \
	parser-utils.c \
	parser-utils.h \
	paths.c \
	paths.h \
	kboot-parser.c \
	grub2-parser.c \
	yaboot-parser.c

EXTRA_DIST = native-parser.c

libparser_la_LIBADD = $(top_builddir)/lib/libpbcore.la

sbin_PROGRAMS = pb-discover

pb_discover_SOURCES = \
	boot.c \
	boot.h \
	device-handler.c \
	device-handler.h \
	discover-server.c \
	discover-server.h \
	event.c \
	event.h \
	event-parser.c \
	params.c \
	params.h \
	pb-discover.c \
	pb-discover.h \
	resource.h \
	udev.c \
	udev.h \
	user-event.c \
	user-event.h

pb_discover_LDADD = libparser.la $(top_builddir)/lib/libpbcore.la

MAINTAINERCLEANFILES = Makefile.in
