## configure.ac -- Process this file with autoconf to produce configure
#
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

AC_INIT([petitboot], [0.2], [Jeremy Kerr <jk@ozlabs.org>])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile])

default_CFLAGS="--std=gnu99 -O2 -g -Wall -W -Wunused -Wstrict-prototypes \
-Wmissing-prototypes -Wmissing-declarations -Wredundant-decls -Winline"

if test "x$CFLAGS" = "x"; then
    CFLAGS="$default_CFLAGS"
fi

AC_PROG_CC
AC_PROG_INSTALL

PKG_CHECK_MODULES([twin], [libtwin])

mkdir -p discover ui/test ui/common lib/talloc lib/pb-protocol lib/list \
	      lib/waiter lib/log

AC_OUTPUT
