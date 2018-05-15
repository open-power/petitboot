#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>
#include <sys/stat.h>

#include <log/log.h>
#include <security/security.h>

#define SECURITY_TEST_DATA_DIR  TEST_LIB_DATA_BASE "/security/"
#define SECURITY_TEST_DATA_CERT SECURITY_TEST_DATA_DIR "/cert.pem"

int main(void)
{
	FILE *keyfile;

	pb_log_init(stdout);

	/* start with basic pubkey extraction */
	keyfile = fopen(SECURITY_TEST_DATA_DIR "cert.pem", "r");
	if (!keyfile)
		return EXIT_FAILURE;

	/* first basic verify case */
	/* assuming the default sha256 mode */

	if (verify_file_signature(SECURITY_TEST_DATA_DIR "rootdata.txt",
				  SECURITY_TEST_DATA_DIR "rootdatasha256.sig",
				  keyfile,
				  NULL))
	{
		fclose(keyfile);
		return EXIT_FAILURE;
	}

	/* now check different file */

	if (!verify_file_signature(SECURITY_TEST_DATA_DIR "rootdata_different.txt",
				   SECURITY_TEST_DATA_DIR "rootdatasha256.sig",
				   keyfile,
				   NULL))
	{
		fclose(keyfile);
		return EXIT_FAILURE;
	}

	/* now check different signature */

	if (!verify_file_signature(SECURITY_TEST_DATA_DIR "rootdata.txt",
				   SECURITY_TEST_DATA_DIR "rootdatasha512.sig",
				   keyfile,
				   NULL))
	{
		fclose(keyfile);
		return EXIT_FAILURE;
	}

	/* check CMS verify */
	if (verify_file_signature(SECURITY_TEST_DATA_DIR "rootdata.txt",
				  SECURITY_TEST_DATA_DIR "rootdata.cmsver",
				  keyfile,
				  NULL))
	{
		fclose(keyfile);
		return EXIT_FAILURE;
	}

	fclose(keyfile);

	/* now check basic pubkey fallback */
	keyfile = fopen(SECURITY_TEST_DATA_DIR "pubkey.pem", "r");
	if (!keyfile)
		return EXIT_FAILURE;

	if (verify_file_signature(SECURITY_TEST_DATA_DIR "rootdata.txt",
				  SECURITY_TEST_DATA_DIR "rootdatasha256.sig",
				  keyfile,
				  NULL))
	{
		fclose(keyfile);
		return EXIT_FAILURE;
	}

	fclose(keyfile);

	/* finally check different key */
	keyfile = fopen(SECURITY_TEST_DATA_DIR "wrong_cert.pem", "r");
	if (!keyfile)
		return EXIT_FAILURE;

	if (!verify_file_signature(SECURITY_TEST_DATA_DIR "rootdata.txt",
				   SECURITY_TEST_DATA_DIR "rootdatasha256.sig",
				   keyfile,
				   NULL))
	{
		fclose(keyfile);
		return EXIT_FAILURE;
	}


	fclose(keyfile);
	return EXIT_SUCCESS;
}
