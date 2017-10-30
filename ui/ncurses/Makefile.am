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

ui_ncurses_common_libs = \
	ui/ncurses/libpbnc.la \
	ui/common/libpbui.la \
	$(core_lib) \
	@MENU_LIB@ @FORM_LIB@ @CURSES_LIB@

noinst_LTLIBRARIES += ui/ncurses/libpbnc.la

ui_ncurses_libpbnc_la_SOURCES = \
	ui/ncurses/nc-config.c \
	ui/ncurses/nc-config.h \
	ui/ncurses/nc-config-help.c \
	ui/ncurses/console-codes.c \
	ui/ncurses/console-codes.h \
	ui/ncurses/nc-cui.c \
	ui/ncurses/nc-cui.h \
	ui/ncurses/nc-cui-help.c \
	ui/ncurses/nc-boot-editor.c \
	ui/ncurses/nc-boot-editor.h \
	ui/ncurses/nc-boot-editor-help.c \
	ui/ncurses/nc-helpscreen.c \
	ui/ncurses/nc-helpscreen.h \
	ui/ncurses/nc-lang.c \
	ui/ncurses/nc-lang.h \
	ui/ncurses/nc-menu.c \
	ui/ncurses/nc-menu.h \
	ui/ncurses/nc-scr.c \
	ui/ncurses/nc-scr.h \
	ui/ncurses/nc-sysinfo.c \
	ui/ncurses/nc-sysinfo.h \
	ui/ncurses/nc-sysinfo-help.c \
	ui/ncurses/nc-textscreen.c \
	ui/ncurses/nc-textscreen.h \
	ui/ncurses/nc-widgets.c \
	ui/ncurses/nc-widgets.h \
	ui/ncurses/nc-add-url.c \
	ui/ncurses/nc-add-url.h \
	ui/ncurses/nc-add-url-help.c \
	ui/ncurses/nc-subset.c \
	ui/ncurses/nc-subset.h \
	ui/ncurses/nc-statuslog.c \
	ui/ncurses/nc-statuslog.h \
	ui/ncurses/nc-plugin.c \
	ui/ncurses/nc-plugin.h \
	ui/ncurses/nc-plugin-help.c \
	ui/ncurses/nc-plugin-menu-help.c

sbin_PROGRAMS += ui/ncurses/petitboot-nc

if ENABLE_PS3
sbin_PROGRAMS += ui/ncurses/petitboot-ps3-nc
endif

ui_ncurses_petitboot_nc_CPPFLAGS = \
	$(DEFAULT_CPPFLAGS) \
	-DLOCALEDIR='"$(localedir)"' \
	-I$(top_srcdir)/lib

ui_ncurses_petitboot_nc_SOURCES = ui/ncurses/generic-main.c
ui_ncurses_petitboot_nc_LDADD = $(ui_ncurses_common_libs)

ui_ncurses_petitboot_ps3_nc_SOURCES = ui/ncurses/ps3-main.c
ui_ncurses_petitboot_ps3_nc_LDADD = $(ui_ncurses_common_libs) \
				    $(libdir)/libps3-utils.so

EXTRA_DIST += ui/ncurses/ui-guidelines.text
