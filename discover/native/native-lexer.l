%{
#include "native.h"
#include "native-parser.h"
#include <talloc/talloc.h>

#define YYSTYPE NSTYPE

void yyerror(struct native_parser *parser, const char *fmt, ...);
%}

%option nounput noinput
%option batch never-interactive
%option warn
%option noyywrap
%option reentrant
%option bison-bridge
%option yylineno
%option noyyalloc noyyfree noyyrealloc
%option extra-type="struct native_parser *"
%option prefix="n"

%x label
%x args

DELIM	[ \t]+
NUMBER	0|[1-9][0-9]*
WORDS	[^\n]+
NEWLINE [\n]+

%%

name		{ BEGIN(label); return TOKEN_NAME; }
image		{ BEGIN(label); return TOKEN_IMAGE; }
initrd		{ BEGIN(label); return TOKEN_INITRD; }
args		{ BEGIN(label); return TOKEN_ARGS; }
dtb		{ BEGIN(label); return TOKEN_DTB; }
description	{ BEGIN(label); return TOKEN_DESCRIPTION; }
default		{ BEGIN(label); return TOKEN_DEFAULT; }
dev_description { BEGIN(label); return TOKEN_DEV_DESCRIPTION; }
{DELIM}		{ ; }
{NEWLINE}	{ ; }
<label>{DELIM}	{ BEGIN(args); return TOKEN_DELIM; }
<args>{WORDS}	{ yylval->word = strdup(yytext); return TOKEN_WORD; }
<args>{NEWLINE}	{ BEGIN(INITIAL); ; }

%%

struct native_parser;

void *yyalloc(size_t bytes, void *yyscanner)
{
	struct native_parser *parser = yyget_extra(yyscanner);
	return talloc_size(parser, bytes);
}

void *yyrealloc(void *ptr, size_t bytes, void *yyscanner)
{
	struct native_parser *parser = yyget_extra(yyscanner);
	return talloc_realloc_size(parser, ptr, bytes);
}

void yyfree(void *ptr, void *yyscanner __attribute__((unused)))
{
	talloc_free(ptr);
}
