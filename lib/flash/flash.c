/*
 *  Copyright (C) 2015 IBM Corporation
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>
#include <asm/byteorder.h>

#include <log/log.h>
#include <talloc/talloc.h>
#include <flash/flash.h>
#include <ccan/endian/endian.h>

#include <libflash/arch_flash.h>
#include <libflash/blocklevel.h>
#include <libflash/libflash.h>
#include <libflash/libffs.h>
#include <libflash/file.h>
#include <libflash/ecc.h>


struct flash_info {
	/* Device information */
	struct blocklevel_device	*bl;
	struct ffs_handle		*ffs;
	uint64_t			size;	/* raw size of partition */
	const char			*path;
	bool				ecc;
	uint32_t			erase_granule;

	/* Partition information */
	uint32_t			attr_part_idx;
	uint32_t			attr_data_pos;
	uint32_t			attr_data_len; /* Includes ECC bytes */

	/* 'Other Side' info if present */
	struct flash_info		*other_side;

};

static int partition_info(struct flash_info *info, const char *partition)
{
	int rc;

	rc = ffs_lookup_part(info->ffs, partition, &info->attr_part_idx);
	if (rc) {
		pb_log("Failed to find %s partition\n", partition);
		return rc;
	}

	return ffs_part_info(info->ffs, info->attr_part_idx, NULL,
			   &info->attr_data_pos, &info->attr_data_len,
			   NULL, &info->ecc);
}

static struct flash_info *flash_setup_buffer(void *ctx, const char *partition)
{
	struct flash_info *info;
	struct ffs_handle *tmp = NULL;
	int rc = 0;

	if (!partition)
		return NULL;

	info = talloc_zero(ctx, struct flash_info);
	if (!info)
		return NULL;

	rc = arch_flash_init(&info->bl, NULL, true);
	if (rc) {
		pb_log("Failed to init mtd device\n");
		goto out;
	}

	rc = blocklevel_get_info(info->bl, &info->path, &info->size,
				 &info->erase_granule);
	if (rc) {
		pb_log("Failed to retrieve blocklevel info\n");
	        goto out_flash;
	}

	rc = ffs_init(0, info->size, info->bl, &info->ffs, 1);
	if (rc) {
		pb_log_fn("Failed to init ffs\n");
		goto out_flash;
	}

	rc = partition_info(info, partition);
	if (rc) {
		pb_log("Failed to retrieve partition info\n");
		goto out_ffs;
	}

	/* Check if there is a second flash side. If there is not, or
	 * we fail to recognise it, ignore it and continue */
	ffs_next_side(info->ffs, &tmp, info->ecc);
	if (tmp && ffs_equal(info->ffs, tmp) == false) {
		pb_debug("Other side present on MTD device\n");
		info->other_side = talloc_zero(info, struct flash_info);
		info->other_side->ffs = tmp;
		info->other_side->bl = info->bl;
		info->other_side->size = info->size;
		info->other_side->path = info->path;
		info->other_side->ecc = info->ecc;
		info->other_side->erase_granule = info->erase_granule;
		rc = partition_info(info->other_side, partition);
		if (rc)
			pb_log("Failed to retrieve partition info "
			       "for other side - ignoring\n");
	}

	pb_debug("%s Details\n", partition);
	pb_debug("\tName\t\t%s\n", info->path);
	pb_debug("\tFlash Size\t%lu\n", info->size);
	pb_debug("\tGranule\t\t%u\n", info->erase_granule);
	pb_debug("\tECC\t\t%s\n", info->ecc ? "Protected" : "Unprotected");
	pb_debug("\tCurrent Side info:\n");
	pb_debug("\tIndex\t\t%u\n", info->attr_part_idx);
	pb_debug("\tOffset\t\t%u\n", info->attr_data_pos);
	pb_debug("\tPart. Size\t%u\n", info->attr_data_len);
	if (info->other_side) {
		pb_debug("\tOther Side info:\n");
		pb_debug("\tIndex\t\t%u\n", info->other_side->attr_part_idx);
		pb_debug("\tOffset\t\t%u\n", info->other_side->attr_data_pos);
		pb_debug("\tPart. Size\t%u\n", info->other_side->attr_data_len);
	}

	return info;
out_ffs:
	ffs_close(info->ffs);
out_flash:
	arch_flash_close(info->bl, NULL);
out:
	talloc_free(info);
	return NULL;
}

int flash_parse_version(void *ctx, char ***versions, bool current)
{
	char *saveptr, *tok,  **tmp, *buffer;
	const char *delim = "\n";
	struct flash_info *info, *cur_info;
	uint32_t len;
	int rc, n = 0;

	saveptr = tok = NULL;
	tmp = NULL;

	info = flash_setup_buffer(ctx, "VERSION");
	if (!info)
		return 0;

	if (!current && !info->other_side)
		return 0;

	cur_info = !current ? info->other_side : info;

	len = cur_info->attr_data_len -  ecc_size(cur_info->attr_data_len);
	buffer = talloc_array(cur_info, char, len);
	if (!buffer) {
		pb_log_fn("Failed to init buffer!\n");
		goto out;
	}

	rc = blocklevel_read(cur_info->bl, cur_info->attr_data_pos,
			     buffer, len);
	if (rc) {
		pb_log("Failed to read VERSION partition\n");
		goto out;
	}

	/* open-power-platform */
	tok = strtok_r(buffer, delim, &saveptr);
	if (tok) {
		tmp = talloc_realloc(ctx, tmp, char *, n + 1);
		if (!tmp) {
			pb_log_fn("Failed to allocate memory\n");
			goto out;
		}
		tmp[n++] = talloc_strdup(ctx, tok);
	}

	tok = strtok_r(NULL, delim, &saveptr);
	while (tok) {
		/* Ignore leading tab from subsequent lines */
		tmp = talloc_realloc(ctx, tmp, char *, n + 1);
		if (!tmp) {
			pb_log_fn("Failed to reallocate memory\n");
			n = 0;
			goto out;
		}
		tmp[n++] = talloc_strdup(ctx, tok + 1);
		tok = strtok_r(NULL, delim, &saveptr);
	}

out:
	pb_debug("%d version strings read from %s side\n",
		 n, current ? "current" : "other");
	arch_flash_close(info->bl, NULL);
	talloc_free(info);
	*versions = tmp;
	return n;
}
