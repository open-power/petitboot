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

noinst_LTLIBRARIES += ui/common/libpbui.la

ui_common_libpbui_la_SOURCES = \
	ui/common/discover-client.c \
	ui/common/discover-client.h \
	ui/common/joystick.c \
	ui/common/joystick.h \
	ui/common/timer.c \
	ui/common/timer.h \
	ui/common/ui-system.c \
	ui/common/ui-system.h

if ENABLE_PS3
ui_common_libpbui_la_SOURCES += \
	ui/common/ps3.c \
	ui/common/ps3.h
endif

artwork =

if WITH_TWIN
artwork += \
	$(srcdir)/ui/common/artwork/background.jpg \
	$(srcdir)/ui/common/artwork/COPYING-tux \
	$(srcdir)/ui/common/artwork/tux.png \
	$(srcdir)/ui/common/artwork/oxygen/applications-system.png \
	$(srcdir)/ui/common/artwork/oxygen/configure.png \
	$(srcdir)/ui/common/artwork/oxygen/COPYING-oxygen \
	$(srcdir)/ui/common/artwork/oxygen/drive-harddisk.png \
	$(srcdir)/ui/common/artwork/oxygen/network-wired.png \
	$(srcdir)/ui/common/artwork/oxygen/utilities-terminal.png
endif

artworkdir = $(pkgdatadir)/artwork/
dist_artwork_DATA = $(artwork)

