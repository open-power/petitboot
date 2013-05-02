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
	-I$(includedir) \
	$(DEFAULT_CPPFLAGS)

AM_CFLAGS = \
	$(DEFAULT_CFLAGS)

parse_url_SOURCES = parse-url.c

parse_url_LDADD = ../../lib/libpbcore.la

check_PROGRAMS = parse-url
check_SCRIPTS = run-url-test

TESTS = data/double-slash.test \
	data/http-simple.test \
	data/join-full.test \
	data/join-absolute.test \
	data/join-relative.test \
	data/localpath.test

TEST_EXTENSIONS = .test
TEST_LOG_COMPILER = $(builddir)/run-url-test

edit = sed \
	-e 's|@PACKAGE_NAME\@|$(PACKAGE_NAME)|g' \
	-e 's|@PACKAGE_VERSION\@|$(PACKAGE_VERSION)|g' \
	-e 's|@PACKAGE_BUGREPORT\@|$(PACKAGE_BUGREPORT)|g' \
	-e 's|@abs_srcdir\@|$(abs_srcdir)|g' \
	-e 's|@abs_builddir\@|$(abs_builddir)|g' \
	-e 's|@prefix\@|$(prefix)|g'

EXTRA_DIST = $(TESTS) run-url-test.in

run-url-test: Makefile $(srcdir)/run-url-test.in
	rm -f $@ $@.tmp
	$(edit) $(srcdir)/$@.in >$@.tmp
	chmod +x $@.tmp
	mv $@.tmp $@

CLEANFILES = run-url-test
