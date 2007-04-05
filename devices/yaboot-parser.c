
#include "parser.h"
#include "params.h"
#include "yaboot-cfg.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/param.h>

static struct device *dev;
static const char *mountpoint;
static char partition_mntpoint[PATH_MAX];
static char *defimage;

char *
make_params(char *label, char *params)
{
     char *p, *q;
     static char buffer[2048];

     q = buffer;
     *q = 0;

     p = cfg_get_strg(label, "literal");
     if (p) {
          strcpy(q, p);
          q = strchr(q, 0);
          if (params) {
               if (*p)
                    *q++ = ' ';
               strcpy(q, params);
          }
          return buffer;
     }

     p = cfg_get_strg(label, "root");
     if (p) {
          strcpy (q, "root=");
          strcpy (q + 5, p);
          q = strchr (q, 0);
          *q++ = ' ';
     }
     if (cfg_get_flag(label, "read-only")) {
          strcpy (q, "ro ");
          q += 3;
     }
     if (cfg_get_flag(label, "read-write")) {
          strcpy (q, "rw ");
          q += 3;
     }
     p = cfg_get_strg(label, "ramdisk");
     if (p) {
          strcpy (q, "ramdisk=");
          strcpy (q + 8, p);
          q = strchr (q, 0);
          *q++ = ' ';
     }
     p = cfg_get_strg(label, "initrd-size");
     if (p) {
          strcpy (q, "ramdisk_size=");
          strcpy (q + 13, p);
          q = strchr (q, 0);
          *q++ = ' ';
     }
     if (cfg_get_flag(label, "novideo")) {
          strcpy (q, "video=ofonly");
          q = strchr (q, 0);
          *q++ = ' ';
     }
     p = cfg_get_strg (label, "append");
     if (p) {
          strcpy (q, p);
          q = strchr (q, 0);
          *q++ = ' ';
     }
     *q = 0;
     if (params)
          strcpy(q, params);

     return buffer;
}

static int check_and_add_device(struct device *dev)
{
	if (!dev->icon_file)
		dev->icon_file = strdup(generic_icon_file(guess_device_type()));

	return !add_device(dev);
}

void process_image(char *label)
{
	struct boot_option opt;
	char *cfgopt;

	memset(&opt, 0, sizeof(opt));

	opt.name = label;
	cfgopt = cfg_get_strg(label, "image");
	opt.boot_image_file = join_paths(mountpoint, cfgopt);
	if (cfgopt == defimage)
		pb_log("This one is default. What do we do about it?\n");

	cfgopt = cfg_get_strg(label, "initrd");
	if (cfgopt)
		opt.initrd_file = join_paths(mountpoint, cfgopt);

	opt.boot_args = make_params(label, NULL);

	add_boot_option(&opt);

	if (opt.initrd_file)
		free(opt.initrd_file);
}

static int yaboot_parse(const char *devicepath, const char *_mountpoint)
{
	char *filepath;
	char *conf_file;
	char *tmpstr;
	ssize_t conf_len;
	int fd;
	struct stat st;
	char *label;

	mountpoint = _mountpoint;

	filepath = join_paths(mountpoint, "/etc/yaboot.conf");

	fd = open(filepath, O_RDONLY);
	if (fd < 0) {
		free(filepath);
		filepath = join_paths(mountpoint, "/yaboot.conf");
		fd = open(filepath, O_RDONLY);
		
		if (fd < 0)
			return 0;
	}

	if (fstat(fd, &st)) {
		close(fd);
		return 0;
	}

	conf_file = malloc(st.st_size+1);
	if (!conf_file) {
		close(fd);
		return 0;
	}
	
	conf_len = read(fd, conf_file, st.st_size);
	if (conf_len < 0) {
		close(fd);
		return 0;
	}
	conf_file[conf_len] = 0;

	close(fd);
	
	if (cfg_parse(filepath, conf_file, conf_len)) {
		pb_log("Error parsing yaboot.conf\n");
		return 0;
	}

	free(filepath);

	dev = malloc(sizeof(*dev));
	memset(dev, 0, sizeof(*dev));
	dev->id = strdup(devicepath);
	if (cfg_get_strg(0, "init-message")) {
		char *newline;
		dev->description = strdup(cfg_get_strg(0, "init-message"));
		newline = strchr(dev->description, '\n');
		if (newline)
			*newline = 0;
	}
	dev->icon_file = strdup(generic_icon_file(guess_device_type()));

	/* Mount the 'partition' which is what all the image filenames
	   are relative to */
	tmpstr = cfg_get_strg(0, "partition");
	if (tmpstr) {
		char *endp;
		int partnr = strtol(tmpstr, &endp, 10);
		if (endp != tmpstr && !*endp) {
			char *new_dev = malloc(strlen(devicepath) + strlen(tmpstr) + 1);
			if (!new_dev)
				return 0;

			strcpy(new_dev, devicepath);

			/* Strip digits (partition number) from string */
			endp = &new_dev[strlen(devicepath) - 1];
			while (isdigit(*endp))
				*(endp--) = 0;

			/* and add our own... */
			sprintf(endp+1, "%d", partnr);

			/* FIXME: udev may not have created the device node
			   yet. And on removal, unmount_device() only unmounts
			   it once, while in fact it may be mounted twice. */
			if (mount_device(new_dev, partition_mntpoint)) {
				pb_log("Error mounting image partition\n");
				return 0;
			}
			mountpoint = partition_mntpoint;
			dev->id = new_dev;
		}				
	}

	defimage = cfg_get_default();
	if (!defimage)
		return 0;
	defimage = cfg_get_strg(defimage, "image");

	label = cfg_next_image(NULL);
	if (!label || !check_and_add_device(dev))
		return 0;

	do {
		process_image(label);
	} while ((label = cfg_next_image(label)));

	return 1;
}

struct parser yaboot_parser = {
	.name = "yaboot.conf parser",
	.priority = 99,
	.parse	  = yaboot_parse
};
