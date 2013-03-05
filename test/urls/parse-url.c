
#include <stdlib.h>
#include <stdio.h>

#include <url/url.h>
#include <log/log.h>

int main(int argc, char **argv)
{
	struct pb_url *url;
	FILE *null;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s <URL>\n", argv[0]);
		return EXIT_FAILURE;
	}

	/* discard log output */
	null = fopen("/dev/null", "w");
	pb_log_set_stream(null);

	url = pb_url_parse(NULL, argv[1]);
	if (!url)
		return EXIT_FAILURE;

	printf("%s\n", argv[1]);
	printf("scheme\t%s\n", pb_url_scheme_name(url->scheme));
	printf("host\t%s\n", url->host);
	printf("port\t%s\n", url->port);
	printf("path\t%s\n", url->path);
	printf("dir\t%s\n", url->dir);
	printf("file\t%s\n", url->file);

	return EXIT_SUCCESS;
}
