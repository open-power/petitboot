#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <sys/ioctl.h>
#include <linux/cdrom.h>

#include <log/log.h>

#include "cdrom.h"

static int cdrom_open(const char *devpath, const char *loc)
{
	int fd;

	fd = open(devpath, O_RDONLY | O_NONBLOCK);
	if (fd < 0)
		pb_log("%s: can't open %s: %s\n", loc, devpath,
				strerror(errno));

	return fd;
}

void cdrom_init(const char *devpath)
{
	int fd, rc;

	fd = cdrom_open(devpath, __func__);
	if (fd < 0)
		return;

	/* We disable autoclose so that any attempted mount() operation doesn't
	 * close the tray, and disable CDO_LOCK to prevent the lock status
	 * changing on open()/close()
	 */
	rc = ioctl(fd, CDROM_CLEAR_OPTIONS, CDO_LOCK | CDO_AUTO_CLOSE);
	if (rc < 0)
		pb_debug("%s: CLEAR CDO_LOCK|CDO_AUTO_CLOSE failed: %s\n",
				__func__, strerror(errno));

	close(fd);
}

bool cdrom_media_present(const char *devpath)
{
	int fd, rc;

	fd = cdrom_open(devpath, __func__);
	if (fd < 0)
		return false;

	rc = ioctl(fd, CDROM_DRIVE_STATUS, CDSL_CURRENT);

	close(fd);

	return rc == CDS_DISC_OK;
}

void cdrom_eject(const char *devpath)
{
	int fd, rc;

	fd = cdrom_open(devpath, __func__);
	if (fd < 0)
		return;

	/* unlock cdrom device */
	rc = ioctl(fd, CDROM_LOCKDOOR, 0);
	if (rc < 0)
		pb_log("%s: CDROM_LOCKDOOR(unlock) failed: %s\n",
				__func__, strerror(errno));

	rc = ioctl(fd, CDROMEJECT, 0);
	if (rc < 0)
		pb_log("%s: CDROM_EJECT failed: %s\n",
				__func__, strerror(errno));
	close(fd);
}

