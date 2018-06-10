/*
 *  Copyright (C) 2018 Opengear
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

#include <openssl/conf.h>
#include <openssl/bio.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/evp.h>
#include <openssl/err.h>
#include <openssl/cms.h>
#include <openssl/pkcs12.h>

#include "security.h"

static const EVP_MD *s_verify_md = NULL;

static	__attribute__((constructor)) void crypto_init(void)
{
#if OPENSSL_VERSION_NUMBER < 0x10100000L
	OPENSSL_no_config();
	OpenSSL_add_all_algorithms();
	ERR_load_crypto_strings();
	ERR_load_CMS_strings();
#endif

	s_verify_md = EVP_get_digestbyname(VERIFY_DIGEST);
	if (!s_verify_md)
		pb_log("Specified OpenSSL digest '%s' not found\n", VERIFY_DIGEST);

}

#if OPENSSL_VERSION_NUMBER < 0x10100000L
static __attribute__((destructor)) void crypto_fini(void)
{
	EVP_cleanup();
	ERR_free_strings();
}
#endif

static int pb_log_print_errors_cb(const char *str,
				  size_t len __attribute__((unused)),
				  void *u __attribute__((unused)))
{
	pb_log("    %s\n", str);
	return 0;
}

static int get_pkcs12(FILE *keyfile, X509 **cert, EVP_PKEY **priv)
{
	PKCS12 *p12 = NULL;
	int ok = 0;

	rewind(keyfile);

	p12 = d2i_PKCS12_fp(keyfile, NULL);
	if (p12) {
		/*
		 * annoying but NULL and "" are two valid but different
		 * default passwords
		 */
		if (!PKCS12_parse(p12, NULL, priv, cert, NULL) &&
		    !PKCS12_parse(p12,   "", priv, cert, NULL)) {
			pb_log("%s: Error parsing OpenSSL PKCS12:\n", __func__);
			ERR_print_errors_cb(&pb_log_print_errors_cb, NULL);
		} else
			ok = 1;

		PKCS12_free(p12);
	}

	return ok;
}

static X509 *get_cert(FILE *keyfile)
{
	EVP_PKEY *priv = NULL;
	X509 *cert = NULL;

	if (get_pkcs12(keyfile, &cert, &priv)) {
		EVP_PKEY_free(priv);
	} else {
		rewind(keyfile);
		ERR_clear_error();
		cert = PEM_read_X509(keyfile, NULL, NULL, NULL);
	}

	return cert;
}

static STACK_OF(X509) *get_cert_stack(FILE *keyfile)
{
	STACK_OF(X509) *certs = sk_X509_new_null();
	X509 *cert = NULL;

	if (certs) {
		cert = get_cert(keyfile);
		if (cert)
			sk_X509_push(certs, get_cert(keyfile));
	} else {
		pb_log("%s: Error allocating OpenSSL X509 stack:\n", __func__);
		ERR_print_errors_cb(&pb_log_print_errors_cb, NULL);
	}

	return certs;
}


static EVP_PKEY *get_public_key(FILE *keyfile)
{
	EVP_PKEY *pkey = NULL;
	X509 *cert = NULL;

	/*
	 * walk through supported file types looking for a public key:
	 *
	 * 1. PKCS12
	 * 2. PEM encoded X509
	 * 3. PEM encoded raw public key
	 *
	 * someday in the future maybe utilize the keyring_path
	 * as an input for X509_STORE_load_locations for certificate
	 * validity checking
	 */

	cert = get_cert(keyfile);
	if (cert) {
		pkey = X509_get_pubkey(cert);
		X509_free(cert);
	} else {
		rewind(keyfile);
		ERR_clear_error();
		pkey = PEM_read_PUBKEY(keyfile, NULL, NULL, NULL);
	}

	/* handles both cases */
	if (!pkey) {
		pb_log("%s: Error loading OpenSSL public key:\n", __func__);
		ERR_print_errors_cb(&pb_log_print_errors_cb, NULL);
	}

	return pkey;
}

int decrypt_file(const char *filename,
		 FILE *authorized_signatures_handle,
		 const char *keyring_path __attribute__((unused)))
{
	BIO *content_bio = NULL, *file_bio = NULL, *out_bio = NULL;
	STACK_OF(X509) *certs = NULL;
	CMS_ContentInfo *cms = NULL;
	EVP_PKEY *priv = NULL;
	X509 *cert = NULL;
	int nok = -1;
	char *outptr;
	long outl;
	int bytes;

	if (!get_pkcs12(authorized_signatures_handle, &cert, &priv)) {
		pb_log("%s: Error opening OpenSSL decrypt authorization file:\n",
		       __func__);
		ERR_print_errors_cb(&pb_log_print_errors_cb, NULL);
		goto out;
	}

	file_bio = BIO_new_file(filename, "r");
	if (!file_bio) {
		pb_log("%s: Error opening OpenSSL decrypt cipher file '%s':\n",
		       __func__, filename);
		ERR_print_errors_cb(&pb_log_print_errors_cb, NULL);
		goto out;
	}

	out_bio = BIO_new(BIO_s_mem());
	if (!out_bio) {
		pb_log("%s: Error allocating OpenSSL decrypt output buffer:\n",
		       __func__);
		ERR_print_errors_cb(&pb_log_print_errors_cb, NULL);
		goto out;
	}

	/* right now only support signed-envelope CMS */

	cms = SMIME_read_CMS(file_bio, &content_bio);
	if (!cms) {
		pb_log("%s: Error parsing OpenSSL CMS decrypt '%s'\n",
		       __func__, filename);
		ERR_print_errors_cb(&pb_log_print_errors_cb, NULL);
		goto out;
	}

	BIO_free(content_bio);
	content_bio = BIO_new(BIO_s_mem());
	if (!content_bio) {
		pb_log("%s: Error allocating OpenSSL decrypt content buffer:\n",
		       __func__);
		ERR_print_errors_cb(&pb_log_print_errors_cb, NULL);
		goto out;
	}

	if (!CMS_decrypt(cms, priv, cert, NULL, out_bio, 0)) {
		pb_log("%s: Error in OpenSSL CMS decrypt '%s'\n",
		       __func__, filename);
		ERR_print_errors_cb(&pb_log_print_errors_cb, NULL);
		goto out;
	}

	certs = sk_X509_new_null();
	if (!certs) {
		pb_log("%s: Error allocating OpenSSL X509 stack:\n", __func__);
		ERR_print_errors_cb(&pb_log_print_errors_cb, NULL);
		goto out;
	}

	sk_X509_push(certs, cert);

	CMS_ContentInfo_free(cms);

	cms = SMIME_read_CMS(out_bio, &content_bio);
	if (!cms) {
		pb_log("%s: Error parsing OpenSSL CMS decrypt verify:\n",
		       __func__);
		ERR_print_errors_cb(&pb_log_print_errors_cb, NULL);
		goto out;
	}

	/* this is a mem BIO so failure is 0 or -1 */
	if (BIO_reset(out_bio) < 1) {
		pb_log("%s: Error resetting OpenSSL decrypt output buffer:\n",
		       __func__);
		ERR_print_errors_cb(&pb_log_print_errors_cb, NULL);
		goto out;
	}

	/* in this mode its attached content */
	if (!CMS_verify(cms, certs, NULL, content_bio, out_bio,
			CMS_NO_SIGNER_CERT_VERIFY | CMS_BINARY)) {
		pb_log("%s: Failed OpenSSL CMS decrypt verify:\n", __func__);
		ERR_print_errors_cb(&pb_log_print_errors_cb, NULL);
		goto out;
	}

	/* reopen the file so we force a truncation */
	BIO_free(file_bio);
	file_bio = BIO_new_file(filename, "w");
	if (!file_bio) {
		pb_log("%s: Error opening OpenSSL decrypt output file '%s'\n",
		       __func__, filename);
		ERR_print_errors_cb(&pb_log_print_errors_cb, NULL);
		goto out;
	}

	outl = BIO_get_mem_data(out_bio, &outptr);

	while (outl) {
		bytes = BIO_write(file_bio, outptr, outl);
		if (bytes > 0) {
			outl -= (long)bytes;
			outptr += bytes;

		} else if (bytes < 0) {
			pb_log("%s: OpenSSL decrypt output write failure on file '%s':\n",
				       __func__, filename);
			ERR_print_errors_cb(&pb_log_print_errors_cb, NULL);
			goto out;
		}
	}

	if (!outl)
		nok = 0;

out:
	if (cms)
		CMS_ContentInfo_free(cms);
	BIO_free(file_bio);
	BIO_free(content_bio);
	BIO_free(out_bio);
	X509_free(cert);
	sk_X509_free(certs);
	EVP_PKEY_free(priv);
	return nok;
}

int verify_file_signature(const char *plaintext_filename,
			  const char *signature_filename,
			  FILE *authorized_signatures_handle,
			  const char *keyring_path __attribute__((unused)))
{
	BIO *signature_bio = NULL, *plaintext_bio = NULL, *content_bio = NULL;
	STACK_OF(X509) *certs = NULL;
	CMS_ContentInfo *cms = NULL;
	ssize_t bytes_read = -1;
	EVP_MD_CTX *ctx = NULL;
	EVP_PKEY *pkey = NULL;
	char *sigbuf = NULL;
	char rdbuf[8192];
	int nok = -1;
	int siglen;

	plaintext_bio = BIO_new_file(plaintext_filename, "r");
	if (!plaintext_bio) {
		pb_log("%s: Error opening OpenSSL verify plaintext file '%s'\n",
		       __func__, plaintext_filename);
		ERR_print_errors_cb(&pb_log_print_errors_cb, NULL);
		goto out;
	}

	signature_bio = BIO_new_file(signature_filename, "r");
	if (!signature_bio) {
		pb_log("%s: Error opening OpenSSL verify signature file '%s'\n",
		       __func__, signature_filename);
		ERR_print_errors_cb(&pb_log_print_errors_cb, NULL);
		goto out;
	}

	/* first check CMS */
	cms = SMIME_read_CMS(signature_bio, &content_bio);
	if (cms) {
		certs = get_cert_stack(authorized_signatures_handle);

		/*
		 * this has to always be detached, which means we always
		 * ignore content_bio and we have to set the NO_SIGNER_CERT_VERIFY
		 * until such time we implement the keyring_path as a X509_STORE
		 */

		if (!CMS_verify(cms, certs, NULL, plaintext_bio, NULL,
				CMS_DETACHED | CMS_NO_SIGNER_CERT_VERIFY | CMS_BINARY)) {
			pb_log("%s: Failed OpenSSL CMS verify:\n", __func__);
			ERR_print_errors_cb(&pb_log_print_errors_cb, NULL);
			goto out;
		}

		nok = 0;

	} else {

		/* for explicit dgst mode we need an explicit md defined */
		if (!s_verify_md)
			goto out;

		ctx = EVP_MD_CTX_create();

		if (!ctx) {
			pb_log("%s: Error allocating OpenSSL MD ctx:\n", __func__);
			ERR_print_errors_cb(&pb_log_print_errors_cb, NULL);
			goto out;
		}

		pkey = get_public_key(authorized_signatures_handle);
		if (!pkey)
			goto out;

		if (EVP_DigestVerifyInit(ctx, NULL, s_verify_md, NULL, pkey) < 1) {
			pb_log("%s: Error initializing OpenSSL verify:\n", __func__);
			ERR_print_errors_cb(&pb_log_print_errors_cb, NULL);
			goto out;
		}

		while (bytes_read) {
			bytes_read = BIO_read(plaintext_bio, rdbuf, 8192);
			if (bytes_read > 0) {
				if (EVP_DigestVerifyUpdate(ctx, rdbuf, (size_t)(bytes_read)) < 1) {
					pb_log("%s: OpenSSL digest update failure on file '%s':\n",
					       __func__, plaintext_filename);
					ERR_print_errors_cb(&pb_log_print_errors_cb, NULL);
					goto out;
				}
			} else if (bytes_read < 0) {
				pb_log("%s: OpenSSL read failure on file '%s':\n",
				       __func__, plaintext_filename);
				ERR_print_errors_cb(&pb_log_print_errors_cb, NULL);
				goto out;
			}
		}

		/*
		 * can't do signature buffer as an update so have to read in whole file
		 * would be handy if there was some sort of BIO_read_all but there
		 * doesn't seem to be so rather than reinvent the wheel close it and
		 * use the existing support
		 */
		BIO_free(signature_bio);
		signature_bio = NULL;

		if (read_file(NULL, signature_filename, &sigbuf, &siglen)) {
			pb_log("%s: Error reading OpenSSL signature file '%s'\n",
			       __func__, signature_filename);
			goto out;
		}

		if (EVP_DigestVerifyFinal(ctx, (unsigned char*)sigbuf, siglen))
			nok = 0;
		else {
			pb_log("%s: Error finalizing OpenSSL verify:\n", __func__);
			ERR_print_errors_cb(&pb_log_print_errors_cb, NULL);
		}
	}

out:
	if (cms)
		CMS_ContentInfo_free(cms);
	talloc_free(sigbuf);
	sk_X509_free(certs);
	BIO_free(plaintext_bio);
	BIO_free(signature_bio);
	BIO_free(content_bio);
	EVP_PKEY_free(pkey);
	EVP_MD_CTX_destroy(ctx);
	return nok;
}

int lockdown_status(void)
{
	/*
	 * if it's a PKCS12 then we're in decrypt mode since we have the
	 * private key, otherwise it's sign mode
	 *
	 * someday add in support for runtime determination based on what
	 * files come back in the async sig file load?
	 */
	FILE *authorized_signatures_handle = NULL;
	int ret = PB_LOCKDOWN_SIGN;
	PKCS12 *p12 = NULL;

#if !defined(HARD_LOCKDOWN)
	if (access(LOCKDOWN_FILE, F_OK) == -1)
		return PB_LOCKDOWN_NONE;
#endif

	/* determine lockdown type */

	authorized_signatures_handle = fopen(LOCKDOWN_FILE, "r");
	if (authorized_signatures_handle) {
		p12 = d2i_PKCS12_fp(authorized_signatures_handle, NULL);
		if (p12) {
			ret = PB_LOCKDOWN_DECRYPT;
			PKCS12_free(p12);
		}
		fclose(authorized_signatures_handle);
	}

	return ret;
}

