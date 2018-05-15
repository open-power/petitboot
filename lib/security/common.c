/*
 *  Copyright (C) 2016 Raptor Engineering, LLC
 *  Copyright (C) 2018 Opengear, Inc
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
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>
#include <dirent.h>
#include <string.h>
#include <fcntl.h>
#include <locale.h>
#include <sys/types.h>

#include <log/log.h>
#include <file/file.h>
#include <talloc/talloc.h>
#include <url/url.h>
#include <util/util.h>
#include <i18n/i18n.h>

#include "security.h"

struct pb_url * get_signature_url(void *ctx, struct pb_url *base_file)
{
	struct pb_url *signature_file = NULL;

	signature_file = pb_url_copy(ctx, base_file);
	talloc_free(signature_file->file);
	signature_file->file = talloc_asprintf(signature_file,
		"%s.sig", base_file->file);
	talloc_free(signature_file->path);
	signature_file->path = talloc_asprintf(signature_file,
		"%s.sig", base_file->path);

	return signature_file;
}

int validate_boot_files(struct boot_task *boot_task) {
    int result = 0;
    char *kernel_filename = NULL;
    char *initrd_filename = NULL;
    char *dtb_filename = NULL;

    FILE *authorized_signatures_handle = NULL;

    char cmdline_template[] = "/tmp/petitbootXXXXXX";
    int cmdline_fd = mkstemp(cmdline_template);
    FILE *cmdline_handle = NULL;

    const char* local_initrd_signature = (boot_task->verify_signature) ?
        boot_task->local_initrd_signature : NULL;
    const char* local_dtb_signature = (boot_task->verify_signature) ?
        boot_task->local_dtb_signature : NULL;
    const char* local_image_signature = (boot_task->verify_signature) ?
        boot_task->local_image_signature : NULL;
    const char* local_cmdline_signature =
        (boot_task->verify_signature || boot_task->decrypt_files) ?
        boot_task->local_cmdline_signature : NULL;

    if ((!boot_task->verify_signature) && (!boot_task->decrypt_files))
        return result;

    /* Load authorized signatures file */
    authorized_signatures_handle = fopen(LOCKDOWN_FILE, "r");
    if (!authorized_signatures_handle) {
        pb_log("%s: unable to read lockdown file\n", __func__);
        return KEXEC_LOAD_SIG_SETUP_INVALID;
    }

    /* Copy files to temporary directory for verification / boot */
    result = copy_file_secure_dest(boot_task,
        boot_task->local_image,
        &kernel_filename);
    if (result) {
        pb_log("%s: image copy failed: (%d)\n",
            __func__, result);
        return result;
    }
    if (boot_task->local_initrd) {
        result = copy_file_secure_dest(boot_task,
            boot_task->local_initrd,
            &initrd_filename);
        if (result) {
            pb_log("%s: initrd copy failed: (%d)\n",
                __func__, result);
            return result;
        }
    }
    if (boot_task->local_dtb) {
        result = copy_file_secure_dest(boot_task,
            boot_task->local_dtb,
            &dtb_filename);
        if (result) {
            pb_log("%s: dtb copy failed: (%d)\n",
                __func__, result);
            return result;
        }
    }
    boot_task->local_image_override = talloc_strdup(boot_task,
        kernel_filename);
    if (boot_task->local_initrd)
        boot_task->local_initrd_override = talloc_strdup(boot_task,
            initrd_filename);
    if (boot_task->local_dtb)
        boot_task->local_dtb_override = talloc_strdup(boot_task,
            dtb_filename);

    /* Write command line to temporary file for verification */
    if (cmdline_fd < 0) {
        /* mkstemp failed */
        pb_log("%s: failed: unable to create command line"
            " temporary file for verification\n",
            __func__);
        result = -1;
    }
    else {
        cmdline_handle = fdopen(cmdline_fd, "w");
    }
    if (!cmdline_handle) {
        /* Failed to open file */
        pb_log("%s: failed: unable to write command line"
            " temporary file for verification\n",
            __func__);
        result = -1;
    }
    else {
        fwrite(boot_task->args, sizeof(char),
            strlen(boot_task->args), cmdline_handle);
        fflush(cmdline_handle);
    }

    if (boot_task->verify_signature) {
        /* Check signatures */
        if (verify_file_signature(kernel_filename,
            local_image_signature,
            authorized_signatures_handle,
            KEYRING_PATH))
            result = KEXEC_LOAD_SIGNATURE_FAILURE;
        if (verify_file_signature(cmdline_template,
            local_cmdline_signature,
            authorized_signatures_handle,
            KEYRING_PATH))
            result = KEXEC_LOAD_SIGNATURE_FAILURE;

        if (boot_task->local_initrd_signature)
            if (verify_file_signature(initrd_filename,
                local_initrd_signature,
                authorized_signatures_handle,
                KEYRING_PATH))
                result = KEXEC_LOAD_SIGNATURE_FAILURE;
        if (boot_task->local_dtb_signature)
            if (verify_file_signature(dtb_filename,
                local_dtb_signature,
                authorized_signatures_handle,
                KEYRING_PATH))
                result = KEXEC_LOAD_SIGNATURE_FAILURE;

        /* Clean up */
        if (cmdline_handle) {
            fclose(cmdline_handle);
            unlink(cmdline_template);
        }
        fclose(authorized_signatures_handle);
    } else if (boot_task->decrypt_files) {
        /* Decrypt files */
        if (decrypt_file(kernel_filename,
            authorized_signatures_handle,
            KEYRING_PATH))
            result = KEXEC_LOAD_DECRYPTION_FALURE;
        if (verify_file_signature(cmdline_template,
            local_cmdline_signature,
            authorized_signatures_handle,
            KEYRING_PATH))
            result = KEXEC_LOAD_SIGNATURE_FAILURE;
        if (boot_task->local_initrd)
            if (decrypt_file(initrd_filename,
                authorized_signatures_handle,
                KEYRING_PATH))
                result = KEXEC_LOAD_DECRYPTION_FALURE;
        if (boot_task->local_dtb)
            if (decrypt_file(dtb_filename,
                authorized_signatures_handle,
                KEYRING_PATH))
                result = KEXEC_LOAD_DECRYPTION_FALURE;

        /* Clean up */
        if (cmdline_handle) {
            fclose(cmdline_handle);
            unlink(cmdline_template);
        }
        fclose(authorized_signatures_handle);
    }

    return result;
}

void validate_boot_files_cleanup(struct boot_task *boot_task) {
	if ((boot_task->verify_signature) || (boot_task->decrypt_files)) {
		unlink(boot_task->local_image_override);
		if (boot_task->local_initrd_override)
			unlink(boot_task->local_initrd_override);
		if (boot_task->local_dtb_override)
			unlink(boot_task->local_dtb_override);

		talloc_free(boot_task->local_image_override);
		if (boot_task->local_initrd_override)
			talloc_free(boot_task->local_initrd_override);
		if (boot_task->local_dtb_override)
			talloc_free(boot_task->local_dtb_override);
	}
}

