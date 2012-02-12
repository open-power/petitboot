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

noinst_LTLIBRARIES = libpbui.la

libpbui_la_SOURCES = \
	discover-client.c \
	discover-client.h \
	joystick.c \
	joystick.h \
	loader.c \
	loader.h \
	timer.c \
	timer.h \
	ui-system.c \
	ui-system.h \
	url.c \
	url.h

if ENABLE_PS3
libpbui_la_SOURCES += \
	ps3.c \
	ps3.h
endif

artwork =

if WITH_TWIN
artwork += \
	artwork/background.jpg \
	artwork/cdrom.png \
	artwork/cursor.gz \
	artwork/hdd.png \
	artwork/network.png \
	artwork/system.png \
	artwork/tux.png \
	artwork/usbpen.png
endif

artworkdir = $(pkgdatadir)/artwork/
dist_artwork_DATA = $(artwork)

MAINTAINERCLEANFILES = Makefile.in
