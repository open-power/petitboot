#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>
#include <sys/stat.h>

#include <log/log.h>
#include <talloc/talloc.h>
#include <file/file.h>
#include <security/security.h>

#define SECURITY_TEST_DATA_DIR  TEST_LIB_DATA_BASE "/security/"

int main(void)
{
	char *verify_data = NULL;
	char *compare_data = NULL;
	char *filename = NULL;
	FILE *keyfile = NULL;
	int ret = EXIT_FAILURE;
	int verify_len;
	int compare_len;

	pb_log_init(stdout);

	keyfile = fopen(SECURITY_TEST_DATA_DIR "cert.p12", "r");
	if (!keyfile)
		return EXIT_FAILURE;

	if (read_file(NULL, SECURITY_TEST_DATA_DIR "rootdata.txt", &verify_data, &verify_len))
		goto out;

	/* first basic CMS decrypt case */

	/*
	 * these calls overwrite so need a temp file
	 * copy_file_secure_dest is having some permission issues
	 */
	if (copy_file_secure_dest(NULL,
				  SECURITY_TEST_DATA_DIR "rootdata.cmsencver",
				  &filename))
		goto out;

	if (decrypt_file(filename, keyfile, NULL))
		goto out;

	if (read_file(verify_data, filename, &compare_data, &compare_len))
		goto out;

	if (verify_len != compare_len)
		goto out;

	if (memcmp(verify_data, compare_data, verify_len))
		goto out;

	/* check an encrypted but unverified message fails */
	unlink(filename);
	talloc_free(filename);

	if (copy_file_secure_dest(NULL,
				  SECURITY_TEST_DATA_DIR "rootdata.cmsenc",
				  &filename))
		goto out;


	if (!decrypt_file(filename, keyfile, NULL))
		goto out;

	/* got here, all fine */
	ret = EXIT_SUCCESS;

out:
	if (keyfile)
		fclose(keyfile);
	if (filename) {
		unlink(filename);
		talloc_free(filename);
	}
	talloc_free(verify_data);
	return ret;
}
