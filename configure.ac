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

AC_INIT([petitboot],
    [m4_esyscmd_s([./version.sh])],
    [Geoff Levand <geoff@infradead.org>])

AC_CONFIG_MACRO_DIR([m4])

AC_PREFIX_DEFAULT([/usr/local])

AS_IF([test "x$CFLAGS" = "x"], [AC_SUBST([CFLAGS], [""])])

AC_PROG_CC
AC_PROG_LEX
AC_PROG_YACC
AC_PROG_INSTALL

AM_INIT_AUTOMAKE
AC_GNU_SOURCE
AM_GNU_GETTEXT([external])
AM_GNU_GETTEXT_VERSION(0.18.1)
LT_INIT

AM_SILENT_RULES([yes])

AX_WITH_CURSES
AX_WITH_CURSES_MENU
AX_WITH_CURSES_FORM

AM_CONDITIONAL([WITH_NCURSES], [test "x$ax_cv_curses" = "xyes"])

if test "x$ax_cv_curses" = xyes; then
	if test "x$ax_cv_menu" != xyes; then
		AC_MSG_ERROR([--with-ncurses requires the ncurses menu library])
	fi
	if test "x$ax_cv_form" != xyes; then
		AC_MSG_ERROR([--with-ncurses requires the ncurses form library])
	fi
fi

AC_CHECK_LIB([udev], [udev_new],
	[UDEV_LIBS=-ludev],
	[AC_MSG_FAILURE([The libudev development library is required by petitboot.  Try installing the package libudev-dev or libudev-devel.])]
)

PKG_CHECK_EXISTS(libudev >= 218, [old_udev=no], [old_udev=yes])
if test "$old_udev" = "yes" ; then
      AC_DEFINE(UDEV_LOGGING, 1, [Support old udev logging interface])
fi

AC_CHECK_LIB([devmapper], [dm_task_create],
	[DEVMAPPER_LIBS=-ldevmapper],
	[AC_MSG_FAILURE([The libdevmapper development library is required by petitboot.  Try installing the package libdevmapper-dev or device-mapper-devel.])]
)

AC_ARG_WITH([fdt],
	AS_HELP_STRING([--without-fdt],
		[Build without libfdt (default: no)]))

AS_IF([test "x$with_fdt" != "xno"],
	AC_CHECK_LIB([fdt], [fdt_check_header],
		[FDT_LIBS=-lfdt; have_libfdt=yes]))

AM_CONDITIONAL([HAVE_LIBFDT], [test x"$have_libfdt" = xyes])

AC_CHECK_HEADERS([stdarg.h])
AC_CHECK_HEADERS([varargs.h])

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

AC_ARG_WITH(
	[twin-x11],
	[AS_HELP_STRING([--with-twin-x11],
		[build x11 GUI programs using the twin window system [default=yes]]
	)],
	[],
	[with_twin_x11=yes]
)
AM_CONDITIONAL([WITH_TWIN_X11], [test "x$with_twin_x11" = "xyes"])

AC_ARG_WITH(
	[twin-fbdev],
	[AS_HELP_STRING(
		[--with-twin-fbdev],
		[build frame buffer GUI programs using the twin window system [default=no]]
	)],
	[],
	[with_twin_fbdev=yes]
)
AM_CONDITIONAL([WITH_TWIN_FBDEV], [test "x$with_twin_fbdev" = "xyes"])

AM_CONDITIONAL(
	[WITH_TWIN],
	[test "x$with_twin_x11" = "xyes" || test "x$with_twin_fbdev" = "xyes"])

AS_IF(
	[test "x$with_twin_x11" = "xyes" || test "x$with_twin_fbdev" = "xyes"],
	[PKG_CHECK_MODULES(
		[twin],
		[libtwin],
		[SAVE_LIBS="$LIBS" LIBS="$LIBS $twin_LIBS"
			AC_CHECK_LIB(
				[twin],
				[twin_feature_init],
				[],
				[AC_MSG_FAILURE([--with-twin was given but the test for libtwin failed.])]
			)
			LIBS="$SAVE_LIBS"
		],
		[AC_MSG_RESULT([$twin_PKG_ERRORS])
			AC_MSG_FAILURE([ Consider adjusting PKG_CONFIG_PATH environment variable])
		]
	)]
)

AS_IF(
	[test "x$with_twin_x11" = "xyes"],
	[SAVE_CPPFLAGS="$CPPFLAGS" CPPFLAGS="$CPPFLAGS $twin_CFLAGS"
		AC_CHECK_HEADERS(
			[libtwin/twin_x11.h],
			[],
			[AC_MSG_FAILURE([ --with-twin-x11 given but libtwin/twin_x11.h not found])]
		)
		CPPFLAGS="$SAVE_CPPFLAGS"
	]
)

AS_IF(
	[test "x$with_twin_fbdev" = "xyes"],
	[SAVE_CPPFLAGS="$CPPFLAGS" CPPFLAGS="$CPPFLAGS $twin_CFLAGS"
		AC_CHECK_HEADERS(
			[libtwin/twin_fbdev.h],
			[],
			[AC_MSG_FAILURE([ --with-twin-fbdev given but libtwin/twin_fbdev.h not found])]
		)
		CPPFLAGS="$SAVE_CPPFLAGS"
	]
)

AC_ARG_WITH(
	[signed-boot],
	[AS_HELP_STRING([--with-signed-boot],
		[build kernel signature checking support [default=no]]
	)],
	[],
	[with_signed_boot=no]
)

AM_CONDITIONAL(
	[WITH_SIGNED_BOOT],
	[test "x$with_signed_boot" = "xyes"])

AS_IF(
	[test "x$with_signed_boot" = "xyes"],
	[PKG_CHECK_MODULES(
		[GPGME],
		[gpgme >= 1.0.0],
		[SAVE_LIBS="$LIBS" LIBS="$LIBS $gpgme_LIBS"
			AC_CHECK_LIB(
				[gpgme],
				[gpgme_op_verify],
				[],
				[AC_MSG_FAILURE([--with-signed-boot was given but the test for gpgme failed.])]
			)
			LIBS="$SAVE_LIBS"
		],
		[AM_PATH_GPGME([1.0.0], [SAVE_LIBS="$LIBS" LIBS="$LIBS $gpgme_LIBS"
			AC_CHECK_LIB(
				[gpgme],
				[gpgme_op_verify],
				[],
				[AC_MSG_FAILURE([--with-signed-boot was given but the test for gpgme failed.])]
			)
			LIBS="$SAVE_LIBS"],
			[AC_MSG_RESULT([$gpgme_PKG_ERRORS])
				AC_MSG_FAILURE([ Consider adjusting PKG_CONFIG_PATH environment variable])
			])
		]
	)]
)

AS_IF(
	[test "x$with_signed_boot" = "xyes"],
	[SAVE_CPPFLAGS="$CPPFLAGS" CPPFLAGS="$CPPFLAGS $gpgme_CFLAGS"
		AC_CHECK_HEADERS(
			[gpgme.h],
			[],
			[AC_MSG_FAILURE([ --with-signed-boot given but gpgme.h not found])]
		)
		CPPFLAGS="$SAVE_CPPFLAGS"
	]
)

AM_CONDITIONAL([WITH_GPGME], [test "x$with_signed_boot" = "xyes"])

AC_ARG_VAR(
	[lockdown_file],
	[Location of authorized signature file [default = "/etc/pb-lockdown"]]
)
AS_IF([test "x$lockdown_file" = x], [lockdown_file="/etc/pb-lockdown"])
AC_DEFINE_UNQUOTED(LOCKDOWN_FILE, "$lockdown_file", [Lockdown file location])

AC_ARG_ENABLE(
	[busybox],
	[AS_HELP_STRING(
		[--enable-busybox],
		[build programs with extra support for busybox [default=no]])
	],
	[],
	[enable_busybox=no]
)
AM_CONDITIONAL([ENABLE_BUSYBOX], [test "x$enable_busybox" = "xyes"])
AS_IF([test "x$enable_busybox" = "xyes"],
      [AC_DEFINE(WITH_BUSYBOX, 1, [Busybox environment enabled])],
      []
)

AC_ARG_ENABLE(
	[mtd],
	[AS_HELP_STRING(
		[--enable-mtd],
		[Add support for MTD devices on certain platforms [default=no]]
	)],
	[],
	[enable_mtd=no]
)
AM_CONDITIONAL([ENABLE_MTD], [test "x$enable_mtd" = "xyes"])
AS_IF([test "x$enable_mtd" = "xyes"],
      [AC_DEFINE(MTD_SUPPORT, 1, [Enable MTD support])],
      []
)

AS_IF(
	[test "x$enable_mtd" = "xyes"],
		AC_CHECK_LIB([flash], [arch_flash_init],
			[LIBFLASH_LIBS=-lflash],
			[AC_MSG_FAILURE([--enable-mtd requires the libflash library])]
		)
)
AS_IF(
	[test "x$enable_mtd" = "xyes"],
	[SAVE_CPPFLAGS="$CPPFLAGS" CPPFLAGS="$CPPFLAGS $libflash_CFLAGS"
		AC_CHECK_HEADERS(
			[libflash/libflash.h],
			[],
			[AC_MSG_FAILURE([--enable-mtd given but libflash/libflash.h not found])]
		)
		CPPFLAGS="$SAVE_CPPFLAGS"
	]
)

AC_ARG_ENABLE(
	[ps3],
	[AS_HELP_STRING(
		[--enable-ps3],
		[build additional programs for the PS3 game console [default=no]]
	)],
	[],
	[enable_ps3=no]
)
AM_CONDITIONAL([ENABLE_PS3], [test "x$enable_ps3" = "xyes"])

AC_ARG_ENABLE(
	[debug],
	[AS_HELP_STRING([--enable-debug],
		[build programs with extra debug info [default=no]]
	)],
	[],
	[enable_debug=check]
)
#AM_CONDITIONAL([ENABLE_DEBUG], [test "x$enable_debug" = "xyes"])

AC_ARG_ENABLE(
	[test-valgrind],
	[AS_HELP_STRING([--enable-test-valgrind],
		[run all tests with valgrind]
	)],
)
AM_CONDITIONAL([ENABLE_TEST_VALGRIND], [test "x$enable_test_valgrind" = "xyes"])

# host program paths
AC_DEFUN([DEFINE_HOST_PROG],
	[
		AC_ARG_VAR([HOST_PROG_$1], [Path to "$2" on the host [default $3]])
		if test "x$HOST_PROG_$1" = "x"; then
			HOST_PROG_$1="$3"
		fi
		AC_DEFINE_UNQUOTED(HOST_PROG_$1, ["$HOST_PROG_$1"],
				[Path to "$2" on the host])
	])

DEFINE_HOST_PROG(CP, cp, [/bin/cp])
DEFINE_HOST_PROG(KEXEC, kexec, [/sbin/kexec])
DEFINE_HOST_PROG(MOUNT, mount, [/bin/mount])
DEFINE_HOST_PROG(SHUTDOWN, shutdown, [/sbin/shutdown])
DEFINE_HOST_PROG(SFTP, sftp, [/usr/bin/sftp])
DEFINE_HOST_PROG(TFTP, tftp, [/usr/bin/tftp])
DEFINE_HOST_PROG(UMOUNT, umount, [/bin/umount])
DEFINE_HOST_PROG(WGET, wget, [/usr/bin/wget])
DEFINE_HOST_PROG(IP, ip, [/sbin/ip])
DEFINE_HOST_PROG(UDHCPC, udhcpc, [/sbin/udhcpc])
DEFINE_HOST_PROG(VGSCAN, vgscan, [/usr/sbin/vgscan])
DEFINE_HOST_PROG(VGCHANGE, vgchange, [/usr/sbin/vgchange])
DEFINE_HOST_PROG(PB_PLUGIN, pb-plugin, [/usr/sbin/pb-plugin])
DEFINE_HOST_PROG(PB_EXEC, pb-exec, [/usr/sbin/pb-exec])
DEFINE_HOST_PROG(SH, sh, [/bin/sh])

AC_ARG_WITH(
    [tftp],
    [AS_HELP_STRING([--with-tftp=TYPE],
        [Use TYPE-type ftp client (either hpa or busybox) [default=runtime-check]]
    )],
    [],
    [with_tftp=detect]
)

case x$with_tftp in
'xhpa')
    tftp_type='TFTP_TYPE_HPA'
    ;;
'xbusybox')
    tftp_type='TFTP_TYPE_BUSYBOX'
    ;;
*)
    tftp_type='TFTP_TYPE_UNKNOWN'
    ;;
esac

AC_DEFINE_UNQUOTED(TFTP_TYPE, $tftp_type, [tftp client type])

default_cflags="--std=gnu99 -g \
	-Wall -W -Wunused -Wstrict-prototypes -Wmissing-prototypes \
	-Wmissing-declarations -Wredundant-decls"

AC_ARG_ENABLE(
	[werror],
	[AS_HELP_STRING([--enable-werror],
		[build programs with -Werror]
	)],
	[default_cflags="$default_cflags -Werror"]
)

AS_IF(
	[test "x$enable_debug" = "xyes"],
	[
		AC_SUBST([DEFAULT_CFLAGS], ["$default_cflags -O0"])
		AC_SUBST([DEFAULT_CPPFLAGS], ["-DDEBUG"])
	],
	[
		AC_SUBST([DEFAULT_CFLAGS], ["$default_cflags -O2"])
		AC_SUBST([DEFAULT_CPPFLAGS], ["-DNDEBUG"])
	]
)
 
AC_SUBST([UDEV_LIBS])
AC_SUBST([DEVMAPPER_LIBS])
AC_SUBST([FDT_LIBS])
AC_SUBST([LIBFLASH_LIBS])
AC_SUBST([LIBTOOL_DEPS])
AC_SUBST([DESTDIR])
AC_SUBST([pkgsysconfdir], ["${sysconfdir}/${package}"])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([
	Makefile
	po/Makefile.in
])

AC_OUTPUT
