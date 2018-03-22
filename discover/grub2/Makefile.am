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

noinst_PROGRAMS += discover/grub2/grub2-parser.ro

discover_grub2_grub2_parser_ro_SOURCES = \
	discover/grub2/blscfg.c \
	discover/grub2/builtins.c \
	discover/grub2/env.c \
	discover/grub2/grub2.h \
	discover/grub2/grub2.c \
	discover/grub2/grub2-lexer.l \
	discover/grub2/grub2-parser.y \
	discover/grub2/script.c

BUILT_SOURCES += \
	discover/grub2/grub2-parser.c \
	discover/grub2/grub2-parser.h \
	discover/grub2/grub2-lexer.h \
	discover/grub2/grub2-lexer.c

CLEANFILES += \
	discover/grub2/grub2-parser.c \
	discover/grub2/grub2-parser.h \
	discover/grub2/grub2-lexer.c \
	discover/grub2/grub2-lexer.h

discover_grub2_grub2_parser_ro_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	-I$(top_srcdir)/discover/grub2 \
	-I$(top_builddir)/discover/grub2

discover_grub2_grub2_parser_ro_LINK = \
	$(LD) -r -o $@

# ylwrap doesn't handle flex header files well; use our own rule here.
discover/grub2/grub2-lexer.h discover/grub2/grub2-lexer.c: \
		$(top_srcdir)/discover/grub2/grub2-lexer.l
	$(AM_V_LEX)$(LEXCOMPILE) --header-file=discover/grub2/grub2-lexer.h \
				       -o discover/grub2/grub2-lexer.c $^

# We need to loosen our warnings for the generated lexer code.
discover/grub2/%grub2-lexer.o discover/grub2/grub2-lexer.o: \
		AM_CFLAGS += -Wno-unused-parameter -Wno-missing-prototypes \
			       -Wno-missing-declarations -Wno-sign-compare

$(discover_grub2_grub2_parser_ro_OBJECTS): discover/grub2/grub2-parser.h
