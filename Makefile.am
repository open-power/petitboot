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

SUBDIRS = lib discover test ui utils man

ACLOCAL_AMFLAGS = -I m4

AM_CPPFLAGS = $(DEFAULT_CPPFLAGS)
AM_CFLAGS = $(DEFAULT_CFLAGS)

LIBTOOL_DEPS = @LIBTOOL_DEPS@

libtool: $(LIBTOOL_DEPS)
	$(SHELL) ./config.status libtool

EXTRA_DIST = bootstrap configure.ac.in $(srcdir)/m4

MAINTAINERCLEANFILES = aclocal.m4 config.* configure configure.ac depcomp \
	install-sh ltmain.sh Makefile.in missing ylwrap $(PACKAGE)-*.gz

maintainer-clean-local:
	rm -rf m4
