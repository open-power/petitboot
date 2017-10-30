/*
 *  Copyright (C) 2017 IBM Corporation
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
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "talloc/talloc.h"

#ifndef PETITBOOT_TEST
#include "log/log.h"
#include "nc-scr.h"
#endif

#include "console-codes.h"

#define ESC_CHAR			033
#define CSI_CHAR			'['
#define INTER_CHAR_START		040
#define INTER_CHAR_END			057
#define ESC_SEQUENCE_FINAL_START	060
#define ESC_SEQUENCE_FINAL_END		0176
#define CTRL_SEQUENCE_FINAL_START	0100
#define CTRL_SEQUENCE_FINAL_END		0176
#define DEC_PARAMETER			077

enum console_sequence_state {
	CONSOLE_STATE_START,
	CONSOLE_STATE_ESC_SEQ,
	CONSOLE_STATE_CTRL_SEQ_START,
	CONSOLE_STATE_CTRL_SEQ,
	CONSOLE_STATE_DONE,
	CONSOLE_STATE_CONFUSED,
};

static inline bool is_intermediate(signed char c)
{
	return c > INTER_CHAR_START && c < INTER_CHAR_END;
}

static inline bool is_parameter(signed char c)
{
	return (c >= 060 && c <= 071) || c == 073;
}

static inline bool is_escape_final(signed char c)
{
	return c >= ESC_SEQUENCE_FINAL_START && c < ESC_SEQUENCE_FINAL_END;
}

static inline bool is_control_final(signed char c)
{
	return c >= CTRL_SEQUENCE_FINAL_START && c <= CTRL_SEQUENCE_FINAL_END;
}

static char console_sequence_getch(char **sequence)
{
	signed char c = getch();

	if (c != ERR)
		*sequence = talloc_asprintf_append(*sequence, "%c", c);
	return c;
}

/*
 * Catch terminal control sequences that have accidentally been sent to
 * Petitboot. These are of the form
 * 	ESC I .. I F
 * where I is an Intermediate Character and F is a Final Character, eg:
 * 	ESC ^ [ ? 1 ; 0 c
 * or	ESC # 6
 *
 * This is based off the definitions provided by
 * https://vt100.net/docs/vt100-ug/contents.html
 */
char *handle_control_sequence(void *ctx, signed char start)
{
	enum console_sequence_state state = CONSOLE_STATE_START;
	bool in_sequence = true;
	signed char c;
	char *seq;

	if (start != ESC_CHAR) {
		pb_log("%s: Called with non-escape character: 0%o\n",
				__func__, start);
		return NULL;
	}

	seq = talloc_asprintf(ctx, "%c", start);

	while (in_sequence) {
		switch (state) {
		case CONSOLE_STATE_START:
			c = console_sequence_getch(&seq);
			if (c == CSI_CHAR)
				state = CONSOLE_STATE_CTRL_SEQ_START;
			else if (is_intermediate(c))
				state = CONSOLE_STATE_ESC_SEQ;
			else if (is_escape_final(c))
				state = CONSOLE_STATE_DONE;
			else if (c != ERR) {
				/* wait on c == ERR */
				pb_debug("Unexpected start: \\x%x\n", c);
				state = CONSOLE_STATE_CONFUSED;
			}
			break;
		case CONSOLE_STATE_ESC_SEQ:
			c = console_sequence_getch(&seq);
			if (is_intermediate(c))
				state = CONSOLE_STATE_ESC_SEQ;
			else if (is_escape_final(c))
				state = CONSOLE_STATE_DONE;
			else if (c != ERR) {
				/* wait on c == ERR */
				pb_debug("Unexpected character after intermediate: \\x%x\n",
						c);
				state = CONSOLE_STATE_CONFUSED;
			}
			break;
		case CONSOLE_STATE_CTRL_SEQ_START:
			c = console_sequence_getch(&seq);
			if (is_intermediate(c) || is_parameter(c) ||
					c == DEC_PARAMETER)
				state = CONSOLE_STATE_CTRL_SEQ;
			else if (is_control_final(c))
				state = CONSOLE_STATE_DONE;
			else if (c != ERR) {
				/* wait on c == ERR */
				pb_debug("Unexpected character in param string:  \\x%x\n",
						c);
				state = CONSOLE_STATE_CONFUSED;
			}
			break;
		case CONSOLE_STATE_CTRL_SEQ:
			c = console_sequence_getch(&seq);
			if (is_intermediate(c) || is_parameter(c))
				state = CONSOLE_STATE_CTRL_SEQ;
			else if (is_control_final(c))
				state = CONSOLE_STATE_DONE;
			else if (c != ERR) {
				/* wait on c == ERR */
				pb_debug("Unexpected character in param string:  \\x%x\n",
						c);
				state = CONSOLE_STATE_CONFUSED;
			}
			break;
		case CONSOLE_STATE_DONE:
			in_sequence = false;
			break;
		case CONSOLE_STATE_CONFUSED:
			/* fall-through */
		default:
			pb_debug("We got lost interpreting a control sequence!\n");
			seq = talloc_asprintf_append(seq, "...");
			in_sequence = false;
			break;
		};
	}

	return seq;
}
