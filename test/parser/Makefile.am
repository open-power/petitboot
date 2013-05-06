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

TESTS = test-null \
	test-kboot-single \
	test-yaboot-single \
	test-yaboot-external

check_PROGRAMS = $(TESTS)
check_LIBRARIES = $(test_libs)
check_DATA =

common_libs = $(top_builddir)/lib/libpbcore.la
test_libs = libtest.o

libtest.o: $(libtest_o_OBJECTS)
	$(LD) -o $@ -r $^

# objects under test
parser_test_objs = $(top_srcdir)/discover/yaboot-parser.c \
		   $(top_srcdir)/discover/kboot-parser.c \
		   $(top_srcdir)/discover/resource.c \
		   $(top_srcdir)/discover/paths.c \
		   $(top_srcdir)/discover/device-handler.c \
		   $(top_srcdir)/discover/parser-conf.c


LDADD = $(common_libs) $(test_libs)

libtest_o_SOURCES = utils.c parser-test.h handler.c main.c $(parser_test_objs)

$(check_PROGRAMS): %: %.embedded-config.o
$(check_PROGRAMS): LDADD += $@.embedded-config.o

extract_config = $(srcdir)/extract-config.awk

%.embedded-config.c: %.c $(extract_config)
	gawk --file=$(extract_config) $^ > $@

EXTRA_DIST = $(check_DATA) $(extract_config)

CLEANFILES = $(foreach f, $(check_PROGRAMS), \
		$(f).embedded-config.c $(f).embedded-config.o)
