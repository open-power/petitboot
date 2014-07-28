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

dist_sbin_SCRIPTS += utils/pb-udhcpc
dist_pkglibexec_SCRIPTS = utils/pb-console utils/pb-sysinfo
sbin_PROGRAMS += utils/pb-event

utils_pb_event_SOURCES = utils/pb-event.c

dist_pkgdata_DATA = \
	utils/kboot.conf.sample \
	utils/bb-kexec-reboot \
	utils/logrotate.conf \
	utils/hooks/01-create-default-dtb \
	utils/hooks/20-update-dtb-sample \
	utils/hooks/20-set-stdout \
	utils/hooks/90-sort-dtb
