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

lib_TESTS = \
	test/lib/list-test \
	test/lib/test-process-noargs \
	test/lib/test-process-sync \
	test/lib/test-process-sync-stdout \
	test/lib/test-process-stderr \
	test/lib/test-process-stderr-stdout \
	test/lib/test-process-async \
	test/lib/test-process-async-stdout \
	test/lib/test-process-parent-stdout \
	test/lib/test-process-both \
	test/lib/test-process-stdout-eintr \
	test/lib/test-fold

if WITH_OPENSSL
lib_TESTS += \
	test/lib/test-security-openssl-verify \
	test/lib/test-security-openssl-decrypt
endif

$(lib_TESTS): LIBS += $(core_lib)
$(lib_TESTS): AM_CPPFLAGS += -DTEST_LIB_DATA_BASE='"$(abs_top_srcdir)/test/lib/data"'

check_PROGRAMS += $(lib_TESTS)
TESTS += $(lib_TESTS)
