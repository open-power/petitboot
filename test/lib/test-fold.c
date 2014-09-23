
#define _GNU_SOURCE

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <locale.h>
#include <wchar.h>
#include <langinfo.h>

#include <fold/fold.h>
#include <list/list.h>
#include <talloc/talloc.h>

struct line {
	const char		*buf;
	unsigned int		len;
	struct list_item	list;
};

struct ctx {
	struct list		lines;
};

struct test {
	const char	*in;
	unsigned int	linelen;
	const char	*out[];
};

/* split on newline boundaries, no actual folding */
struct test test_split = {
	.in	= "Lorem ipsum dolor\nsit amet,\nconsectetuer\n",
	.linelen = 20,
	.out = {
		"Lorem ipsum dolor",
		"sit amet,",
		"consectetuer",
		"",
		NULL,
	},
};

/* fold a long line */
struct test test_fold_line = {
	.in = "Lorem ipsum dolor sit amet, consectetuer adipiscing "
		"elit, sed diam nonummy nibh euismod tincidunt ut "
		"laoreet dolore magna aliquam erat volutpat.",
	.linelen = 20,
	.out = {
		"Lorem ipsum dolor",
		"sit amet,",
		"consectetuer",
		"adipiscing elit,",
		"sed diam nonummy",
		"nibh euismod",
		"tincidunt ut",
		"laoreet dolore",
		"magna aliquam erat",
		"volutpat.",
		NULL
	},
};

/* break a word */
struct test test_break = {
	.in = "Lorem ipsum dolor sit amet, consectetuer",
	.linelen = 10,
	.out = {
		"Lorem",
		"ipsum",
		"dolor sit",
		"amet,",
		"consectetu",
		"er",
		NULL
	},
};

struct test test_mbs = {
	.in = "從主功能表畫面中，選取啟動選項。",
	.linelen = 15,
	.out = {
		"從主功能表畫面",
		"中，選取啟動選",
		"項。",
		NULL,
	},
};

static struct test *tests[] = {
	&test_split, &test_fold_line, &test_break, &test_mbs,
};

static void __attribute__((noreturn)) fail(struct ctx *ctx,
		struct test *test, const char *msg)
{
	struct line *line;
	int i;

	fprintf(stderr, "%s\n", msg);
	fprintf(stderr, "input, split at %d:\n%s\n", test->linelen, test->in);

	fprintf(stderr, "expected:\n");
	for (i = 0; test->out[i]; i++)
		fprintf(stderr, "  '%s'\n", test->out[i]);

	fprintf(stderr, "actual:\n");
	list_for_each_entry(&ctx->lines, line, list) {
		char *buf = talloc_strndup(ctx, line->buf, line->len);
		fprintf(stderr, "  '%s'\n", buf);
		talloc_free(buf);
	}

	exit(EXIT_FAILURE);
}

static int fold_line_cb(void *arg, const char *start, int len)
{
	struct ctx *ctx = arg;
	struct line *line;

	line = talloc(ctx, struct line);
	line->buf = start;
	line->len = len;
	list_add_tail(&ctx->lines, &line->list);

	return 0;
}

static void run_test(struct test *test)
{
	struct line *line;
	struct ctx *ctx;
	wchar_t *wcs;
	int i, n;

	ctx = talloc(NULL, struct ctx);
	n = strlen(test->in) + 1;
	list_init(&ctx->lines);
	fold_text(test->in, test->linelen, fold_line_cb, ctx);


	i = 0;
	list_for_each_entry(&ctx->lines, line, list) {
		size_t wcslen;
		char *buf;
		int width;

		if (!test->out[i])
			fail(ctx, test,
				"fold_text returned more lines than expected");

		buf = talloc_strndup(ctx, line->buf, line->len);
		wcslen = mbstowcs(NULL, buf, 0);

		if (wcslen == (size_t)-1)
			fail(ctx, test, "invalid mutlibyte sequence");

		wcs = talloc_array(ctx, wchar_t, wcslen + 1);
		wcslen = mbstowcs(wcs, buf, n);

		width = wcswidth(wcs, wcslen);
		if (width == -1)
			fail(ctx, test, "nonprintable characters present");

		if (width > (signed int)test->linelen)
			fail(ctx, test, "line too long");

		if (line->len != strlen(test->out[i]))
			fail(ctx, test, "line lengths differ");

		if (strncmp(line->buf, test->out[i], line->len))
			fail(ctx, test, "line data differs");

		i++;
	}

	if (test->out[i])
		fail(ctx, test, "fold_text returned fewer lines than expected");

	talloc_free(ctx);
}

int main(void)
{
	unsigned int i;
	char *charset;

	setlocale(LC_CTYPE, "");

	charset = nl_langinfo(CODESET);
	if (strcmp(charset, "UTF-8")) {
		fprintf(stderr, "Current charset is %s, tests require UTF-8\n",
				charset);
		return EXIT_FAILURE;
	}

	for (i = 0; i < ARRAY_SIZE(tests); i++)
		run_test(tests[i]);

	return EXIT_SUCCESS;
}
