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
	$(srcdir)/artwork/background.jpg \
	$(srcdir)/artwork/COPYING-tux \
	$(srcdir)/artwork/tux.png \
	$(srcdir)/artwork/oxygen/applications-system.png \
	$(srcdir)/artwork/oxygen/configure.png \
	$(srcdir)/artwork/oxygen/COPYING-oxygen \
	$(srcdir)/artwork/oxygen/drive-harddisk.png \
	$(srcdir)/artwork/oxygen/drive-optical.png \
	$(srcdir)/artwork/oxygen/drive-removable-media-usb-pendrive.png \
	$(srcdir)/artwork/oxygen/media-optical.png \
	$(srcdir)/artwork/oxygen/network-wired.png \
	$(srcdir)/artwork/oxygen/utilities-terminal.png
endif

artworkdir = $(pkgdatadir)/artwork/
dist_artwork_DATA = $(artwork)

MAINTAINERCLEANFILES = Makefile.in
