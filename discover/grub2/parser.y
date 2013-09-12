
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
	struct grub2_word	*word;
	struct grub2_argv	*argv;
	struct grub2_statement	*statement;
	struct grub2_statements	*statements;
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

%type <statement>	statement
%type <statements>	statements
%type <argv>		words
%type <word>		word

/* syntax */
%token	TOKEN_EOL
%token	TOKEN_DELIM
%token	<word> TOKEN_WORD

%start	script
%debug

%%

script: statements {
		parser->statements = $1;
	}

statements: statement {
		$$ = create_statements(parser);
		statement_append($$, $1);
	}
	| statements statement {
		statement_append($1, $2);
	}

statement: TOKEN_EOL {
		$$ = NULL;
	}
	| words TOKEN_EOL {
		   $$ = create_statement_simple(parser, $1);
	}
	| '{' statements '}' {
		$$ = create_statement_block(parser, $2);
	}
	| "if" TOKEN_DELIM statement
		"then" TOKEN_EOL
		statements
		"fi" TOKEN_EOL {
		$$ = create_statement_if(parser, $3, $6, NULL);
	}
	| "menuentry" TOKEN_DELIM words TOKEN_DELIM
		'{' statements '}'
		TOKEN_EOL {
		$$ = create_statement_menuentry(parser, $3, $6);
	}

words:	word {
		$$ = create_argv(parser);
		argv_append($$, $1);
	}
	| words TOKEN_DELIM word {
		argv_append($1, $3);
		$$ = $1;
	}

word:	TOKEN_WORD
	| word TOKEN_WORD {
		word_append($1, $2);
		$$ = $1;
	}

%%
void yyerror(struct grub2_parser *parser, char const *s)
{
	fprintf(stderr, "%d: error: %s '%s'\n",
			yyget_lineno(parser->scanner),
			s, yyget_text(parser->scanner));
}
