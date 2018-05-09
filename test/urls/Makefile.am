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

check_PROGRAMS += test/urls/parse-url
check_SCRIPTS += test/urls/run-url-test

test_urls_parse_url_SOURCES = test/urls/parse-url.c
test_urls_parse_url_LDADD = $(core_lib)

url_TESTS = \
	test/urls/data/double-slash.test \
	test/urls/data/http-simple.test \
	test/urls/data/ipv6-full.test \
	test/urls/data/ipv6-multidirs.test \
	test/urls/data/ipv6-noport.test \
	test/urls/data/join-full.test \
	test/urls/data/join-absolute.test \
	test/urls/data/join-relative.test \
	test/urls/data/localpath.test

TESTS += $(url_TESTS)

TEST_EXTENSIONS = .test
TEST_LOG_COMPILER = $(builddir)/test/urls/run-url-test

if ENABLE_TEST_VALGRIND
TEST_LOG_FLAGS = --valgrind
endif

edit = sed \
	-e 's|@PACKAGE_NAME\@|$(PACKAGE_NAME)|g' \
	-e 's|@PACKAGE_VERSION\@|$(PACKAGE_VERSION)|g' \
	-e 's|@PACKAGE_BUGREPORT\@|$(PACKAGE_BUGREPORT)|g' \
	-e 's|@abs_srcdir\@|$(abs_srcdir)/test/urls|g' \
	-e 's|@abs_builddir\@|$(abs_builddir)/test/urls|g' \
	-e 's|@prefix\@|$(prefix)|g'

EXTRA_DIST += $(url_TESTS) test/urls/run-url-test.in

test/urls/run-url-test: $(srcdir)/test/urls/run-url-test.in
	$(AM_V_GEN)$(edit) $(srcdir)/$@.in > $@.tmp && \
		chmod +x $@.tmp && \
		mv $@.tmp $@

CLEANFILES += test/urls/run-url-test

