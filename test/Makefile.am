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

common_libs = \
	$(top_builddir)/lib/libpbcore.la \
	$(top_builddir)/discover/libparser.la

noinst_SCRIPTS = parser-test.sh hotplug-device.sh

noinst_PROGRAMS = parser-test

parser_test_SOURCES = parser-test.c
parser_test_LDADD = $(common_libs)

MAINTAINERCLEANFILES = Makefile.in
