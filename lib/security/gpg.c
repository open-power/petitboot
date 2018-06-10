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
#include <locale.h>
#include <sys/types.h>

#include <log/log.h>
#include <file/file.h>
#include <talloc/talloc.h>
#include <url/url.h>
#include <util/util.h>
#include <i18n/i18n.h>

#include <gpgme.h>

#include "security.h"

/*
 * If --with-signed-boot is enabled lib/security provides the ability to handle
 * gpg-signed and/or encrypted boot sources (kernel, initrd, etc).
 * This can be used to enable a form of secure boot, but it is important to
 * recognise that it depends on the security of the entire system, for example
 * a full trusted-boot implementation. Petitboot can not and will not be able
 * to guarantee secure boot by itself.
 */

int decrypt_file(const char *filename,
	FILE *authorized_signatures_handle, const char *keyring_path)
{
	int result = 0;
	int valid = 0;
	size_t bytes_read = 0;
	unsigned char buffer[8192];

	if (filename == NULL)
		return -1;

	gpgme_signature_t verification_signatures;
	gpgme_verify_result_t verification_result;
	gpgme_data_t ciphertext_data;
	gpgme_data_t plaintext_data;
	gpgme_engine_info_t enginfo;
	gpgme_ctx_t gpg_context;
	gpgme_error_t err;

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
			GPGME_PROTOCOL_OpenPGP,
			enginfo->file_name, keyring_path);
	else
		err = gpgme_ctx_set_engine_info (gpg_context,
			GPGME_PROTOCOL_OpenPGP,
			enginfo->file_name, enginfo->home_dir);
	if (err != GPG_ERR_NO_ERROR) {
		pb_log("%s: Could not set GPG engine information\n", __func__);
		return -1;
	}
	err = gpgme_data_new(&plaintext_data);
	if (err != GPG_ERR_NO_ERROR) {
		pb_log("%s: Could not create GPG plaintext data buffer\n",
			__func__);
		return -1;
	}
	err = gpgme_data_new_from_file(&ciphertext_data, filename, 1);
	if (err != GPG_ERR_NO_ERROR) {
		pb_log("%s: Could not create GPG ciphertext data buffer"
			" from file '%s'\n", __func__, filename);
		return -1;
	}

	/* Decrypt and verify file */
	err = gpgme_op_decrypt_verify(gpg_context, ciphertext_data,
		plaintext_data);
	if (err != GPG_ERR_NO_ERROR) {
		pb_log("%s: Could not decrypt file\n", __func__);
		return -1;
	}
	verification_result = gpgme_op_verify_result(gpg_context);
	verification_signatures = verification_result->signatures;
	while (verification_signatures) {
		if (verification_signatures->status == GPG_ERR_NO_ERROR) {
			pb_log("%s: Good signature for key ID '%s' ('%s')\n",
				__func__,
				verification_signatures->fpr, filename);
			/* Verify fingerprint is present in authorized
			 * signatures file
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
				auth_sig_line[auth_sig_len] = 0;
				if (strcmp(auth_sig_line,
					verification_signatures->fpr) == 0)
					valid = 1;
			}
			free(auth_sig_line);
		}
		else {
			pb_log("%s: Signature for key ID '%s' ('%s') invalid."
				"  Status: %08x\n", __func__,
				verification_signatures->fpr, filename,
				verification_signatures->status);
		}
		verification_signatures = verification_signatures->next;
	}

	gpgme_data_release(ciphertext_data);

	if (valid) {
		/* Write decrypted file over ciphertext */
		FILE *plaintext_file_handle = NULL;
		plaintext_file_handle = fopen(filename, "wb");
		if (!plaintext_file_handle) {
			pb_log("%s: Could not create GPG plaintext file '%s'\n",
				__func__, filename);
			gpgme_data_release(plaintext_data);
			gpgme_release(gpg_context);
			return -1;
		}
		gpgme_data_seek(plaintext_data, 0, SEEK_SET);
		if (err != GPG_ERR_NO_ERROR) {
			pb_log("%s: Could not seek in GPG plaintext buffer\n",
				__func__);
			gpgme_data_release(plaintext_data);
			gpgme_release(gpg_context);
			fclose(plaintext_file_handle);
			return -1;
		}
		while ((bytes_read = gpgme_data_read(plaintext_data, buffer,
			8192)) > 0) {
			size_t l2 = fwrite(buffer, 1, bytes_read,
				plaintext_file_handle);
			if (l2 < bytes_read) {
				if (ferror(plaintext_file_handle)) {
					/* General error */
					result = -1;
					pb_log("%s: failed: unknown fault\n",
						__func__);
				}
				else {
					/* No space on destination device */
					result = -1;
					pb_log("%s: failed: temporary storage"
						" full\n", __func__);
				}
				break;
			}
		}
		fclose(plaintext_file_handle);
	}

	/* Clean up */
	gpgme_data_release(plaintext_data);
	gpgme_release(gpg_context);

	if (!valid) {
		pb_log("%s: Incorrect GPG signature\n", __func__);
		return -1;
	}

	pb_log("%s: GPG signature for decrypted file '%s' verified\n",
		__func__, filename);

	return result;
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

int lockdown_status() {
	/* assume most restrictive lockdown type */
	int ret = PB_LOCKDOWN_SIGN;

#if !defined(HARD_LOCKDOWN)
	if (access(LOCKDOWN_FILE, F_OK) == -1)
		return PB_LOCKDOWN_NONE;
#endif

	/* determine lockdown type */
	FILE *authorized_signatures_handle = NULL;
	authorized_signatures_handle = fopen(LOCKDOWN_FILE, "r");
	if (!authorized_signatures_handle)
		return ret;

	char *auth_sig_line = NULL;
	size_t auth_sig_len = 0;
	ssize_t auth_sig_read;
	rewind(authorized_signatures_handle);
	if ((auth_sig_read = getline(&auth_sig_line,
		&auth_sig_len,
		authorized_signatures_handle)) != -1) {
		auth_sig_len = strlen(auth_sig_line);
		while ((auth_sig_line[auth_sig_len-1] == '\n')
		    || (auth_sig_line[auth_sig_len-1] == '\r'))
			auth_sig_len--;
		auth_sig_line[auth_sig_len] = 0;
		if (strcmp(auth_sig_line, "ENCRYPTED") == 0) {
			/* first line indicates encrypted files
			 * expected.  enable decryption.
			 */
			ret = PB_LOCKDOWN_DECRYPT;
		}
	}
	free(auth_sig_line);

    return ret;
}

