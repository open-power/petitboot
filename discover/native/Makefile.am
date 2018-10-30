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

noinst_PROGRAMS += discover/native/native-parser.ro

discover_native_native_parser_ro_SOURCES = \
	discover/native/native.h \
	discover/native/native.c \
	discover/native/native-lexer.l \
	discover/native/native-parser.y

BUILT_SOURCES += \
	discover/native/native-parser.c \
	discover/native/native-parser.h \
	discover/native/native-lexer.h \
	discover/native/native-lexer.c

CLEANFILES += \
	discover/native/native-parser.c \
	discover/native/native-parser.h \
	discover/native/native-lexer.c \
	discover/native/native-lexer.h

discover_native_native_parser_ro_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	-I$(top_srcdir)/discover/native \
	-I$(top_builddir)/discover/native

discover_native_native_parser_ro_LINK = \
	$(LD) -r -o $@

# ylwrap doesn't handle flex header files well; use our own rule here.
discover/native/native-lexer.h discover/native/native-lexer.c: \
		$(top_srcdir)/discover/native/native-lexer.l
	$(AM_V_LEX)$(LEXCOMPILE) --header-file=discover/native/native-lexer.h \
				       -o discover/native/native-lexer.c $^

# We need to loosen our warnings for the generated lexer code.
discover/native/%native-lexer.o discover/native/native-lexer.o: \
		AM_CFLAGS += -Wno-unused-parameter -Wno-missing-prototypes \
			       -Wno-missing-declarations -Wno-sign-compare

$(discover_native_native_parser_ro_OBJECTS): discover/native/native-parser.h
