
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/un.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <asm/byteorder.h>
#include <linux/cdrom.h>
#include <sys/ioctl.h>

#include "parser.h"
#include "paths.h"
#include "petitboot-paths.h"

/* Define below to operate without the frontend */
#undef USE_FAKE_SOCKET

/* Delay in seconds between polling of removable devices */
#define REMOVABLE_SLEEP_DELAY	2

static FILE *logf;
static int sock;

void pb_log(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vfprintf(logf, fmt, ap);
	va_end(ap);
}

static void print_boot_option(const struct boot_option *opt)
{
	pb_log("\tname: %s\n", opt->name);
	pb_log("\tdescription: %s\n", opt->description);
	pb_log("\tboot_image: %s\n", opt->boot_image_file);
	pb_log("\tinitrd: %s\n", opt->initrd_file);
	pb_log("\tboot_args: %s\n", opt->boot_args);

}

static void print_device(const struct device *dev)
{
	pb_log("\tid: %s\n", dev->id);
	pb_log("\tname: %s\n", dev->name);
	pb_log("\tdescription: %s\n", dev->description);
	pb_log("\tboot_image: %s\n", dev->icon_file);
}

static int write_action(int fd, enum device_action action)
{
	uint8_t action_buf = action;
	return write(fd, &action_buf, sizeof(action_buf)) != sizeof(action_buf);
}

static int write_string(int fd, const char *str)
{
	int len, pos = 0;
	uint32_t len_buf;

	if (!str) {
		len_buf = 0;
		if (write(fd, &len_buf, sizeof(len_buf)) != sizeof(len_buf)) {
			pb_log("write failed: %s\n", strerror(errno));
			return -1;
		}
		return 0;
	}

	len = strlen(str);
	if (len > (1ull << (sizeof(len_buf) * 8 - 1))) {
		pb_log("string too large\n");
		return -1;
	}

	len_buf = __cpu_to_be32(len);
	if (write(fd, &len_buf, sizeof(len_buf)) != sizeof(len_buf)) {
		pb_log("write failed: %s\n", strerror(errno));
		return -1;
	}

	while (pos < len) {
		int rc = write(fd, str, len - pos);
		if (rc <= 0) {
			pb_log("write failed: %s\n", strerror(errno));
			return -1;
		}
		pos += rc;
		str += rc;
	}

	return 0;
}

int add_device(const struct device *dev)
{
	int rc;

	pb_log("device added:\n");
	print_device(dev);
	rc = write_action(sock, DEV_ACTION_ADD_DEVICE) ||
		write_string(sock, dev->id) ||
		write_string(sock, dev->name) ||
		write_string(sock, dev->description) ||
		write_string(sock, dev->icon_file);

	if (rc)
		pb_log("error writing device %s to socket\n", dev->name);

	return rc;
}

int add_boot_option(const struct boot_option *opt)
{
	int rc;

	pb_log("boot option added:\n");
	print_boot_option(opt);

	rc = write_action(sock, DEV_ACTION_ADD_OPTION) ||
		write_string(sock, opt->id) ||
		write_string(sock, opt->name) ||
		write_string(sock, opt->description) ||
		write_string(sock, opt->icon_file) ||
		write_string(sock, opt->boot_image_file) ||
		write_string(sock, opt->initrd_file) ||
		write_string(sock, opt->boot_args);

	if (rc)
		pb_log("error writing boot option %s to socket\n", opt->name);

	return rc;
}

int remove_device(const char *dev_path)
{
	return write_action(sock, DEV_ACTION_REMOVE_DEVICE) ||
		write_string(sock, dev_path);
}

int connect_to_socket()
{
#ifndef USE_FAKE_SOCKET
	int fd;
	struct sockaddr_un addr;

	fd = socket(PF_UNIX, SOCK_STREAM, 0);
	if (fd == -1) {
		pb_log("can't create socket: %s\n", strerror(errno));
		return -1;
	}

	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, PBOOT_DEVICE_SOCKET);

	if (connect(fd, (struct sockaddr *)&addr, sizeof(addr))) {
		pb_log("can't connect to %s: %s\n",
				addr.sun_path, strerror(errno));
		return -1;
	}
	sock = fd;

	return 0;
#else
	int fd;
	fd = open("./debug_socket", O_WRONLY | O_CREAT, 0640);
	if (fd < 0) {
		pb_log("can't create output file: %s\n", strerror(errno));
		return -1;
	}
	sock = fd;
	return 0;
#endif
}

static int mkdir_recursive(const char *dir)
{
	char *str, *sep;
	int mode = 0755;
	struct stat statbuf;

	pb_log("mkdir_recursive(%s)\n", dir);

	if (!*dir)
		return 0;

	if (!stat(dir, &statbuf)) {
		if (!S_ISDIR(statbuf.st_mode)) {
			pb_log("%s: %s exists, but isn't a directory\n",
					__func__, dir);
			return -1;
		}
		return 0;
	}

	str = strdup(dir);
	sep = strchr(*str == '/' ? str + 1 : str, '/');

	while (1) {

		/* terminate the path at sep */
		if (sep)
			*sep = '\0';
		pb_log("mkdir(%s)\n", str);

		if (mkdir(str, mode) && errno != EEXIST) {
			pb_log("mkdir(%s): %s\n", str, strerror(errno));
			return -1;
		}

		if (!sep)
			break;

		/* reset dir to the full path */
		strcpy(str, dir);
		sep = strchr(sep + 1, '/');
	}

	free(str);

	return 0;
}

static void setup_device_links(const char *device)
{
	struct link {
		char *env, *dir;
	} *link, links[] = {
		{
			.env = "ID_FS_UUID",
			.dir = "disk/by-uuid"
		},
		{
			.env = "ID_FS_LABEL",
			.dir = "disk/by-label"
		},
		{
			.env = NULL
		}
	};

	for (link = links; link->env; link++) {
		char *value, *dir, *path;

		value = getenv(link->env);
		if (!value)
			continue;

		value = encode_label(value);
		dir = join_paths(TMP_DIR, link->dir);
		path = join_paths(dir, value);

		if (!mkdir_recursive(dir)) {
			unlink(path);
			if (symlink(mountpoint_for_device(device), path)) {
				pb_log("symlink(%s): %s\n",
						path, strerror(errno));
			}
		}

		free(path);
		free(dir);
		free(value);
	}
}

int mount_device(const char *dev_path)
{
	const char *dir;
	int pid, status, rc = -1;
	struct stat statbuf;

	dir = mountpoint_for_device(dev_path);

	if (stat(dir, &statbuf)) {
		if (mkdir(dir, 0755)) {
			pb_log("couldn't create directory %s: %s\n",
					dir, strerror(errno));
			goto out;
		}
	} else {
		if (!S_ISDIR(statbuf.st_mode)) {
			pb_log("mountpoint %s exists, "
					"but isn't a directory\n", dir);
			goto out;
		}
	}


	pid = fork();
	if (pid == -1) {
		pb_log("%s: fork failed: %s\n", __FUNCTION__, strerror(errno));
		goto out;
	}

	if (pid == 0) {
		execl(MOUNT_BIN, MOUNT_BIN, dev_path, dir, "-o", "ro", NULL);
		exit(EXIT_FAILURE);
	}

	if (waitpid(pid, &status, 0) == -1) {
		pb_log("%s: waitpid failed: %s\n", __FUNCTION__,
				strerror(errno));
		goto out;
	}

	if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
		setup_device_links(dev_path);
		rc = 0;
	}

out:
	return rc;
}

static int unmount_device(const char *dev_path)
{
	int pid, status, rc;

	pid = fork();

	if (pid == -1) {
		pb_log("%s: fork failed: %s\n", __FUNCTION__, strerror(errno));
		return -1;
	}

	if (pid == 0) {
		execl(UMOUNT_BIN, UMOUNT_BIN, dev_path, NULL);
		exit(EXIT_FAILURE);
	}

	if (waitpid(pid, &status, 0) == -1) {
		pb_log("%s: waitpid failed: %s\n", __FUNCTION__,
				strerror(errno));
		return -1;
	}

	rc = !WIFEXITED(status) || WEXITSTATUS(status) != 0;

	return rc;
}

static const struct device fake_boot_devices[] =
{
	{
		.id		= "fakeDisk0",
		.name		= "Hard Disk",
		.icon_file	= artwork_pathname("hdd.png"),
	},
	{
		.id		= "fakeDisk1",
		.name		= "PinkCat Linux CD",
		.icon_file	= artwork_pathname("cdrom.png"),
	}
};

static const struct boot_option fake_boot_options[] =
{
	{
		.id		= "fakeBoot0",
		.name		= "Bloobuntu Linux",
		.description	= "Boot Bloobuntu Linux",
		.icon_file	= artwork_pathname("hdd.png"),
	},
	{
		.id		= "fakeBoot1",
		.name		= "Pendora Gore 6",
		.description	= "Boot Pendora Gora 6",
		.icon_file	= artwork_pathname("hdd.png"),
	},
	{
		.id		= "fakeBoot2",
		.name		= "Genfoo Minux",
		.description	= "Boot Genfoo Minux",
		.icon_file	= artwork_pathname("hdd.png"),
	},
	{
		.id		= "fakeBoot3",
		.name		= "PinkCat Linux",
		.description	= "Install PinkCat Linux - Graphical install",
		.icon_file	= artwork_pathname("cdrom.png"),
	},
};

enum generic_icon_type guess_device_type(void)
{
	const char *type = getenv("ID_TYPE");
	const char *bus = getenv("ID_BUS");
	if (type && streq(type, "cd"))
		return ICON_TYPE_OPTICAL;
	if (!bus)
		return ICON_TYPE_UNKNOWN;
	if (streq(bus, "usb"))
		return ICON_TYPE_USB;
	if (streq(bus, "ata") || streq(bus, "scsi"))
		return ICON_TYPE_DISK;
	return ICON_TYPE_UNKNOWN;
}


static int is_removable_device(const char *sysfs_path) 
{
	char full_path[PATH_MAX];
	char buf[80];
	int fd, buf_len;

	sprintf(full_path, "/sys/%s/removable", sysfs_path);	
	fd = open(full_path, O_RDONLY);
	pb_log(" -> removable check on %s, fd=%d\n", full_path, fd);
	if (fd < 0)
		return 0;
	buf_len = read(fd, buf, 79);
	close(fd);
	if (buf_len < 0)
		return 0;
	buf[buf_len] = 0;
	return strtol(buf, NULL, 10);
}

static int is_ignored_device(const char *devname)
{
	static const char *ignored_devices[] =
		{ "/dev/ram", "/dev/loop", NULL };
	const char **dev;

	for (dev = ignored_devices; *dev; dev++)
		if (!strncmp(devname, *dev, strlen(*dev)))
			return 1;

	return 0;
}

static int found_new_device(const char *dev_path)
{
	const char *mountpoint = mountpoint_for_device(dev_path);

	if (mount_device(dev_path)) {
		pb_log("failed to mount %s\n", dev_path);
		return EXIT_FAILURE;
	}

	pb_log("mounted %s at %s\n", dev_path, mountpoint);

	iterate_parsers(dev_path, mountpoint);

	return EXIT_SUCCESS;
}

static void detach_and_sleep(int sec)
{
	static int forked = 0;
	int rc = 0;

	if (sec <= 0)
		return;

	if (!forked) {
		pb_log("running in background...");
		rc = fork();
		forked = 1;
	}

	if (rc == 0) {
		sleep(sec);

	} else if (rc == -1) {
		perror("fork()");
		exit(EXIT_FAILURE);
	} else {
		exit(EXIT_SUCCESS);
	}
}

static int poll_device_plug(const char *dev_path,
			    int *optical)
{
	int rc, fd;

	/* Polling loop for optical drive */
	for (; (*optical) != 0; ) {
		fd = open(dev_path, O_RDONLY|O_NONBLOCK);
		if (fd < 0)
			return EXIT_FAILURE;
		rc = ioctl(fd, CDROM_DRIVE_STATUS, CDSL_CURRENT);
		close(fd);
		if (rc == -1)
			break;

		*optical = 1;
		if (rc == CDS_DISC_OK)
			return EXIT_SUCCESS;

		detach_and_sleep(REMOVABLE_SLEEP_DELAY);
	}

	/* Fall back to bare open() */
	*optical = 0;
	for (;;) {
		fd = open(dev_path, O_RDONLY);
		if (fd < 0 && errno != ENOMEDIUM)
			return EXIT_FAILURE;
		close(fd);
		if (fd >= 0)
			return EXIT_SUCCESS;
		detach_and_sleep(REMOVABLE_SLEEP_DELAY);
	}
}

static int poll_device_unplug(const char *dev_path, int optical)
{
	int rc, fd;

	for (;optical;) {
		fd = open(dev_path, O_RDONLY|O_NONBLOCK);
		if (fd < 0)
			return EXIT_FAILURE;
		rc = ioctl(fd, CDROM_DRIVE_STATUS, CDSL_CURRENT);
		close(fd);
		if (rc != CDS_DISC_OK)
			return EXIT_SUCCESS;
		detach_and_sleep(REMOVABLE_SLEEP_DELAY);
	}

	/* Fall back to bare open() */
	for (;;) {
		fd = open(dev_path, O_RDONLY);
		if (fd < 0 && errno != ENOMEDIUM)
			return EXIT_FAILURE;
		close(fd);
		if (fd < 0)
			return EXIT_SUCCESS;
		detach_and_sleep(REMOVABLE_SLEEP_DELAY);
	}
}

static int poll_removable_device(const char *sysfs_path,
				 const char *dev_path)
{
	int rc, mounted, optical = -1;
       
	for (;;) {
		rc = poll_device_plug(dev_path, &optical);
		if (rc == EXIT_FAILURE)
			return rc;
		rc = found_new_device(dev_path);
		mounted = (rc == EXIT_SUCCESS);

		poll_device_unplug(dev_path, optical);

		remove_device(dev_path);

		/* Unmount it repeatedly, if needs be */
		while (mounted && !unmount_device(dev_path))
			;
		detach_and_sleep(1);
	}
}

int main(int argc, char **argv)
{
	char *dev_path, *action;
	int rc;

	action = getenv("ACTION");

	logf = fopen("/var/log/petitboot-udev-helpers.log", "a");
	if (!logf)
		logf = stdout;
	pb_log("%d started\n", getpid());
	rc = EXIT_SUCCESS;

	if (!action) {
		pb_log("missing environment?\n");
		return EXIT_FAILURE;
	}

	set_mount_base(TMP_DIR);

	if (connect_to_socket())
		return EXIT_FAILURE;

	if (streq(action, "fake")) {
		pb_log("fake mode");

		add_device(&fake_boot_devices[0]);
		add_boot_option(&fake_boot_options[0]);
		add_boot_option(&fake_boot_options[1]);
		add_boot_option(&fake_boot_options[2]);
		add_device(&fake_boot_devices[1]);
		add_boot_option(&fake_boot_options[3]);

		return EXIT_SUCCESS;
	}

	dev_path = getenv("DEVNAME");
	if (!dev_path) {
		pb_log("missing environment?\n");
		return EXIT_FAILURE;
	}

	if (is_ignored_device(dev_path))
		return EXIT_SUCCESS;

	if (streq(action, "add")) {
		char *sysfs_path = getenv("DEVPATH");
		if (sysfs_path && is_removable_device(sysfs_path))
			rc = poll_removable_device(sysfs_path, dev_path);
		else
			rc = found_new_device(dev_path);
	} else if (streq(action, "remove")) {
		pb_log("%s removed\n", dev_path);

		remove_device(dev_path);

		/* Unmount it repeatedly, if needs be */
		while (!unmount_device(dev_path))
			;

	} else {
		pb_log("invalid action '%s'\n", action);
		rc = EXIT_FAILURE;
	}
	return rc;
}
