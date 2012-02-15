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
	$(DEFAULT_CPPFLAGS)

AM_CFLAGS = \
	$(DEFAULT_CFLAGS)

AM_LDFLAGS = -lmenu -lform -lncurses

common_libs = \
	libpbnc.la \
	$(top_builddir)/ui/common/libpbui.la \
	$(top_builddir)/lib/libpbcore.la

noinst_LTLIBRARIES = libpbnc.la

libpbnc_la_SOURCES = \
	nc-cui.c \
	nc-cui.h \
	nc-ked.c \
	nc-ked.h \
	nc-menu.c \
	nc-menu.h \
	nc-scr.c \
	nc-scr.h

bin_PROGRAMS = pb-nc

if ENABLE_PS3
bin_PROGRAMS += pb-nc-ps3
endif


pb_nc_SOURCES = generic-main.c
pb_nc_LDADD = $(common_libs)

pb_nc_ps3_SOURCES = ps3-main.c
pb_nc_ps3_LDADD = $(common_libs) $(libdir)/libps3-utils.so
#pb_nc_ps3_LDFLAGS = -L$(libdir) -lps3-utils


MAINTAINERCLEANFILES = Makefile.in
