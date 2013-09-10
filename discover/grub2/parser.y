
%pure-parser
%lex-param { yyscan_t scanner }
%parse-param { struct grub2_parser *parser }

%{
#include "lexer.h"
%}

%union {
	struct {
		char	*strval;
		int	expand;
		int	split;
	};
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

%%

script: /* empty */

%%
