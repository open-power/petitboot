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

ui_TESTS = \
	test/ui/console-sequence

TESTS += $(ui_TESTS)
check_PROGRAMS += $(ui_TESTS)

test_ui_console_sequence_SOURCES = \
	test/ui/console-sequence.c

test_ui_console_sequence_CPPFLAGS = \
	-DPETITBOOT_TEST \
	-I$(top_srcdir)/lib

test_ui_console_sequence_LDFLAGS = \
	$(core_lib)



#$(ui_TESTS): LIBS += \
#	ui/ncurses/libpbnc.la \
#	$(core_lib) \
#	@MENU_LIB@ @FORM_LIB@ @CURSES_LIB@
