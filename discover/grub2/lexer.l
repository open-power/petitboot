
%{
#include "parser.h"
#include <talloc/talloc.h>
%}

%option nounput noinput
%option warn
%option noyywrap
%option stack noyy_top_state
%option reentrant
%option bison-bridge
%option noyyalloc noyyfree noyyrealloc
%option extra-type="struct grub2_parser *"
%option header-file="lexer.h"
%option outfile="lexer.c"

%x sqstring
%x dqstring

WORD	[^{}|&$;<> \t\n'"]+
VARNAME ([[:alpha:]][_[:alnum:]]*|[0-9]|[\?@\*#])

%%

 /* discard leading & trailing whitespace, but keep inter-word delimeters */
^[ \t]+	;
[ \t]+$	;
[ \t]*	return TOKEN_DELIM;

 /* reserved words */
"[["         return TOKEN_LDSQBRACKET;
"]]"         return TOKEN_RDSQBRACKET;
"case"       return TOKEN_CASE;
"do"         return TOKEN_DO;
"done"       return TOKEN_DONE;
"elif"       return TOKEN_ELIF;
"esac"       return TOKEN_ESAC;
"fi"         return TOKEN_FI;
"for"        return TOKEN_FOR;
"function"   return TOKEN_FUNCTION;
"if"         return TOKEN_IF;
"in"         return TOKEN_IN;
"menuentry"  return TOKEN_MENUENTRY;
"select"     return TOKEN_SELECT;
"then"       return TOKEN_THEN;
"time"       return TOKEN_TIME;
"until"      return TOKEN_UTIL;
"while"      return TOKEN_WHILE;

 /* anything that's not a metachar: return as a plain word */
{WORD}	{
		yylval->strval = talloc_strdup(yyscanner, yytext);
		yylval->expand = 0;
		return TOKEN_WORD;
	}

\${VARNAME} |
\$\{{VARNAME}\} {
		yylval->strval = talloc_strdup(yyscanner, yytext);
		yylval->expand = 1;
		yylval->split = 1;
		return TOKEN_WORD;
	}

 /* single-quoted strings: return a single, non-expanded word token */
\'	{
		yy_push_state(sqstring, yyscanner);
	}
<sqstring>\' {
		yy_pop_state(yyscanner);
		return TOKEN_WORD;
	}
<sqstring>[^']+ {
		yylval->expand = 0;
		yylval->split = 0;
		yylval->strval = talloc_strdup(yyscanner, yytext);
	}

 /* double-quoted strings: return a single, expanded word token */
\"	{
		yy_push_state(dqstring, yyscanner);
	}
<dqstring>\" {
		yy_pop_state(yyscanner);
		return TOKEN_WORD;
	}
<dqstring>([^"]|\\\")+ {
		yylval->expand = 1;
		yylval->split = 0;
		yylval->strval = talloc_strdup(yyscanner, yytext);
	}


 /* blocks */
"{"	return '{';
"}"	return '}';

 /* end-of-line */
[ \t]*(;|\n)[ \t]*	return TOKEN_EOL;

 /* strip comments */
#.*$	;


.	printf("unknown token '%s'\n", yytext); exit(1);

%%

struct grub2_parser;

void *yyalloc(size_t bytes, void *yyscanner)
{
	struct grub2_parser *parser = yyget_extra(yyscanner);
	return talloc_size(parser, bytes);
}

void *yyrealloc(void *ptr, size_t bytes, void *yyscanner)
{
	struct grub2_parser *parser = yyget_extra(yyscanner);
	return talloc_realloc_size(parser, ptr, bytes);
}

void yyfree(void *ptr, void *yyscanner __attribute__((unused)))
{
	talloc_free(ptr);
}
