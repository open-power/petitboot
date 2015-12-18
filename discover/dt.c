#include <asm/byteorder.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include <talloc/talloc.h>
#include <file/file.h>

#include "dt.h"

static int filter_sensors(const struct dirent *ent)
{
	/* Check for prefix "sensor@" */
	return strncmp(ent->d_name, "sensor@", strlen("sensor@")) == 0;
}

int get_ipmi_sensor(void *t, enum ipmi_sensor_ids sensor_id)
{
	int rc, len, n;
	struct dirent **namelist;
	char *buf, *filename;
	const char sensor_dir[] = "/proc/device-tree/bmc/sensors/";

	n = scandir(sensor_dir, &namelist, filter_sensors, alphasort);
	if (n <= 0)
		return -1;

	while (n--) {
		filename = talloc_asprintf(t, "%s%s/ipmi-sensor-type",
					   sensor_dir, namelist[n]->d_name);
		rc = read_file(t, filename, &buf, &len);
		if (rc == 0 && len == 4 &&
		    __be32_to_cpu(*(uint32_t *)buf) == sensor_id)
				break;
		free(namelist[n]);
	}
	if (n < 0) {
		rc = -1;
		goto out;
	}

	filename = talloc_asprintf(t, "%s%s/reg", sensor_dir,
				   namelist[n]->d_name);
	/* Free the rest of the scandir strings, if there are any */
	do {
		free(namelist[n]);
	} while (n-- > 0);

	rc = read_file(t, filename, &buf, &len);
	if (rc != 0 || len != 4) {
		rc = -1;
		goto out;
	}

	rc = __be32_to_cpu(*(uint32_t *)buf);

out:
	talloc_free(buf);
	free(namelist);
	return rc;
}
