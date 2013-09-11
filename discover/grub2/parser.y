
%pure-parser
%lex-param { yyscan_t scanner }
%parse-param { struct grub2_parser *parser }

%{
#include "grub2.h"
#include "parser.h"
#include "lexer.h"

#define YYLEX_PARAM parser->scanner

static void yyerror(struct grub2_parser *, char const *s);
%}

%union {
	struct grub2_word *word;
}

/* reserved words */
%token	TOKEN_LDSQBRACKET	"[["
%token	TOKEN_RDSQBRACKET	"]]"
%token	TOKEN_CASE		"case"
%token	TOKEN_DO		"do"
%token	TOKEN_DONE		"done"
%token	TOKEN_ELIF		"elif"
%token	TOKEN_ESAC		"esac"
%token	TOKEN_FI		"fi"
%token	TOKEN_FOR		"for"
%token	TOKEN_FUNCTION		"function"
%token	TOKEN_IF		"if"
%token	TOKEN_IN		"in"
%token	TOKEN_MENUENTRY		"menuentry"
%token	TOKEN_SELECT		"select"
%token	TOKEN_THEN		"then"
%token	TOKEN_TIME		"time"
%token	TOKEN_UTIL		"until"
%token	TOKEN_WHILE		"while"

/* syntax */
%token	TOKEN_EOL
%token	TOKEN_DELIM
%token	TOKEN_WORD

%start	script
%debug

%%

script: statements
	;

statements: statement
	| statements statement
	;

statement: TOKEN_EOL
	| words TOKEN_EOL
	| '{' statements '}'
	| "if" TOKEN_DELIM statement
		"then" TOKEN_EOL
		statements
		"fi" TOKEN_EOL
	| "menuentry" TOKEN_DELIM words TOKEN_DELIM
		'{' statements '}'
		TOKEN_EOL
	;

words:	| word
	| words TOKEN_DELIM word
	;

word:	TOKEN_WORD
	| word TOKEN_WORD
	;

%%
void yyerror(struct grub2_parser *parser, char const *s)
{
	fprintf(stderr, "%d: error: %s '%s'\n",
			yyget_lineno(parser->scanner),
			s, yyget_text(parser->scanner));
}
