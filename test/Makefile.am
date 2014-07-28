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

if ENABLE_TEST_VALGRIND
LOG_COMPILER = valgrind
AM_LOG_FLAGS = --leak-check=full --error-exitcode=1
export LOG_COMPILER AM_LOG_FLAGS
endif

noinst_SCRIPTS = test/hotplug-device.sh
EXTRA_DIST += $(noinst_SCRIPTS)

$(check_PROGRAMS): AM_CFLAGS = -O0 -g -Wall -Wextra -Werror
$(check_PROGRAMS): AM_CPPFLAGS = -DDEBUG -I$(top_srcdir)/lib
