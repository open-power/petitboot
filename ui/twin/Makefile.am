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

ui_twin_common_libs = \
	ui/twin/libpbt.la \
	ui/common/libpbui.la \
	$(core_lib)

ui_twin_common_cppflags = \
	$(DEFAULT_CPPFLAGS) \
	-I$(top_srcdir)/lib \
	-DPB_ARTWORK_PATH='"$(pkgdatadir)/artwork/"'

noinst_LTLIBRARIES += ui/twin/libpbt.la

ui_twin_libpbt_la_SOURCES = \
	ui/twin/pbt-client.c \
	ui/twin/pbt-client.h \
	ui/twin/pbt-main.c \
	ui/twin/pbt-main.h \
	ui/twin/pbt-menu.c \
	ui/twin/pbt-menu.h \
	ui/twin/pbt-scr.c \
	ui/twin/pbt-scr.h

ui_twin_libpbt_la_CPPFLAGS = $(ui_twin_common_cppflags)

sbin_PROGRAMS += ui/twin/petitboot-twin

if PLATFORM_PS3
sbin_PROGRAMS += ui/twin/petitboot-ps3-twin
endif


ui_twin_petitboot_twin_SOURCES = ui/twin/main-generic.c
ui_twin_petitboot_twin_CPPFLAGS = $(ui_twin_common_cppflags)
ui_twin_petitboot_twin_LDADD = $(ui_twin_common_libs) $(twin_LIBS)

ui_twin_petitboot_ps3_twin_SOURCES = ui/twin/main-ps3.c
ui_twin_petitboot_ps3_twin_CPPFLAGS = $(ui_twin_common_cppflags)
ui_twin_petitboot_ps3_twin_LDADD = $(ui_twin_common_libs) $(twin_LIBS) -lps3-utils

