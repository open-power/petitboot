
#include <stdio.h>
#include <stdlib.h>
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

#include "udev-helper.h"

#define parser_dir "."

#define tmp_dir "/var/tmp/petitboot"
#define socket_file "/var/tmp/petitboot-dev"
#define mount_bin "/bin/mount"
#define umount_bin "/bin/umount"

extern struct parser native_parser;
static FILE *logf;
static int sock;

/* array of parsers, ordered by priority */
static struct parser *parsers[] = {
	&native_parser,
	NULL
};

#define log(...) fprintf(logf, __VA_ARGS__)

static void iterate_parsers(const char *devpath, const char *mountpoint)
{
	int i;

	log("trying parsers for %s@%s\n", devpath, mountpoint);

	for (i = 0; parsers[i]; i++) {
		log("\ttrying parser '%s'\n", parsers[i]->name);
		/* just use a dummy device path for now */
		if (parsers[i]->parse(devpath, mountpoint))
			return;
	}
	log("\tno boot_options found\n");
}

static void print_boot_option(const struct boot_option *opt)
{
	log("\tname: %s\n", opt->name);
	log("\tdescription: %s\n", opt->description);
	log("\tboot_image: %s\n", opt->boot_image_file);
	log("\tboot_args: %s\n", opt->boot_args);

}

static void print_device(const struct device *dev)
{
	log("\tid: %s\n", dev->name);
	log("\tname: %s\n", dev->name);
	log("\tdescription: %s\n", dev->description);
	log("\tboot_image: %s\n", dev->icon_file);
}


void free_device(struct device *dev)
{
	if (!dev)
		return;
	if (dev->id)
		free(dev->id);
	if (dev->name)
		free(dev->name);
	if (dev->description)
		free(dev->description);
	if (dev->icon_file)
		free(dev->icon_file);
	free(dev);
}

void free_boot_option(struct boot_option *opt)
{
	if (!opt)
		return;
	if (opt->name)
		free(opt->name);
	if (opt->description)
		free(opt->description);
	if (opt->icon_file)
		free(opt->icon_file);
	if (opt->boot_image_file)
		free(opt->boot_image_file);
	if (opt->initrd_file)
		free(opt->initrd_file);
	if (opt->boot_args)
		free(opt->boot_args);
	free(opt);
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
			log("write failed: %s\n", strerror(errno));
			return -1;
		}
		return 0;
	}

	len = strlen(str);
	if (len > (1ull << (sizeof(len_buf) * 8 - 1))) {
		log("string too large\n");
		return -1;
	}

	len_buf = __cpu_to_be32(len);
	if (write(fd, &len_buf, sizeof(len_buf)) != sizeof(len_buf)) {
		log("write failed: %s\n", strerror(errno));
		return -1;
	}

	while (pos < len) {
		int rc = write(fd, str, len - pos);
		if (rc <= 0) {
			log("write failed: %s\n", strerror(errno));
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

	log("device added:\n");
	print_device(dev);
	rc = write_action(sock, DEV_ACTION_ADD_DEVICE) ||
		write_string(sock, dev->id) ||
		write_string(sock, dev->name) ||
		write_string(sock, dev->description) ||
		write_string(sock, dev->icon_file);

	if (rc)
		log("error writing device %s to socket\n", dev->name);

	return rc;
}

int add_boot_option(const struct boot_option *opt)
{
	int rc;

	log("boot option added:\n");
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
		log("error writing boot option %s to socket\n", opt->name);

	return rc;
}

int remove_device(const char *dev_path)
{
	return write_action(sock, DEV_ACTION_REMOVE_DEVICE) ||
		write_string(sock, dev_path);
}

int connect_to_socket()
{
#if 1
	int fd;
	struct sockaddr_un addr;

	fd = socket(PF_UNIX, SOCK_STREAM, 0);
	if (fd == -1) {
		log("can't create socket: %s\n", strerror(errno));
		return -1;
	}

	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, socket_file);

	if (connect(fd, (struct sockaddr *)&addr, sizeof(addr))) {
		log("can't connect to %s: %s\n",
				addr.sun_path, strerror(errno));
		return -1;
	}
	sock = fd;

	return 0;
#else
	int fd;
	fd = open("./debug_socket", O_WRONLY | O_CREAT, 0640);
	if (fd < 0) {
		log("can't create output file: %s\n", strerror(errno));
		return -1;
	}
	sock = fd;
	return 0;
#endif
}

#define template "mnt-XXXXXX"

static int mount_device(const char *dev_path, char *mount_path)
{
	char *dir;
	int pid, status, rc = -1;

	/* create a unique mountpoint */
	dir = malloc(strlen(tmp_dir) + 2 + strlen(template));
	sprintf(dir, "%s/%s", tmp_dir, template);

	if (!mkdtemp(dir)) {
		log("failed to create temporary directory in %s: %s",
				tmp_dir, strerror(errno));
		goto out;
	}

	pid = fork();
	if (pid == -1) {
		log("%s: fork failed: %s\n", __FUNCTION__, strerror(errno));
		goto out;
	}

	if (pid == 0) {
		execl(mount_bin, mount_bin, dev_path, dir, "-o", "ro", NULL);
		exit(EXIT_FAILURE);
	}

	if (waitpid(pid, &status, 0) == -1) {
		log("%s: waitpid failed: %s\n", __FUNCTION__, strerror(errno));
		goto out;
	}

	if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
		strcpy(mount_path, dir);
		rc = 0;
	}

out:
	free(dir);
	return rc;
}

static int unmount_device(const char *dev_path)
{
	int pid, status, rc;

	pid = fork();

	if (pid == -1) {
		log("%s: fork failed: %s\n", __FUNCTION__, strerror(errno));
		return -1;
	}

	if (pid == 0) {
		execl(umount_bin, umount_bin, dev_path, NULL);
		exit(EXIT_FAILURE);
	}

	if (waitpid(pid, &status, 0) == -1) {
		log("%s: waitpid failed: %s\n", __FUNCTION__, strerror(errno));
		return -1;
	}

	rc = !WIFEXITED(status) || WEXITSTATUS(status) != 0;

	return rc;
}

const char *generic_icon_file(enum generic_icon_type type)
{
	switch (type) {
	case ICON_TYPE_DISK:
		return "artwork/hdd.png";
	case ICON_TYPE_USB:
		return "artwork/usbpen.png";
	case ICON_TYPE_OPTICAL:
		return "artwork/cdrom.png";
	case ICON_TYPE_NETWORK:
	case ICON_TYPE_UNKNOWN:
		break;
	}
	return "artwork/hdd.png";
}

static const struct device fake_boot_devices[] =
{
	{
		.id		= "fakeDisk0",
		.name		= "Hard Disk",
		.icon_file	= "artwork/hdd.png",
	},
	{
		.id		= "fakeDisk1",
		.name		= "PinkCat Linux CD",
		.icon_file	= "artwork/cdrom.png",
	}
};

static const struct boot_option fake_boot_options[] =
{
	{
		.id		= "fakeBoot0",
		.name		= "Bloobuntu Linux",
		.description	= "Boot Bloobuntu Linux",
		.icon_file	= "artwork/hdd.png",
	},
	{
		.id		= "fakeBoot1",
		.name		= "Pendora Gore 6",
		.description	= "Boot Pendora Gora 6",
		.icon_file	= "artwork/hdd.png",
	},
	{
		.id		= "fakeBoot2",
		.name		= "Genfoo Minux",
		.description	= "Boot Genfoo Minux",
		.icon_file	= "artwork/hdd.png",
	},
	{
		.id		= "fakeBoot3",
		.name		= "PinkCat Linux",
		.description	= "Install PinkCat Linux - Graphical install",
		.icon_file	= "artwork/cdrom.png",
	},
};

enum generic_icon_type guess_device_type(void)
{
	const char *bus = getenv("ID_BUS");
	if (streq(bus, "usb"))
		return ICON_TYPE_USB;
	if (streq(bus, "ata") || streq(bus, "scsi"))
		return ICON_TYPE_DISK;
	return ICON_TYPE_UNKNOWN;
}

int main(int argc, char **argv)
{
	char mountpoint[PATH_MAX];
	char *dev_path, *action;
	int rc;

	/*if (fork())
		return EXIT_SUCCESS;
		*/
	action = getenv("ACTION");

	logf = stdout;
	rc = EXIT_SUCCESS;

	if (!action) {
		log("missing environment?\n");
		return EXIT_FAILURE;
	}

	if (connect_to_socket())
		return EXIT_FAILURE;

	if (streq(action, "fake")) {
		log("fake mode");

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
		log("missing environment?\n");
		return EXIT_FAILURE;
	}

	if (streq(action, "add")) {
		if (mount_device(dev_path, mountpoint)) {
			log("failed to mount %s\n", dev_path);
			return EXIT_FAILURE;
		}

		log("mounted %s at %s\n", dev_path, mountpoint);

		iterate_parsers(dev_path, mountpoint);

	} else if (streq(action, "remove")) {
		log("%s removed\n", dev_path);

		remove_device(dev_path);

		unmount_device(dev_path);

	} else {
		log("invalid action '%s'\n", action);
		rc = EXIT_FAILURE;
	}
	return rc;
}
