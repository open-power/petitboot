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

AUTOMAKE_OPTIONS = parallel-tests

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/discover \
	-DLOCAL_STATE_DIR='"$(localstatedir)"' \
	-DTEST_CONF_BASE='"$(srcdir)/data"' \
	-DDEBUG -DPETITBOOT_TEST

AM_CFLAGS = -O0 -ggdb -Wall -Wextra -Werror

LDADD = $(top_builddir)/lib/libpbcore.la libtest.ro

TESTS = \
	test-null \
	test-grub2-single \
	test-grub2-default \
	test-grub2-default-index \
	test-grub2-default-multiword \
	test-grub2-multiple-resolve \
	test-grub2-multiple-id \
	test-grub2-single-line-if \
	test-grub2-load-env \
	test-grub2-save-env \
	test-grub2-saved-default \
	test-grub2-nondefault-prefix \
	test-grub2-f18-ppc64 \
	test-grub2-f20-ppc64 \
	test-grub2-ubuntu-13_04-x86 \
	test-grub2-lexer-error \
	test-grub2-parser-error \
	test-kboot-single \
	test-yaboot-empty \
	test-yaboot-single \
	test-yaboot-partition \
	test-yaboot-partition-override \
	test-yaboot-external \
	test-yaboot-root-global \
	test-yaboot-root-override \
	test-yaboot-device-override \
	test-yaboot-global-state \
	test-yaboot-leftovers \
	test-yaboot-default \
	test-yaboot-rh8-ppc64 \
	test-pxe-empty \
	test-pxe-single \
	test-pxe-initrd-in-append \
	test-pxe-mac-without-conf \
	test-pxe-ip-without-conf \
	test-pxe-non-url-conf \
	test-pxe-local \
	test-pxe-pathprefix-with-conf \
	test-pxe-non-url-pathprefix-with-conf \
	test-pxe-pathprefix-discover \
	test-pxe-pathprefix-discover-mac \
	test-pxe-path-resolve-relative \
	test-pxe-path-resolve-absolute \
	test-pxe-discover-bootfile-root \
	test-pxe-discover-bootfile-subdir \
	test-pxe-discover-bootfile-pathprefix \
	test-pxe-discover-bootfile-relative-conffile \
	test-pxe-discover-bootfile-absolute-conffile \
	test-unresolved-remove

$(TESTS): %: %.embedded-config.o
$(TESTS): LDADD += $@.embedded-config.o

extract_config = $(srcdir)/extract-config.awk

%.embedded-config.c: %.c $(extract_config)
	$(AWK) -f $(extract_config) $< > $@

# objects under test
parser_objs = \
	$(top_srcdir)/discover/yaboot-parser.c \
	$(top_srcdir)/discover/kboot-parser.c \
	$(top_srcdir)/discover/pxe-parser.c \
	$(top_srcdir)/discover/platform.c \
	$(top_srcdir)/discover/resource.c \
	$(top_srcdir)/discover/paths.c \
	$(top_srcdir)/discover/device-handler.c \
	$(top_srcdir)/discover/parser-conf.c \
	$(top_srcdir)/discover/user-event.c \
	$(top_srcdir)/discover/event.c

libtest_ro_SOURCES = \
	main.c \
	utils.c \
	handler.c \
	parser-test.h \
	$(parser_objs)

libtest_ro_LDADD = \
	../../discover/grub2/grub2-parser.ro

libtest.ro$(EXEEXT): $(libtest_ro_OBJECTS) $(libtest_ro_LDADD)
	$(LD) -o $@ -r $^

check_PROGRAMS = $(TESTS) libtest.ro

check_DATA = \
	data/grub2-f18-ppc64.conf \
	data/grub2-f20-ppc.conf \
	data/grub2-ubuntu-13_04-x86.conf \
	data/yaboot-rh8-ppc64.conf

EXTRA_DIST = $(check_DATA) $(extract_config)

CLEANFILES = \
	$(foreach f, $(TESTS), $(f).embedded-config.c $(f).embedded-config.o)

MAINTAINERCLEANFILES = Makefile.in
