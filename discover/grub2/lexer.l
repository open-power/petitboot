
%{
#include "grub2.h"
#include "parser.h"
#include <talloc/talloc.h>
%}

%option nounput noinput
%option batch never-interactive
%option warn
%option noyywrap
%option stack noyy_top_state
%option reentrant
%option bison-bridge
%option yylineno
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
"else"       return TOKEN_ELSE;
"esac"       return TOKEN_ESAC;
"fi"         return TOKEN_FI;
"for"        return TOKEN_FOR;
"function"   return TOKEN_FUNCTION;
"if"         return TOKEN_IF;
"in"         return TOKEN_IN;
"menuentry"  return TOKEN_MENUENTRY;
"select"     return TOKEN_SELECT;
"submenu"    return TOKEN_SUBMENU;
"then"       return TOKEN_THEN;
"time"       return TOKEN_TIME;
"until"      return TOKEN_UTIL;
"while"      return TOKEN_WHILE;

 /* anything that's not a metachar: return as a plain word */
{WORD}	{
		yylval->word = create_word_text(yyget_extra(yyscanner), yytext);
		return TOKEN_WORD;
	}

\${VARNAME} |
\$\{{VARNAME}\} {
		if (yytext[1] == '{') {
			yytext[yyleng-1] = '\0';
			yytext++;
		}
		yytext++;
		yylval->word = create_word_var(yyget_extra(yyscanner), yytext,
						true);
		return TOKEN_WORD;
	}

 /* single-quoted strings: return a single, non-expanded word token */
\'	{
		yy_push_state(sqstring, yyscanner);
	}
<sqstring>\' {
		yy_pop_state(yyscanner);
	}
<sqstring>[^']+ {
		yylval->word = create_word_text(yyget_extra(yyscanner), yytext);
		return TOKEN_WORD;
	}

 /* double-quoted strings: return a single, expanded word token */
\"	{
		yy_push_state(dqstring, yyscanner);
	}
<dqstring>\" {
		yy_pop_state(yyscanner);
	}
<dqstring>([^"\$]|\\\")+ {
		yylval->word = create_word_text(yyget_extra(yyscanner), yytext);
		return TOKEN_WORD;
	}
<dqstring>\${VARNAME} |
<dqstring>\$\{{VARNAME}\} {
		if (yytext[1] == '{') {
			yytext[yyleng-1] = '\0';
			yytext++;
		}
		yytext++;
		yylval->word = create_word_var(yyget_extra(yyscanner), yytext,
						false);
		return TOKEN_WORD;
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
