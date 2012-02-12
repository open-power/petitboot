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

if WITH_NCURSES
  EXTRA_1 = ncurses
endif

if WITH_TWIN_X11
  EXTRA_2 = twin
endif

if WITH_TWIN_FBDEV
  EXTRA_2 = twin
endif

SUBDIRS = common test $(EXTRA_1) $(EXTRA_2)

MAINTAINERCLEANFILES = Makefile.in
