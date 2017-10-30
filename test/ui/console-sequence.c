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

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

#include "talloc/talloc.h"

#define ERR     (-1)
#define pb_log(...)	printf(__VA_ARGS__)
#define pb_debug(...)	printf(__VA_ARGS__)

/*
 * Several example terminal commands, see:
 * https://vt100.net/docs/vt100-ug/chapter3.html
 */
static char identify_rsp[] = {033, 0133, 077, 061, 073, 060, 0143, '\0'};
static char decdwl[] = {033, 043, 066, '\0'};
static char attrs[] = {033, 0133, 060, 073, 064, 073, 065, 0155, '\0'};
static char cursor[] = {033, 070, '\0'};
static char conf_test[] = {033, 0133, 062, 073, 061, 0171, '\0'};
static char status[] = {033, 0133, 060, 0156, '\0'};
static char erase_screen[] = {033, 0133, 062, 0112, '\0'};
static char status_ok_rsp[] = {033, 0133, 064, 0156, '\0'};
static char garbage[] = {001, 002, 003, 004, '\0'};
static char esc_garbage[] = {033, 002, 003, 004, '\0'};
static char *ptr;

static signed char getch(void)
{
	if (!ptr || *ptr == '\0')
		return -ERR;
	return *ptr++;
}

#include "ui/ncurses/console-codes.c"

int main(void)
{
	void *ctx;
	char *seq, *confused;

	ctx = talloc_new(NULL);

	ptr = &identify_rsp[1];
	printf("Identity response\n");
	seq = handle_control_sequence(ctx, identify_rsp[0]);
	assert(strncmp(seq, identify_rsp, strlen(identify_rsp)) == 0);

	printf("DECDWL\n");
	ptr = &decdwl[1];
	seq = handle_control_sequence(ctx, decdwl[0]);
	assert(strncmp(seq, decdwl, strlen(decdwl)) == 0);

	printf("Attributes\n");
	ptr = &attrs[1];
	seq = handle_control_sequence(ctx, attrs[0]);
	assert(strncmp(seq, attrs, strlen(attrs)) == 0);

	printf("Reset Cursor\n");
	ptr = &cursor[1];
	seq = handle_control_sequence(ctx, cursor[0]);
	assert(strncmp(seq, cursor, strlen(cursor)) == 0);

	printf("Confidence Test\n");
	ptr = &conf_test[1];
	seq = handle_control_sequence(ctx, conf_test[0]);
	assert(strncmp(seq, conf_test, strlen(conf_test)) == 0);

	printf("Status\n");
	ptr = &status[1];
	seq = handle_control_sequence(ctx, status[0]);
	assert(strncmp(seq, status, strlen(status)) == 0);

	printf("Erase Screen\n");
	ptr = &erase_screen[1];
	seq = handle_control_sequence(ctx, erase_screen[0]);
	assert(strncmp(seq, erase_screen, strlen(erase_screen)) == 0);

	printf("Status Response\n");
	ptr = &status_ok_rsp[1];
	seq = handle_control_sequence(ctx, status_ok_rsp[0]);
	assert(strncmp(seq, status_ok_rsp, strlen(status_ok_rsp)) == 0);

	printf("Garbage\n");
	ptr = &garbage[1];
	seq = handle_control_sequence(ctx, garbage[0]);
	assert(seq == NULL);

	printf("ESC then Garbage\n");
	ptr = &esc_garbage[1];
	confused = talloc_asprintf(ctx, "%c%c...", 033, 002);
	seq = handle_control_sequence(ctx, esc_garbage[0]);
	assert(strncmp(seq, confused, strlen(confused)) == 0);

	talloc_free(ctx);
	return EXIT_SUCCESS;
}
