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
	-DDEBUG

AM_CFLAGS = -O0 -ggdb -Wall -Wextra -Werror

LDADD = ../../lib/libpbcore.la

check_PROGRAMS = list-test \
	test-process-noargs \
	test-process-sync \
	test-process-sync-stdout \
	test-process-stderr \
	test-process-stderr-stdout \
	test-process-async \
	test-process-async-stdout \
	test-process-parent-stdout \
	test-process-both \
	test-fold

TESTS = $(check_PROGRAMS)

MAINTAINERCLEANFILES = Makefile.in
