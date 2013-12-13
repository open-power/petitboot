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
	$(DEFAULT_CPPFLAGS)

AM_CFLAGS = \
	$(DEFAULT_CFLAGS)

common_libs = \
	libpbnc.la \
	$(top_builddir)/ui/common/libpbui.la \
	$(top_builddir)/lib/libpbcore.la \
	@MENU_LIB@ @FORM_LIB@ @CURSES_LIB@

noinst_LTLIBRARIES = libpbnc.la

libpbnc_la_SOURCES = \
	nc-config.c \
	nc-config.h \
	nc-config-help.c \
	nc-cui.c \
	nc-cui.h \
	nc-boot-editor.c \
	nc-boot-editor.h \
	nc-helpscreen.c \
	nc-helpscreen.h \
	nc-menu.c \
	nc-menu.h \
	nc-scr.c \
	nc-scr.h \
	nc-sysinfo.c \
	nc-sysinfo.h \
	nc-sysinfo-help.c \
	nc-textscreen.c \
	nc-textscreen.h \
	nc-widgets.c \
	nc-widgets.h

sbin_PROGRAMS = petitboot-nc

if ENABLE_PS3
sbin_PROGRAMS += petitboot-ps3-nc
endif


petitboot_nc_SOURCES = generic-main.c generic-main-help.c
petitboot_nc_LDADD = $(common_libs)

petitboot_ps3_nc_SOURCES = ps3-main.c
petitboot_ps3_nc_LDADD = $(common_libs) $(libdir)/libps3-utils.so
#petitboot_ps3_nc_LDFLAGS = -L$(libdir) -lps3-utils


MAINTAINERCLEANFILES = Makefile.in
