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

AUTOMAKE_OPTIONS = foreign subdir-objects parallel-tests

SUBDIRS = po

ACLOCAL_AMFLAGS = -I m4

AM_CPPFLAGS = $(DEFAULT_CPPFLAGS) -I$(top_srcdir)/lib
AM_CFLAGS = $(DEFAULT_CFLAGS)
AM_YFLAGS = -d

LIBTOOL_DEPS = @LIBTOOL_DEPS@

pkgsysconfdir = @sysconfdir@/@PACKAGE@

libtool: $(LIBTOOL_DEPS)
	$(SHELL) ./config.status libtool

EXTRA_DIST = version.sh configure.ac $(srcdir)/m4 config.rpath

MAINTAINERCLEANFILES = aclocal.m4 config.* configure configure.ac depcomp \
	install-sh ltmain.sh Makefile.in missing ylwrap $(PACKAGE)-*.gz

maintainer-clean-local:
	rm -rf m4

noinst_LTLIBRARIES =
noinst_PROGRAMS =
sbin_PROGRAMS =
dist_sbin_SCRIPTS =
check_PROGRAMS =
check_DATA =
check_SCRIPTS =
TESTS =
BUILT_SOURCES =
CLEANFILES =

include lib/Makefile.am
include discover/grub2/Makefile.am
include discover/Makefile.am
include test/Makefile.am
include test/lib/Makefile.am
include test/parser/Makefile.am
include test/urls/Makefile.am
include ui/common/Makefile.am

if WITH_NCURSES
include ui/ncurses/Makefile.am
endif

if WITH_TWIN
include ui/twin/Makefile.am
endif

include ui/test/Makefile.am

include man/Makefile.am

include utils/Makefile.am

