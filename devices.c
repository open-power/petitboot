#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <asm/byteorder.h>

#include <libtwin/twin_png.h>
#include "petitboot.h"
#include "petitboot-paths.h"
#include "devices/message.h"

#define PBOOT_DEFAULT_ICON	"tux.png"

static const char *default_icon = artwork_pathname(PBOOT_DEFAULT_ICON);

struct discovery_context {
	/* nothing at present */
	int pad;
} _ctx;

struct device_context {
	struct discovery_context *discovery_ctx;
	uint8_t action;
	int device_idx;
};

static twin_pixmap_t *get_icon(const char *filename)
{
	/* todo: cache */
	twin_pixmap_t *icon;

	if (!filename)
		filename = default_icon;

retry:
	LOG("loading icon %s ... ", filename);
	icon = twin_png_to_pixmap(filename, TWIN_ARGB32);
	LOG("%s\n", icon ? "ok" : "failed");

	if (!icon && filename != default_icon) {
		filename = default_icon;
		LOG("reverting to default icon %s\n", filename);
		goto retry;
	}

	return icon;
}

#define MAX_LEN 4096
static char *read_string(int fd)
{
	int len;
	uint32_t len_buf;
	char *str, *pos;

	if (read(fd, &len_buf, sizeof(len_buf)) != sizeof(len_buf)) {
		perror("read");
		return NULL;
	}

	len = __be32_to_cpu(len_buf);
	if (len + 1 > MAX_LEN) {
		/* todo: truncate instead */
		return NULL;
	}

	pos = str = malloc(len + 1);

	while (len) {
		int rc = read(fd, pos, len);
		if (rc <= 0) {
			free(str);
			LOG("read failed: %s\n", strerror(errno));
			return NULL;
		}
		pos += rc;
		len -= rc;
	}
	*pos = '\0';

	return str;
}

static int _read_strings(int fd, void *ptr, int n_strings)
{
	char **strings = ptr;
	int i, ret = TWIN_TRUE;

	for (i = 0; i < n_strings; i++) {
		strings[i] = read_string(fd);
		if (!strings[i]) {
			ret = TWIN_FALSE;
			break;
		}
	}

	if (!ret)
		while (i-- > 0) {
			free(strings[i]);
			strings[i] = NULL;
		}

	return ret;
}

static void _free_strings(void *ptr, int n_strings)
{
	char **strings = ptr;
	int i;

	for (i = 0; i < n_strings; i++)
		free(strings[i]);

}

#define n_strings(x) (sizeof((x)) / sizeof(char *))
#define read_strings(f,x) _read_strings((f), &(x), n_strings(x))
#define free_strings(x) _free_strings(&(x), n_strings(x))

static int read_action(int fd, uint8_t *action)
{
	return read(fd, action, sizeof(*action)) != sizeof(*action);
}

static int read_device(int fd, struct device_context *dev_ctx)
{
	/* name, description, icon_file */
	struct device dev;
	twin_pixmap_t *icon;
	int index = -1;

	if (!read_strings(fd, dev))
		return TWIN_FALSE;

	LOG("got device: '%s'\n", dev.name);

	icon = get_icon(dev.icon_file);

	if (!icon)
		goto out;

	index = dev_ctx->device_idx = pboot_add_device(dev.id, dev.name, icon);

out:
	free_strings(dev);

	return index != -1;
}

static int read_option(int fd, struct device_context *dev_ctx)
{
	struct boot_option *opt = malloc(sizeof(*opt));
	twin_pixmap_t *icon;
	int index = -1;

	if (!opt)
		return TWIN_FALSE;

	if (!read_strings(fd, (*opt)))
		return TWIN_FALSE;

	LOG("got option: '%s'\n", opt->name);
	icon = get_icon(opt->icon_file);

	if (icon)
		index = pboot_add_option(dev_ctx->device_idx, opt->name,
					 opt->description, icon, opt);

	return index != -1;
}

static twin_bool_t pboot_proc_client_sock(int sock, twin_file_op_t ops,
		void *closure)
{
	struct device_context *dev_ctx = closure;
	uint8_t action;

	if (read_action(sock, &action))
		goto out_err;

	if (action == DEV_ACTION_ADD_DEVICE) {
		if (!read_device(sock, dev_ctx))
			goto out_err;

	} else if (action == DEV_ACTION_ADD_OPTION) {
		if (dev_ctx->device_idx == -1) {
			LOG("option, but no device has been sent?\n");
			goto out_err;
		}

		if (!read_option(sock, dev_ctx))
			goto out_err;

	} else if (action == DEV_ACTION_REMOVE_DEVICE) {
		char *dev_id = read_string(sock);
		if (!dev_id)
			goto out_err;
		
		LOG("remove device %s\n", dev_id);
		pboot_remove_device(dev_id);

	} else {
		LOG("unsupported action %d\n", action);
		goto out_err;
	}

	return TWIN_TRUE;

out_err:
	close(sock);
	return TWIN_FALSE;
}

static twin_bool_t pboot_proc_server_sock(int sock, twin_file_op_t ops,
		void *closure)
{
	int fd;
	struct discovery_context *disc_ctx = closure;
	struct device_context *dev_ctx;

	fd = accept(sock, NULL, 0);
	if (fd < 0) {
		LOG("accept failed: %s", strerror(errno));
		return TWIN_FALSE;
	}

	dev_ctx = malloc(sizeof(*dev_ctx));
	dev_ctx->discovery_ctx = disc_ctx;
	dev_ctx->device_idx = -1;
	dev_ctx->action = 0xff;

	twin_set_file(pboot_proc_client_sock, fd, TWIN_READ, dev_ctx);

	return TWIN_TRUE;
}

int pboot_start_device_discovery(int udev_trigger)
{
	int sock;
	struct sockaddr_un addr;

	unlink(PBOOT_DEVICE_SOCKET);

	sock = socket(PF_UNIX, SOCK_STREAM, 0);
	if (sock < 0) {
		perror("socket");
		return TWIN_FALSE;
	}

	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, PBOOT_DEVICE_SOCKET);

	if (bind(sock, (struct sockaddr *)&addr, sizeof(addr))) {
		LOG("can't bind to %s: %s", addr.sun_path, strerror(errno));
		return TWIN_FALSE;
	}

	if (listen(sock, 1)) {
		LOG("can't listen on socket %s: %s",
				addr.sun_path, strerror(errno));
		return TWIN_FALSE;
	}

	LOG("listening on %s\n", addr.sun_path);

	twin_set_file(pboot_proc_server_sock, sock, TWIN_READ, &_ctx);

	if (udev_trigger) {
		int rc = system("udevtrigger");
		if (rc)
			LOG("udevtrigger failed, rc %d\n", rc);
	}

	return TWIN_TRUE;
}

void pboot_exec_option(void *data)
{
	struct boot_option *opt = data;
	char *kexec_opts[10];
	int nr_opts = 2;

	kexec_opts[0] = "/sbin/kexec";
	kexec_opts[1] = "-f";
	if (opt->initrd_file) {
		kexec_opts[nr_opts] = malloc(10 + strlen(opt->initrd_file));
		sprintf(kexec_opts[nr_opts], "--initrd=%s", opt->initrd_file);
		nr_opts++;
	}
	if (opt->boot_args)  {
		kexec_opts[nr_opts] = malloc(10 + strlen(opt->boot_args));
		sprintf(kexec_opts[nr_opts], "--command-line=%s",
			opt->boot_args);
		nr_opts++;
	}

	kexec_opts[nr_opts++] = opt->boot_image_file;
	kexec_opts[nr_opts] = NULL;
	execv(kexec_opts[0], kexec_opts);
}
