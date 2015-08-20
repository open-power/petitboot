
#define _GNU_SOURCE

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <wchar.h>
#include <wctype.h>

#include "fold/fold.h"

void fold_text(const char *text,
		int linelen,
		int line_cb(void *arg, const char *start, int len),
		void *arg)
{
	const char *start, *end, *sep;
	size_t sep_bytes, len;
	int col, rc = 0;
	mbstate_t ps;

	/* start, end and sep are byte-positions in the string, and should always
	 * lie on the start of a multibyte sequence */
	start = end = sep = text;
	sep_bytes = 0;
	col = 0;
	len = strlen(text);
	memset(&ps, 0, sizeof(ps));

	while (!rc) {
		size_t bytes;
		wchar_t wc;
		int width;

		bytes = mbrtowc(&wc, end, len - (end - text), &ps);

		assert(bytes != (size_t)-1);

		/* we'll get a zero size for the nul terminator, or (size_t) -2
		 * if we've reached the end of the buffer */
		if (!bytes || bytes == (size_t) -2) {
			line_cb(arg, start, end - start);
			break;
		}

		if (wc == L'\n') {
			rc = line_cb(arg, start, end - start);
			start = sep = end += bytes;
			sep_bytes = 0;
			col = 0;
			continue;
		}

		width = wcwidth(wc);

		/* we should have caught this in the !bytes check... */
		if (width == 0) {
			line_cb(arg, start, end - start);
			break;
		}

		/* unprintable character? just add it to the current line */
		if (width < 0) {
			end += bytes;
			continue;
		}

		col += width;

		if (col > linelen) {
			if (sep != start) {
				/* split on a previous word boundary, if
				 * possible */
				rc = line_cb(arg, start, sep - start);
				end = sep + sep_bytes;
			} else {
				/* otherwise, break the word */
				rc = line_cb(arg, start, end - start);
			}
			sep_bytes = 0;
			start = sep = end;
			col = 0;

		} else {
			/* record our last separator */
			if (wc == L' ') {
				sep = end;
				sep_bytes = bytes;
			}
			end += bytes;
		}
	}
}
