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

core_lib = lib/libpbcore.la

noinst_LTLIBRARIES += $(core_lib)

lib_libpbcore_la_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	-DPREFIX='"$(prefix)"' \
	$(OPENSSL_INCLUDES)

lib_libpbcore_la_LIBADD = \
	$(GPGME_LIBS) \
	$(OPENSSL_LIBS)

lib_libpbcore_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	$(OPENSSL_LDFLAGS)

lib_libpbcore_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(GPGME_CFLAGS)

lib_libpbcore_la_SOURCES = \
	lib/ccan/endian/endian.h \
	lib/file/file.h \
	lib/file/file.c \
	lib/fold/fold.h \
	lib/fold/fold.c \
	lib/i18n/i18n.h \
	lib/i18n/i18n.c \
	lib/log/log.h \
	lib/log/log.c \
	lib/list/list.c \
	lib/list/list.h \
	lib/waiter/waiter.c \
	lib/waiter/waiter.h \
	lib/pb-protocol/pb-protocol.c \
	lib/pb-protocol/pb-protocol.h \
	lib/pb-config/pb-config.c \
	lib/pb-config/pb-config.h \
	lib/process/process.c \
	lib/process/process.h \
	lib/types/types.c \
	lib/types/types.h \
	lib/talloc/talloc.c \
	lib/talloc/talloc.h \
	lib/system/system.c \
	lib/system/system.h \
	lib/url/url.c \
	lib/url/url.h \
	lib/util/util.c \
	lib/util/util.h \
	lib/flash/config.h \
	lib/flash/flash.h \
	lib/security/security.h

if ENABLE_MTD
lib_libpbcore_la_SOURCES += \
	lib/flash/flash.c

lib_libpbcore_la_LDFLAGS += \
	$(LIBFLASH_LIBS)
endif

if WITH_GPGME
lib_libpbcore_la_SOURCES += \
	lib/security/common.c \
	lib/security/gpg.c
else
if WITH_OPENSSL
lib_libpbcore_la_SOURCES += \
	lib/security/common.c \
	lib/security/openssl.c
else
lib_libpbcore_la_SOURCES += \
	lib/security/none.c
endif
endif
