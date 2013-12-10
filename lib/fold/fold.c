
#include "fold/fold.h"

void fold_text(const char *text,
		int linelen,
		int line_cb(void *arg, const char *start, int len),
		void *arg)
{
	const char *start, *end, *sep;
	int rc = 0;

	start = end = sep = text;

	while (!rc) {

		if (*end == '\n') {
			rc = line_cb(arg, start, end - start);
			start = sep = ++end;

		} else if (*end == '\0') {
			line_cb(arg, start, end - start);
			rc = 1;

		} else if (end - start >= linelen - 1) {
			if (sep != start) {
				/* split on a previous word boundary, if
				 * possible */
				rc = line_cb(arg, start, sep - start);
				start = end = ++sep;
			} else {
				/* otherwise, break the word */
				end++;
				rc = line_cb(arg, start, end - start);
				start = sep = end;
			}

		} else {
			end++;
			/* record our last separator */
			if (*end == ' ')
				sep = end;
		}
	}
}
