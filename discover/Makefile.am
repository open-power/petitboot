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

sbin_PROGRAMS += discover/pb-discover
noinst_PROGRAMS += discover/platform.ro

discover_pb_discover_SOURCES = \
	discover/boot.c \
	discover/boot.h \
	discover/cdrom.c \
	discover/cdrom.h \
	discover/device-handler.c \
	discover/device-handler.h \
	discover/discover-server.c \
	discover/discover-server.h \
	discover/devmapper.c \
	discover/devmapper.h \
	discover/event.c \
	discover/event.h \
	discover/parser.c \
	discover/parser.h \
	discover/parser-conf.c \
	discover/parser-conf.h \
	discover/parser-utils.c \
	discover/parser-utils.h \
	discover/paths.c \
	discover/paths.h \
	discover/pb-discover.c \
	discover/pb-discover.h \
	discover/resource.c \
	discover/resource.h \
	discover/sysinfo.c \
	discover/sysinfo.h \
	discover/network.c \
	discover/network.h \
	discover/udev.c \
	discover/udev.h \
	discover/user-event.c \
	discover/user-event.h \
	discover/kboot-parser.c \
	discover/yaboot-parser.c \
	discover/pxe-parser.c \
	discover/syslinux-parser.c

discover_pb_discover_LDADD = \
	discover/grub2/grub2-parser.ro \
	discover/native/native-parser.ro \
	discover/platform.ro \
	$(core_lib) \
	$(UDEV_LIBS) \
	$(ELF_LIBS)

discover_pb_discover_LDFLAGS = \
	$(AM_LDFLAGS) \
	$(DEVMAPPER_LIBS)

discover_pb_discover_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	-DLOCAL_STATE_DIR='"$(localstatedir)"' \
	-DLOCALEDIR='"$(localedir)"' \
	-DPKG_LIBEXEC_DIR='"$(pkglibexecdir)"' \
	-DPKG_SHARE_DIR='"$(pkgsharedir)"' \
	-DPKG_SYSCONF_DIR='"$(pkgsysconfdir)"'

discover_platform_ro_SOURCES = \
	discover/platform.c \
	discover/platform.h \
	discover/ipmi.c \
	discover/ipmi.h \
	discover/dt.c \
	discover/dt.h \
	discover/hostboot.h \
	discover/elf.c \
	discover/elf.h

if PLATFORM_ARM64
discover_platform_ro_SOURCES += discover/platform-arm64.c
endif

if PLATFORM_POWERPC
discover_platform_ro_SOURCES += discover/platform-powerpc.c
endif

# Build dummy last to put it at the end of the platforms section.
discover_platform_ro_SOURCES += discover/platform-dummy.c

if ENABLE_MTD
discover_platform_ro_SOURCES += \
	discover/hostboot.c

discover_platform_ro_LDFLAGS = \
	$(core_lib) \
	$(UDEV_LIBS)

endif

discover_platform_ro_CPPFLAGS = \
	$(AM_CPPFLAGS)

discover_platform_ro_LINK = \
	$(LD) -r -o $@
