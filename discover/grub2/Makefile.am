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

AM_CPPFLAGS = -I$(top_srcdir)/lib -I$(top_srcdir) $(DEFAULT_CPPFLAGS)

AM_CFLAGS = $(DEFAULT_CFLAGS)  \
	-DPREFIX='"$(prefix)"' \
	-DPKG_SHARE_DIR='"$(pkgdatadir)"' \
	-DPKG_SYSCONF_DIR='"$(pkgsysconfdir)"' \
	-DLOCAL_STATE_DIR='"$(localstatedir)"'

AM_YFLAGS = -d

noinst_PROGRAMS = grub2-parser.ro

grub2-parser.ro$(EXEEXT): $(grub2_parser_ro_OBJECTS)
	$(LD) -r -o $@ $^


grub2_parser_ro_SOURCES = \
	builtins.c \
	env.c \
	grub2.h \
	grub2.c \
	lexer.l \
	parser.y \
	script.c \
	parser.c

BUILT_SOURCES = parser.h lexer.h lexer.c lexer.h
CLEANFILES = lexer.c lexer.h
MAINTAINERCLEANFILES = Makefile.in

# ylwrap doesn't handle flex header files well; use our own rule here.
lexer.h lexer.c: lexer.l
	$(LEX) $(LFLAGS) --header-file=lexer.h -o lexer.c $^

lexer.o: CFLAGS+=-Wno-unused-parameter -Wno-missing-prototypes \
		       -Wno-missing-declarations
