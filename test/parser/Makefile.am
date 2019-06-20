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

parser_TESTS = \
	test/parser/test-null \
	test/parser/test-grub2-single \
	test/parser/test-grub2-default \
	test/parser/test-grub2-empty \
	test/parser/test-grub2-noeol \
	test/parser/test-grub2-menuentry-formats \
	test/parser/test-grub2-if-formats \
	test/parser/test-grub2-default-id \
	test/parser/test-grub2-default-id-space \
	test/parser/test-grub2-default-index \
	test/parser/test-grub2-default-multiword \
	test/parser/test-grub2-implicit-default-unset \
	test/parser/test-grub2-implicit-default-invalid \
	test/parser/test-grub2-multiple-resolve \
	test/parser/test-grub2-multiple-id \
	test/parser/test-grub2-single-line-if \
	test/parser/test-grub2-pos-param \
	test/parser/test-grub2-load-env \
	test/parser/test-grub2-save-env \
	test/parser/test-grub2-save-env-dash-f \
	test/parser/test-grub2-saved-default \
	test/parser/test-grub2-nondefault-prefix \
	test/parser/test-grub2-f18-ppc64 \
	test/parser/test-grub2-f20-ppc64 \
	test/parser/test-grub2-ubuntu-13_04-x86 \
	test/parser/test-grub2-sles-btrfs-snapshot \
	test/parser/test-grub2-lexer-error \
	test/parser/test-grub2-parser-error \
	test/parser/test-grub2-test-file-ops \
	test/parser/test-grub2-single-yocto \
	test/parser/test-grub2-blscfg-default-filename \
	test/parser/test-grub2-blscfg-default-index \
	test/parser/test-grub2-blscfg-default-title \
	test/parser/test-grub2-blscfg-multiple-bls \
	test/parser/test-grub2-blscfg-opts-config \
	test/parser/test-grub2-blscfg-opts-grubenv \
	test/parser/test-kboot-single \
	test/parser/test-kboot-default \
	test/parser/test-yaboot-empty \
	test/parser/test-yaboot-single \
	test/parser/test-yaboot-partition \
	test/parser/test-yaboot-partition-override \
	test/parser/test-yaboot-external \
	test/parser/test-yaboot-root-global \
	test/parser/test-yaboot-root-override \
	test/parser/test-yaboot-device-override \
	test/parser/test-yaboot-global-state \
	test/parser/test-yaboot-leftovers \
	test/parser/test-yaboot-default \
	test/parser/test-yaboot-rh8-ppc64 \
	test/parser/test-pxe-empty \
	test/parser/test-pxe-single \
	test/parser/test-pxe-initrd-in-append \
	test/parser/test-pxe-mac-without-conf \
	test/parser/test-pxe-ip-without-conf \
	test/parser/test-pxe-ipxe \
	test/parser/test-pxe-ipxe-named \
	test/parser/test-pxe-ipxe-default \
	test/parser/test-pxe-non-url-conf \
	test/parser/test-pxe-local \
	test/parser/test-pxe-ipappend \
	test/parser/test-pxe-pathprefix-with-conf \
	test/parser/test-pxe-non-url-pathprefix-with-conf \
	test/parser/test-pxe-pathprefix-discover \
	test/parser/test-pxe-pathprefix-discover-mac \
	test/parser/test-pxe-pathprefix-port \
	test/parser/test-pxe-path-resolve-relative \
	test/parser/test-pxe-path-resolve-absolute \
	test/parser/test-pxe-discover-bootfile-root \
	test/parser/test-pxe-discover-bootfile-subdir \
	test/parser/test-pxe-discover-bootfile-pathprefix \
	test/parser/test-pxe-discover-bootfile-relative-conffile \
	test/parser/test-pxe-discover-bootfile-absolute-conffile \
	test/parser/test-pxe-discover-bootfile-async-file \
	test/parser/test-unresolved-remove \
	test/parser/test-syslinux-single-yocto \
	test/parser/test-syslinux-global-append \
	test/parser/test-syslinux-explicit \
	test/parser/test-syslinux-nested-config \
	test/parser/test-native-globals \
	test/parser/test-native-short \
	test/parser/test-native-simple \
	test/parser/test-native-strings


TESTS += $(parser_TESTS)
check_PROGRAMS += $(parser_TESTS) test/parser/libtest.ro

check_DATA += \
	test/parser/data/grub2-f18-ppc64.conf \
	test/parser/data/grub2-f20-ppc.conf \
	test/parser/data/grub2-ubuntu-13_04-x86.conf \
	test/parser/data/yaboot-rh8-ppc64.conf \
	test/parser/data/syslinux-include-root.cfg \
	test/parser/data/syslinux-include-nest-1.cfg \
	test/parser/data/syslinux-include-nest-2.cfg \
	test/parser/data/native-short.conf

$(parser_TESTS): AM_CPPFLAGS += \
		-I$(top_srcdir)/discover \
		-DLOCAL_STATE_DIR='"$(localstatedir)"'
$(parser_TESTS): LDADD += $@.embedded-config.o test/parser/libtest.ro $(core_lib)
$(parser_TESTS): %: %.embedded-config.o test/parser/libtest.ro $(core_lib)

extract_config = $(srcdir)/test/parser/extract-config.awk

%.embedded-config.c: %.c $(extract_config)
	$(AM_V_GEN)$(AWK) -f $(extract_config) $< > $@

# objects under test

test_parser_libtest_ro_SOURCES = \
	test/parser/main.c \
	test/parser/utils.c \
	test/parser/handler.c \
	test/parser/network.c \
	test/parser/user-event.c \
	test/parser/parser-test.h \
	discover/yaboot-parser.c \
	discover/kboot-parser.c \
	discover/pxe-parser.c \
	discover/syslinux-parser.c \
	discover/platform.c \
	discover/resource.c \
	discover/paths.c \
	discover/device-handler.c \
	discover/parser-conf.c \
	discover/user-event.c \
	discover/event.c \
	$(discover_grub2_grub2_parser_ro_SOURCES) \
	$(discover_native_native_parser_ro_SOURCES)

test_parser_libtest_ro_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	-I$(top_srcdir)/discover \
	-I$(top_srcdir)/discover/grub2 \
	-I$(top_builddir)/discover/grub2 \
	-I$(top_srcdir)/discover/native \
	-I$(top_builddir)/discover/native \
	-DPETITBOOT_TEST \
	-DLOCAL_STATE_DIR='"$(localstatedir)"' \
	-DTEST_CONF_BASE='"$(top_srcdir)/test/parser/data"'

test/parser/libtest.ro$(EXEEXT): $(test_parser_libtest_ro_OBJECTS) \
					   $(test_parser_libtest_ro_LDADD)
	$(AM_V_GEN)$(LD) -o $@ -r $^

EXTRA_DIST += $(check_DATA) $(extract_config)

CLEANFILES += \
	$(foreach f, $(TESTS), $(f).embedded-config.c $(f).embedded-config.o)

