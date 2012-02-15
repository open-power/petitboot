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

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/lib \
	-I$(includedir) \
	$(DEFAULT_CPPFLAGS) \
	$(twin_CFLAGS)

AM_CFLAGS = \
	-DPB_ARTWORK_PATH='"$(pkgdatadir)/artwork/"' \
	$(DEFAULT_CFLAGS)  \
	 $(twin_CFLAGS)

AM_LDFLAGS = \
	$(twin_LIBS)

common_libs = \
	libpbt.la \
	$(top_builddir)/ui/common/libpbui.la \
	$(top_builddir)/lib/libpbcore.la

noinst_LTLIBRARIES = libpbt.la

libpbt_la_SOURCES = \
	pbt-client.c \
	pbt-client.h \
	pbt-main.c \
	pbt-main.h \
	pbt-menu.c \
	pbt-menu.h \
	pbt-scr.c \
	pbt-scr.h

bin_PROGRAMS = pb-twin

if ENABLE_PS3
bin_PROGRAMS += pb-twin-ps3
endif


pb_twin_SOURCES = main-generic.c
pb_twin_LDADD = $(common_libs)

pb_twin_ps3_SOURCES = main-ps3.c
pb_twin_ps3_LDADD = $(common_libs)
pb_twin_ps3_LDFLAGS = -lps3-utils


MAINTAINERCLEANFILES = Makefile.in
