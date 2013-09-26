
#include <stdlib.h>
#include <stdio.h>

#include <talloc/talloc.h>
#include <url/url.h>
#include <log/log.h>

int main(int argc, char **argv)
{
	struct pb_url *url;
	void *ctx;

	if (argc != 2 && argc != 3) {
		fprintf(stderr, "Usage: %s <URL> [update]\n", argv[0]);
		return EXIT_FAILURE;
	}

	ctx = talloc_new(NULL);

	url = pb_url_parse(ctx, argv[1]);
	if (!url)
		return EXIT_FAILURE;

	if (argc == 2) {
		printf("%s\n", argv[1]);

	} else {
		struct pb_url *new_url;
		printf("%s %s\n", argv[1], argv[2]);
		new_url = pb_url_join(ctx, url, argv[2]);
		talloc_free(url);
		url = new_url;
	}

	printf("scheme\t%s\n", pb_url_scheme_name(url->scheme));
	printf("host\t%s\n", url->host);
	printf("port\t%s\n", url->port);
	printf("path\t%s\n", url->path);
	printf("dir\t%s\n", url->dir);
	printf("file\t%s\n", url->file);

	talloc_free(ctx);

	return EXIT_SUCCESS;
}
