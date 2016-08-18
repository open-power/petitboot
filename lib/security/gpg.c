/*
 *  Copyright (C) 2016 Raptor Engineering, LLC
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
#include <sys/types.h>

#include <log/log.h>
#include <file/file.h>
#include <talloc/talloc.h>
#include <url/url.h>
#include <util/util.h>
#include <i18n/i18n.h>

#include "gpg.h"

/*
 * If --with-signed-boot is enabled lib/security provides the ability to handle
 * gpg-signed and/or encrypted boot sources (kernel, initrd, etc).
 * This can be used to enable a form of secure boot, but it is important to
 * recognise that it depends on the security of the entire system, for example
 * a full trusted-boot implementation. Petitboot can not and will not be able
 * to guarantee secure boot by itself.
 */

struct pb_url * gpg_get_signature_url(void *ctx, struct pb_url *base_file)
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

int verify_file_signature(const char *plaintext_filename,
	const char *signature_filename, FILE *authorized_signatures_handle,
	const char *keyring_path)
{
	int valid = 0;
	gpgme_signature_t verification_signatures;
	gpgme_verify_result_t verification_result;
	gpgme_data_t plaintext_data;
	gpgme_data_t signature_data;
	gpgme_engine_info_t enginfo;
	gpgme_ctx_t gpg_context;
	gpgme_error_t err;

	if (signature_filename == NULL)
		return -1;

	/* Initialize gpgme */
	setlocale (LC_ALL, "");
	gpgme_check_version(NULL);
	gpgme_set_locale(NULL, LC_CTYPE, setlocale (LC_CTYPE, NULL));
	err = gpgme_engine_check_version(GPGME_PROTOCOL_OpenPGP);
	if (err != GPG_ERR_NO_ERROR) {
		pb_log("%s: OpenPGP support not available\n", __func__);
		return -1;
	}
	err = gpgme_get_engine_info(&enginfo);
	if (err != GPG_ERR_NO_ERROR) {
		pb_log("%s: GPG engine failed to initialize\n", __func__);
		return -1;
	}
	err = gpgme_new(&gpg_context);
	if (err != GPG_ERR_NO_ERROR) {
		pb_log("%s: GPG context could not be created\n", __func__);
		return -1;
	}
	err = gpgme_set_protocol(gpg_context, GPGME_PROTOCOL_OpenPGP);
	if (err != GPG_ERR_NO_ERROR) {
		pb_log("%s: GPG protocol could not be set\n", __func__);
		return -1;
	}
	if (keyring_path)
		err = gpgme_ctx_set_engine_info (gpg_context,
			GPGME_PROTOCOL_OpenPGP, enginfo->file_name,
			keyring_path);
	else
		err = gpgme_ctx_set_engine_info (gpg_context,
			GPGME_PROTOCOL_OpenPGP, enginfo->file_name,
			enginfo->home_dir);
	if (err != GPG_ERR_NO_ERROR) {
		pb_log("%s: Could not set GPG engine information\n", __func__);
		return -1;
	}
	err = gpgme_data_new_from_file(&plaintext_data, plaintext_filename, 1);
	if (err != GPG_ERR_NO_ERROR) {
		pb_log("%s: Could not create GPG plaintext data buffer"
			" from file '%s'\n", __func__, plaintext_filename);
		return -1;
	}
	err = gpgme_data_new_from_file(&signature_data, signature_filename, 1);
	if (err != GPG_ERR_NO_ERROR) {
		pb_log("%s: Could not create GPG signature data buffer"
			" from file '%s'\n", __func__, signature_filename);
		return -1;
	}

	/* Check signature */
	err = gpgme_op_verify(gpg_context, signature_data, plaintext_data,
		NULL);
	if (err != GPG_ERR_NO_ERROR) {
		pb_log("%s: Could not verify file using GPG signature '%s'\n",
			__func__, signature_filename);
		return -1;
	}
	verification_result = gpgme_op_verify_result(gpg_context);
	verification_signatures = verification_result->signatures;
	while (verification_signatures) {
		if (verification_signatures->status != GPG_ERR_NO_ERROR) {
			/* Signature verification failure */
			pb_log("%s: Signature for key ID '%s' ('%s') invalid."
				"  Status: %08x\n", __func__,
				verification_signatures->fpr,
				signature_filename,
				verification_signatures->status);
			verification_signatures = verification_signatures->next;
			continue;
		}

		/* Signature check passed with no error */
		pb_log("%s: Good signature for key ID '%s' ('%s')\n",
			__func__, verification_signatures->fpr,
			signature_filename);
		/* Verify fingerprint is present in
			* authorized signatures file
			*/
		char *auth_sig_line = NULL;
		size_t auth_sig_len = 0;
		ssize_t auth_sig_read;
		rewind(authorized_signatures_handle);
		while ((auth_sig_read = getline(&auth_sig_line,
			&auth_sig_len,
			authorized_signatures_handle)) != -1) {
			auth_sig_len = strlen(auth_sig_line);
			while ((auth_sig_line[auth_sig_len-1] == '\n')
				|| (auth_sig_line[auth_sig_len-1] == '\r'))
				auth_sig_len--;
			auth_sig_line[auth_sig_len] = '\0';
			if (strcmp(auth_sig_line,
				verification_signatures->fpr) == 0)
				valid = 1;
		}
		free(auth_sig_line);
		verification_signatures = verification_signatures->next;
	}

	/* Clean up */
	gpgme_data_release(plaintext_data);
	gpgme_data_release(signature_data);
	gpgme_release(gpg_context);

	if (!valid) {
		pb_log("%s: Incorrect GPG signature\n", __func__);
		return -1;
	}

	pb_log("%s: GPG signature '%s' for file '%s' verified\n",
		__func__, signature_filename, plaintext_filename);

	return 0;
}

int gpg_validate_boot_files(struct boot_task *boot_task) {
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
	const char* local_cmdline_signature = (boot_task->verify_signature) ?
		boot_task->local_cmdline_signature : NULL;

	if (!boot_task->verify_signature)
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

	/* Check signatures */
	if (verify_file_signature(kernel_filename,
		local_image_signature,
		authorized_signatures_handle, "/etc/gpg"))
		result = KEXEC_LOAD_SIGNATURE_FAILURE;
	if (verify_file_signature(cmdline_template,
		local_cmdline_signature,
		authorized_signatures_handle, "/etc/gpg"))
		result = KEXEC_LOAD_SIGNATURE_FAILURE;
	if (boot_task->local_initrd_signature)
		if (verify_file_signature(initrd_filename,
			local_initrd_signature,
			authorized_signatures_handle, "/etc/gpg"))
			result = KEXEC_LOAD_SIGNATURE_FAILURE;
	if (boot_task->local_dtb_signature)
		if (verify_file_signature(dtb_filename,
			local_dtb_signature,
			authorized_signatures_handle, "/etc/gpg"))
			result = KEXEC_LOAD_SIGNATURE_FAILURE;

	/* Clean up */
	if (cmdline_handle) {
		fclose(cmdline_handle);
		unlink(cmdline_template);
	}
	fclose(authorized_signatures_handle);

	return result;
}

void gpg_validate_boot_files_cleanup(struct boot_task *boot_task) {
	if (boot_task->verify_signature) {
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

int lockdown_status() {
	if (access(LOCKDOWN_FILE, F_OK) == -1)
		return PB_LOCKDOWN_NONE;
	else
		return PB_LOCKDOWN_SIGN;
}