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


AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/lib \
	-I$(includedir) \
	$(DEFAULT_CPPFLAGS)

AM_CFLAGS = \
	$(DEFAULT_CFLAGS)

common_libs = \
	$(top_builddir)/lib/libpbcore.la \
	$(top_builddir)/discover/libparser.la

noinst_PROGRAMS = parser-test

parser_test_SOURCES = parser-test.c
parser_test_LDADD = $(common_libs)

noinst_DATA = \
	data/grub-01/grub.cfg \
	data/grub-02/grub.cfg \
	data/kboot-01/expected-output \
	data/kboot-01/etc/kboot.conf \
	data/kboot-02/expected-output \
	data/kboot-02/etc/kboot.conf \
	data/kboot-03/expected-output \
	data/kboot-03/etc/kboot.conf \
	data/kboot-04/expected-output \
	data/kboot-04/etc/kboot.conf \
	data/kboot-05/expected-output \
	data/kboot-05/etc/kboot.conf \
	data/kboot-06/expected-output \
	data/kboot-06/etc/kboot.conf \
	data/kboot-07/etc/kboot.conf \
	data/yaboot-01/expected-output \
	data/yaboot-01/etc/yaboot.conf \
	data/yaboot-02/etc/yaboot.conf \
	data/yaboot-03/etc/yaboot.conf \
	data/yaboot-04/etc/yaboot.conf \
	data/yaboot-05/etc/yaboot.conf \
	data/yaboot-06/etc/yaboot.conf

composed_noinst_scripts = \
	run-parser-tests

noinst_SCRIPTS = $(composed_noinst_scripts)

edit = sed \
	-e 's|@PACKAGE_NAME\@|$(PACKAGE_NAME)|g' \
	-e 's|@PACKAGE_VERSION\@|$(PACKAGE_VERSION)|g' \
	-e 's|@PACKAGE_BUGREPORT\@|$(PACKAGE_BUGREPORT)|g' \
	-e 's|@abs_srcdir\@|$(abs_srcdir)|g' \
	-e 's|@abs_builddir\@|$(abs_builddir)|g' \
	-e 's|@prefix\@|$(prefix)|g'

run-parser-tests: Makefile $(srcdir)/run-parser-tests.in
	rm -f $@ $@.tmp
	$(edit) $(srcdir)/$@.in >$@.tmp
	chmod +x $@.tmp
	mv $@.tmp $@

composed_files = $(composed_noinst_scripts)

EXTRA_DIST = $(addsuffix .in, $(composed_files)) $(noinst_DATA)

MAINTAINERCLEANFILES = Makefile.in

CLEANFILES = $(composed_files) $(addsuffix .tmp, $(composed_files))
