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

AC_INIT([petitboot], [0.3], [Geoff Levand <geoff@infradead.org>])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile])

default_CFLAGS="--std=gnu99 -O2 -g -Wall -W -Wunused -Wstrict-prototypes \
-Wmissing-prototypes -Wmissing-declarations -Wredundant-decls -Winline"

if test "x$CFLAGS" = "x"; then
    CFLAGS="$default_CFLAGS"
fi

AC_PROG_CC
AC_PROG_INSTALL

AC_CHECK_HEADERS([stdarg.h])
AC_CHECK_HEADERS([varargs.h])
#AC_CHECK_FUNCS([va_copy])

dnl Checking for va_copy availability
AC_MSG_CHECKING([for va_copy])
AC_TRY_LINK([#include <stdarg.h>
va_list ap1,ap2;], [va_copy(ap1,ap2);],
have_va_copy=yes,
have_va_copy=no)
AC_MSG_RESULT($have_va_copy)
if test x"$have_va_copy" = x"yes"; then
    AC_DEFINE(HAVE_VA_COPY,1,[Whether va_copy() is available])
else
    AC_MSG_CHECKING([for __va_copy])
    AC_TRY_LINK([#include <stdarg.h>
    va_list ap1,ap2;], [__va_copy(ap1,ap2);],
    have___va_copy=yes,
    have___va_copy=no)
    AC_MSG_RESULT($have___va_copy)
    if test x"$have___va_copy" = x"yes"; then
        AC_DEFINE(HAVE___VA_COPY,1,[Whether __va_copy() is available])
    fi
fi

AC_ARG_ENABLE([ps3],
	[AS_HELP_STRING([--enable-ps3],
		[build support for the PS3 game console])],
	[],
	[enable_ps3=check])

AS_IF([test "x$enable_ps3" != xno], [AC_SUBST([ENABLE_PS3], ["y"])], [])

AC_ARG_WITH([twin],
	[AS_HELP_STRING([--with-twin],
		[build GUI versions using the twin window system])],
	[],
	[with_twin=check])

PBTWIN=
LIBTWIN=
AS_IF([test "x$with_twin" != xno],
	[PKG_CHECK_MODULES([twin], [libtwin], [],
		[AC_MSG_RESULT([$twin_PKG_ERRORS])
		AC_MSG_RESULT([Consider adjusting PKG_CONFIG_PATH environment variable])])
	AC_CHECK_LIB([twin], [main],
		[AC_SUBST([LIBTWIN], ["-ltwin"])
			AC_SUBST([PBTWIN], ["y"])
			AC_DEFINE([HAVE_LIBTWIN], [1], [Define if you have libtwin])],
		[if test "x$with_twin" != xcheck; then
			AC_MSG_FAILURE([--with-twin was given, but test for twin failed])
		fi],
		[${twin_LIBS}])
	AC_CHECK_HEADERS([libtwin/twin_x11.h])
	AC_CHECK_HEADERS([libtwin/twin_fbdev.h])])

AC_ARG_ENABLE([x11],
	[AS_HELP_STRING([--enable-x11],
		[build for x11])],
	[],
	[enable_x11=check])

AS_IF([test "x$enable_x11" != xno], [AC_SUBST([ENABLE_X11], ["y"])], [])


mkdir -p discover lib/list lib/log lib/pb-protocol lib/system lib/talloc \
	lib/waiter test ui/common ui/ncurses ui/test ui/twin utils

AC_OUTPUT
