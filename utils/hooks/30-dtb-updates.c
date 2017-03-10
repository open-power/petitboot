
#define _GNU_SOURCE

#include <stdlib.h>
#include <err.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/ioctl.h>
#include <string.h>
#include <stdbool.h>
#include <inttypes.h>
#include <errno.h>

#include <linux/fb.h>

#include <libfdt.h>

#include <file/file.h>
#include <talloc/talloc.h>

static const char *fbdev_name = "fb0";

#define MAX_N_CELLS		4
#define ADDRESS_PROP_SIZE	4096

struct offb_ctx {
	const char			*dtb_name;
	void				*dtb;
	int				dtb_node;
	const char			*path;
	struct fb_fix_screeninfo	fscreeninfo;
	struct fb_var_screeninfo	vscreeninfo;
};

static int load_dtb(struct offb_ctx *ctx)
{
	char *buf;
	int len;
	int rc;

	rc = read_file(ctx, ctx->dtb_name, &buf, &len);
	if (rc) {
		warn("error reading %s", ctx->dtb_name);
		return rc;
	}

	rc = fdt_check_header(buf);
	if (rc || (int)fdt_totalsize(buf) > len) {
		warnx("invalid dtb: %s (rc %d)", ctx->dtb_name, rc);
		return -1;
	}

	len = fdt_totalsize(buf) + ADDRESS_PROP_SIZE;

	ctx->dtb = talloc_array(ctx, char, len);
	if (!ctx->dtb) {
		warn("Failed to allocate space for dtb\n");
		return -1;
	}
	fdt_open_into(buf, ctx->dtb, len);

	return 0;
}

static int fbdev_sysfs_lookup(struct offb_ctx *ctx)
{
	char *path, *linkpath, *nodepath;
	int fd, node;
	ssize_t rc __attribute__((unused));

	path = talloc_asprintf(ctx, "/sys/class/graphics/%s", fbdev_name);
	if (!path) {
		warn("Failed to allocate space for sysfs path\n");
		return -1;
	}

	fd = open(path, O_RDONLY | O_DIRECTORY);
	if (fd < 0) {
		warn("Can't open device %s in sysfs", fbdev_name);
		return -1;
	}

	linkpath = talloc_zero_array(ctx, char, PATH_MAX + 1);
	if (!linkpath) {
		warn("Failed to allocate space for link path\n");
		return -1;
	}

	rc = readlinkat(fd, "device/of_node", linkpath, PATH_MAX);
	if (rc < 0) {
		warn("Can't read of_node link for device %s", fbdev_name);
		return -1;
	}

	/* readlinkat() returns a relative path such as:
	 *
	 *  ../../../../../../../firmware/devicetree/base/pciex@n/…/vga@0
	 *
	 * We only need the path component from the device tree itself; so
	 * strip everything before /firmware/devicetree/base
	 */
	nodepath = strstr(linkpath, "/firmware/devicetree/base/");
	if (!nodepath) {
		warnx("Can't resolve device tree link for device %s",
				fbdev_name);
		return -1;
	}

	nodepath += strlen("/firmware/devicetree/base");

	node = fdt_path_offset(ctx->dtb, nodepath);
	if (node < 0) {
		warnx("Can't find node %s in device tree: %s",
				nodepath, fdt_strerror(node));
		return -1;
	}

	ctx->path = nodepath;
	ctx->dtb_node = node;

	return 0;
}

static int fbdev_device_query(struct offb_ctx *ctx)
{
	int fd, rc = -1;
	char *path;

	path = talloc_asprintf(ctx, "/dev/%s", fbdev_name);
	if (!path) {
		warn("Failed to allocate space for device path\n");
		return -1;
	}

	fd = open(path, O_RDWR);
	if (fd < 0) {
		warn("Can't open fb device %s", path);
		return -1;
	}

	rc = ioctl(fd, FBIOGET_VSCREENINFO, &ctx->vscreeninfo);
	if (rc) {
		warn("ioctl(FBIOGET_VSCREENINFO) failed");
		goto out;
	}

	rc = ioctl(fd, FBIOGET_FSCREENINFO, &ctx->fscreeninfo);
	if (rc) {
		warn("ioctl(FBIOGET_FSCREENINFO) failed");
		goto out;
	}

	fprintf(stderr, "Retrieved framebuffer details:\n");
	fprintf(stderr, "device %s:\n", fbdev_name);
	fprintf(stderr, "  addr: %lx\n", ctx->fscreeninfo.smem_start);
	fprintf(stderr, "   len: %" PRIu32 "\n", ctx->fscreeninfo.smem_len);
	fprintf(stderr, "  line: %d\n", ctx->fscreeninfo.line_length);
	fprintf(stderr, "   res:  %dx%d@%d\n", ctx->vscreeninfo.xres,
			ctx->vscreeninfo.yres,
			ctx->vscreeninfo.bits_per_pixel);

	rc = 0;

out:
	close(fd);
	return rc;
}

static char *next_dt_name(struct offb_ctx *ctx, const char **path)
{
	const char *c, *p;
	char *name;

	p = *path;

	if (p[0] == '/')
		p++;

	if (p[0] == '\0')
		return NULL;

	c = strchrnul(p, '/');

	name = talloc_strndup(ctx, p, c - p);

	*path = c;

	return name;
}

static uint64_t of_read_number(const fdt32_t *data, int n)
{
	uint64_t x;

	x = fdt32_to_cpu(data[0]);
	if (n > 1) {
		x <<= 32;
		x |= fdt32_to_cpu(data[1]);
	}
	return x;
}

/* Do a single translation across a PCI bridge. This results in either;
 * - Translating a 2-cell CPU address into a 3-cell PCI address, or
 * - Translating a 3-cell PCI address into a 3-cell PCI address with a
 *   different offset.
 *
 * To simplify translation we make some assumptions about addresses:
 * Addresses are either 3 or 2 cells wide
 * Size is always 2 cells wide
 * The first cell of a 3 cell address is the PCI memory type
 */
static int do_translate(void *fdt, int node,
		const fdt32_t *ranges, int range_size,
		uint32_t *addr, uint32_t *size,
		int *addr_cells, int *size_cells)
{
	uint64_t addr_current_base, addr_child_base, addr_size;
	uint64_t addr_current, offset, new_addr;
	uint64_t current_pci_flags, child_pci_flags;
	int i, na, ns, cna, cns, prop_len;
	const fdt32_t *prop;
	const char *type;
	bool pci = false;

	type = fdt_getprop(fdt, node, "device_type", NULL);
	pci = type && (!strcmp(type, "pci") || !strcmp(type, "pciex"));

	/* We don't translate at vga@0, so we should always see a pci or
	 * pciex device_type */
	if (!pci)
		return -1;

	if (range_size == 0) {
		fprintf(stderr, "Empty ranges property, 1:1 translation\n");
		return 0;
	}

	/* Number of cells for address and size at current level */
	na = *addr_cells;
	ns = *size_cells;

	/* Number of cells for address and size at child level */
	prop = fdt_getprop(fdt, node, "#address-cells", &prop_len);
	cna = prop ? fdt32_to_cpu(*prop) : 2;
	prop = fdt_getprop(fdt, node, "#size-cells", &prop_len);
	cns = prop ? fdt32_to_cpu(*prop) : 2;

	/* We're translating back to a PCI address, so the size should grow */
	if (na > cna) {
		fprintf(stderr, "na > cna, unexpected\n");
		return -1;
	}

	/* If the current address is a PCI address, its type should match the
	 * type of every subsequent child address */
	current_pci_flags = na > 2 ? of_read_number(addr, 1) : 0;
	child_pci_flags = cna > 2 ? of_read_number(ranges, 1) : 0;
	if (current_pci_flags != 0 && current_pci_flags != child_pci_flags) {
		fprintf(stderr, "Unexpected change in flags: %lx, %lx\n",
			current_pci_flags, child_pci_flags);
		return -1;
	}

	if (ns != cns) {
		fprintf(stderr, "Unexpected change in #size-cells: %d vs %d\n",
			ns, cns);
		return -1;
	}

	/*
	 * The ranges property is of the form
	 *	< upstream addr base > < downstream addr base > < size >
	 * The current address stored in addr is similarly of the form
	 *	< current address > < size >
	 * Where either address base and the current address can be a 2-cell
	 * CPU address or a 3-cell PCI address.
	 *
	 * For PCI addresses ignore the type flag in the first cell and use the
	 * 64-bit address in the remaining 2 cells.
	 */
	if (na > 2) {
		addr_current_base =  of_read_number(ranges + cna + 1, na - 1);
		addr_current =  of_read_number(addr + 1, na - 1);
	} else {
		addr_current_base =  of_read_number(ranges + cna, na);
		addr_current =  of_read_number(addr, na);
	}
	if (cna > 2)
		addr_child_base =  of_read_number(ranges + 1, cna - 1);
	else
		addr_child_base =  of_read_number(ranges, cna);

	/*
	 * Perform the actual translation. Find the offset of the current
	 * address from the upstream base, and add the offset to the
	 * downstream base to find the new address.
	 * The new address will be cna-cells wide, inheriting child_pci_flags
	 * as the memory type.
	 */
	addr_size = of_read_number(size, ns);
	offset = addr_current - addr_current_base;
	new_addr = addr_child_base + offset;

	memset(addr, 0, *addr_cells);
	memset(size, 0, *size_cells);
	*addr_cells = cna;
	*size_cells = cns;

	/* Update the current address in addr.
	 * It's highly unlikely any translation will leave us with a 2-cell
	 * CPU address, but for completeness only include PCI flags if the
	 * child offset was definitely a PCI address */
	if (*addr_cells > 2)
		addr[0] = cpu_to_fdt32(child_pci_flags);
	for (i = *addr_cells - 1; i >= *addr_cells - 2; i--) {
		addr[i] = cpu_to_fdt32(new_addr & 0xffffffff);
		new_addr >>= 32;
	}
	for (i = *size_cells - 1; i >= 0; i--) {
		size[i] = cpu_to_fdt32(addr_size & 0xffffffff);
		addr_size >>= 32;
	}

	fprintf(stderr, "New address:\n\t");
	for (i = 0; i < *addr_cells; i++)
		fprintf(stderr, " %lx ", of_read_number(&addr[i], 1));
	fprintf(stderr, "\n");

	return 0;
}

static int create_translated_addresses(struct offb_ctx *ctx,
		int dev_node, const char *path,
		uint64_t in_addr, uint64_t in_size,
		fdt32_t *reg, int reg_cells)
{
	uint32_t addr[MAX_N_CELLS], size[MAX_N_CELLS];
	int addr_cells, size_cells, node, prop_len, ranges_len, rc, i;
	const fdt32_t *ranges, *prop;
	char *name;

	prop = fdt_getprop(ctx->dtb, 0, "#address-cells", &prop_len);
	addr_cells = prop ? fdt32_to_cpu(*prop) : 2;

	prop = fdt_getprop(ctx->dtb, 0, "#size-cells", &prop_len);
	size_cells = prop ? fdt32_to_cpu(*prop) : 2;

	memset(addr, 0, sizeof(uint32_t) * MAX_N_CELLS);
	for (i = addr_cells - 1; i >= 0; i--) {
		addr[i] = cpu_to_fdt32(in_addr & 0xffffffff);
		in_addr >>= 32;
	}
	memset(size, 0, sizeof(uint32_t) * MAX_N_CELLS);
	for (i = size_cells - 1; i >= 0; i--) {
		size[i] = cpu_to_fdt32(in_size & 0xffffffff);
		in_size >>= 32;
	}

	node = 0;
	for (;;) {
		/* get the name of the next child node to 'node' */
		name = next_dt_name(ctx, &path);
		if (!name)
			return -1;

		node = fdt_subnode_offset(ctx->dtb, node, name);
		if (node < 0)
			return -1;
		if (node == dev_node)
			break;

		ranges = fdt_getprop(ctx->dtb, node, "ranges", &ranges_len);
		if (!ranges)
			return -1;

		rc = do_translate(ctx->dtb, node, ranges, ranges_len,
			     addr, size, &addr_cells, &size_cells);
		if (rc)
			return -1;
	}

	fprintf(stderr, "Final address:\n\t");
	for (i = 0; i < addr_cells; i++)
		fprintf(stderr, " %lx ", of_read_number(&addr[i], 1));
	fprintf(stderr, "\n");

	if (addr_cells + size_cells > reg_cells) {
		fprintf(stderr, "Error: na + ns larger than reg\n");
		return -1;
	}

	memcpy(reg, addr, sizeof(fdt32_t) * addr_cells);
	memcpy(reg + addr_cells, size, sizeof(fdt32_t) * size_cells);

	return 0;
}

#define fdt_set_check(dtb, node, fn, prop, ...) \
	do {								\
		int __x = fn(dtb, node,	prop, __VA_ARGS__);		\
		if (__x) {						\
			warnx("failed to update device tree (%s): %s",	\
					prop, fdt_strerror(__x));	\
			return -1;					\
		}							\
	} while (0);

static int populate_devicetree(struct offb_ctx *ctx)
{
	fdt32_t reg[5];
	void *dtb = ctx->dtb;
	int rc, node = ctx->dtb_node;

	memset(reg, 0, sizeof(reg));
	rc = create_translated_addresses(ctx, node, ctx->path,
				ctx->fscreeninfo.smem_start,
				ctx->fscreeninfo.smem_len,
				reg, 5);

	if (rc) {
		fprintf(stderr, "Failed to translate address\n");
		return rc;
	}

	fdt_set_check(dtb, node, fdt_setprop_string, "device_type", "display");

	fdt_set_check(dtb, node, fdt_setprop, "assigned-addresses",
			reg, sizeof(reg));

	fdt_set_check(dtb, node, fdt_setprop_cell,
			"width", ctx->vscreeninfo.xres);
	fdt_set_check(dtb, node, fdt_setprop_cell,
			"height", ctx->vscreeninfo.yres);
	fdt_set_check(dtb, node, fdt_setprop_cell,
			"depth", ctx->vscreeninfo.bits_per_pixel);

	fdt_set_check(dtb, node, fdt_setprop, "little-endian", NULL, 0);
	fdt_set_check(dtb, node, fdt_setprop, "linux,opened", NULL, 0);
	fdt_set_check(dtb, node, fdt_setprop, "linux,boot-display", NULL, 0);

	return 0;
}

/*
 * Find the device tree path assoicated with a hvc device.
 * On OPAL all hvc consoles have a 'serial@X' node under ibm,opal/consoles,
 * so we make a simplifying assumption that a hvcX is associated with a
 * serial@X node.
 */
static char *get_hvc_path(struct offb_ctx *ctx, unsigned int termno)
{
	char *serial;
	int node;

	serial = talloc_asprintf(ctx, "serial@%u", termno);
	if (!serial)
		return NULL;

	node = fdt_subnode_offset(ctx->dtb, 0, "ibm,opal");
	if (node <= 0) {
		fprintf(stderr, "Couldn't find ibm,opal\n");
		return NULL;
	}
	node = fdt_subnode_offset(ctx->dtb, node, "consoles");
	if (node <= 0) {
		fprintf(stderr, "Couldn't find ibm,opal/consoles\n");
		return NULL;
	}

	node = fdt_subnode_offset(ctx->dtb, node, serial);
	if (node <= 0) {
		fprintf(stderr, "Could not locate hvc%u\n", termno);
		return NULL;
	}

	return talloc_asprintf(ctx, "/ibm,opal/consoles/%s", serial);
}

/*
 * Find the device tree path of the vga device. On OPAL we assume there is only
 * one of these that represents any 'tty' console.
 */
static char *get_vga_path(struct offb_ctx *ctx)
{
	char *root, *vga_path;

	root = strstr(ctx->path, "/pciex@");
	if (!root) {
		fprintf(stderr, "Can't find root path for vga device in below:\n");
		fprintf(stderr, "%s\n", ctx->path);
		return NULL;
	}

	vga_path = talloc_strdup(ctx, root);
	fprintf(stderr, "VGA target at '%s'\n", vga_path);

	return vga_path;
}

static int set_stdout(struct offb_ctx *ctx)
{
	const char *boot_console, *ptr;
	long unsigned int termno;
	const fdt32_t *prop;
	int node, prop_len;
	char *stdout_path;

	boot_console = getenv("boot_console");
	if (!boot_console) {
		fprintf(stderr, "boot_console not set, using default stdout for boot\n");
		return 0;
	}

	if (strncmp(boot_console, "/dev/", strlen("/dev/")) != 0) {
		/* We already have the full path */
		stdout_path = talloc_strdup(ctx, boot_console);
		/* Check for a tty* console but don't accidentally catch
		 * ttyS* consoles */
	} else if (strstr(boot_console, "tty") != NULL &&
			strstr(boot_console, "ttyS") == NULL) {
		fprintf(stderr, "TTY recognised: %s\n", boot_console);
		stdout_path = get_vga_path(ctx);
	} else {
		ptr = strstr(boot_console, "hvc");
		if (!ptr || strlen(ptr) <= strlen("hvc")) {
			fprintf(stderr, "Unrecognised console: %s\n",
					boot_console);
			return 0;
		}
		ptr += strlen("hvc");
		errno = 0;
		termno = strtoul(ptr, NULL, 0);
		if (errno) {
			fprintf(stderr, "Couldn't parse termno from %s\n",
					boot_console);
			return 0;
		}
		fprintf(stderr, "HVC recognised: %s\n", boot_console);
		stdout_path = get_hvc_path(ctx, termno);
	}

	if (!stdout_path) {
		fprintf(stderr, "Couldn't parse %s into a path\n",
				boot_console);
		return -1;
	}

	fprintf(stderr, "stdout-path: %s\n", stdout_path);

	node = fdt_subnode_offset(ctx->dtb, 0, "chosen");
	if (node <= 0) {
		fprintf(stderr, "Failed to find chosen\n");
		return -1;
	}

	prop = fdt_getprop(ctx->dtb, node, "linux,stdout-path", &prop_len);
	if (!prop) {
		fprintf(stderr, "Failed to find linux,stdout-path\n");
		return -1;
	}

	fdt_set_check(ctx->dtb, node, fdt_setprop_string, "linux,stdout-path",
			stdout_path);

	return 0;
}

static int write_devicetree(struct offb_ctx *ctx)
{
	int rc;

	fdt_pack(ctx->dtb);

	rc = replace_file(ctx->dtb_name, ctx->dtb, fdt_totalsize(ctx->dtb));
	if (rc)
		warn("failed to write file %s", ctx->dtb_name);

	return rc;
}

static int set_offb(struct offb_ctx *ctx)
{
	int rc;

	rc = load_dtb(ctx);
	if (rc)
		goto out;

	rc = fbdev_sysfs_lookup(ctx);
	if (rc)
		goto out;

	rc = fbdev_device_query(ctx);
	if (rc)
		goto out;

	rc = populate_devicetree(ctx);
	if (rc)
		goto out;
out:
	return rc;
}


int main(void)
{
	struct offb_ctx *ctx;
	int rc;

	ctx = talloc_zero(NULL, struct offb_ctx);

	ctx->dtb_name = getenv("boot_dtb");
	if (!ctx->dtb_name) {
		talloc_free(ctx);
		return EXIT_SUCCESS;
	}

	if (set_offb(ctx)) {
		warn("Failed offb setup step");
		rc = -1;
	}

	if (set_stdout(ctx)) {
		warn("Failed stdout setup step\n");
		rc = -1;
	}

	if (write_devicetree(ctx)) {
		warn("Failed to write back device tree\n");
		rc = -1;
	}

	talloc_free(ctx);
	return rc ? EXIT_FAILURE : EXIT_SUCCESS;
}
