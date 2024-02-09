
%define api.pure
%lex-param { yyscan_t scanner }
%parse-param { struct grub2_parser *parser }
%parse-param { void *scanner }
%define parse.error verbose

%{
#include <talloc/talloc.h>
#include <log/log.h>

#include "grub2.h"

void yyerror(struct grub2_parser *parser, void *scanner, const char *fmt, ...);
%}

%union {
	struct grub2_word	*word;
	struct grub2_argv	*argv;
	struct grub2_statement	*statement;
	struct grub2_statements	*statements;
}

%printer { fprintf(yyoutput, "%s%s:'%s'",
		$$->type == GRUB2_WORD_VAR ? "var" : "text",
		$$->type == GRUB2_WORD_VAR && !$$->split ? "[nosplit]" : "",
		$$->name); } <word>

/* reserved words */
%token	TOKEN_LDSQBRACKET	"[["
%token	TOKEN_RDSQBRACKET	"]]"
%token	TOKEN_CASE		"case"
%token	TOKEN_DO		"do"
%token	TOKEN_DONE		"done"
%token	TOKEN_ELIF		"elif"
%token	TOKEN_ELSE		"else"
%token	TOKEN_ESAC		"esac"
%token	TOKEN_FI		"fi"
%token	TOKEN_FOR		"for"
%token	TOKEN_FUNCTION		"function"
%token	TOKEN_IF		"if"
%token	TOKEN_IN		"in"
%token	TOKEN_MENUENTRY		"menuentry"
%token	TOKEN_SELECT		"select"
%token	TOKEN_SUBMENU		"submenu"
%token	TOKEN_THEN		"then"
%token	TOKEN_TIME		"time"
%token	TOKEN_UTIL		"until"
%token	TOKEN_WHILE		"while"

%type <statement>	statement
%type <statements>	statements
%type <statement>	conditional
%type <statement>	elif
%type <statements>	elifs
%type <argv>		words
%type <word>		word

/* syntax */
%token	TOKEN_EOL
%token	TOKEN_DELIM
%token	<word> TOKEN_WORD
%token	TOKEN_EOF 0

%start	script
%debug

%{
#include "grub2-lexer.h"
%}

%%

script:	statements {
		parser->script->statements = $1;
	}

statements: statement {
		$$ = create_statements(parser);
		statement_append($$, $1);
	}
	| statements eol statement {
		statement_append($1, $3);
		$$ = $1;
	}

conditional: statement eol "then" statements {
		$$ = create_statement_conditional(parser, $1, $4);
	}

elif: "elif" conditional {
		$$ = $2;
      }

elifs: /* empty */ {
		$$ = create_statements(parser);
	}
	| elifs elif {
		statement_append($1, $2);
		$$ = $1;
	}

statement: {
		   $$ = NULL;
	}
	| words delim0 {
		   $$ = create_statement_simple(parser, $1);
	}
	| '{' statements '}' {
		$$ = create_statement_block(parser, $2);
	}
	| "if" conditional elifs "fi" {
		$$ = create_statement_if(parser, $2, $3, NULL);
	}
	| "if" conditional
		elifs
		"else"
		statements
		"fi" {
		$$ = create_statement_if(parser, $2, $3, $5);
	}
	| "function" word delim '{' statements '}' {
		$$ = create_statement_function(parser, $2, $5);
	}
	| "menuentry" words delim0
		'{' statements '}' {
		$$ = create_statement_menuentry(parser, $2, $5);
	}
	| "submenu" words delim
		'{' statements '}' {
		/* we just flatten everything */
		$$ = create_statement_block(parser, $5);
	}
	| "for" word delim "in" delim words eol
		"do"
		statements
		"done" {
		$$ = create_statement_for(parser, $2, $6, $9);
	}

words:	word {
		$$ = create_argv(parser);
		argv_append($$, $1);
	}
	| words delim word {
		argv_append($1, $3);
		$$ = $1;
	}

word:	TOKEN_WORD
	| word TOKEN_WORD {
		word_append($1, $2);
		$$ = $1;
	}

delim0:	/* empty */ |
	delim

delim:	TOKEN_DELIM |
	delim TOKEN_DELIM

eol:	TOKEN_EOL;
%%
void yyerror(struct grub2_parser *parser, void *scanner, const char *fmt, ...)
{
	const char *str;
	va_list ap;

	va_start(ap, fmt);
	str = talloc_vasprintf(parser, fmt, ap);
	va_end(ap);

	pb_log("parse error: %d('%s'): %s\n", yyget_lineno(scanner),
					yyget_text(scanner), str);
}

struct grub2_statements *create_statements(struct grub2_parser *parser)
{
	struct grub2_statements *stmts = talloc(parser,
			struct grub2_statements);
	list_init(&stmts->list);
	return stmts;
}

struct grub2_statement *create_statement_simple(struct grub2_parser *parser,
		struct grub2_argv *argv)
{
	struct grub2_statement_simple *stmt =
		talloc(parser, struct grub2_statement_simple);
	stmt->st.type = STMT_TYPE_SIMPLE;
	stmt->st.exec = statement_simple_execute;
	stmt->argv = argv;
	return &stmt->st;
}

struct grub2_statement *create_statement_menuentry(struct grub2_parser *parser,
		struct grub2_argv *argv, struct grub2_statements *stmts)
{
	struct grub2_statement_menuentry *stmt =
		talloc(parser, struct grub2_statement_menuentry);
	stmt->st.type = STMT_TYPE_MENUENTRY;
	stmt->st.exec = statement_menuentry_execute;
	stmt->argv = argv;
	stmt->statements = stmts;
	return &stmt->st;
}

struct grub2_statement *create_statement_conditional(
		struct grub2_parser *parser,
		struct grub2_statement *condition,
		struct grub2_statements *statements)
{
	struct grub2_statement_conditional *stmt =
		talloc(parser, struct grub2_statement_conditional);
	stmt->st.type = STMT_TYPE_CONDITIONAL;
	stmt->condition = condition;
	stmt->statements = statements;
	return &stmt->st;
}

struct grub2_statement *create_statement_if(struct grub2_parser *parser,
		struct grub2_statement *conditional,
		struct grub2_statements *elifs,
		struct grub2_statements *else_case)
{
	struct grub2_statement_if *stmt =
		talloc(parser, struct grub2_statement_if);

	list_add(&elifs->list, &conditional->list);

	stmt->st.type = STMT_TYPE_IF;
	stmt->st.exec = statement_if_execute;
	stmt->conditionals = elifs;
	stmt->else_case = else_case;
	return &stmt->st;
}

struct grub2_statement *create_statement_block(struct grub2_parser *parser,
		struct grub2_statements *stmts)
{
	struct grub2_statement_block *stmt =
		talloc(parser, struct grub2_statement_block);
	stmt->st.type = STMT_TYPE_BLOCK;
	stmt->st.exec = statement_block_execute;
	stmt->statements = stmts;
	return &stmt->st;
}

struct grub2_statement *create_statement_function(struct grub2_parser *parser,
		struct grub2_word *name, struct grub2_statements *body)
{
	struct grub2_statement_function *stmt =
		talloc(parser, struct grub2_statement_function);
	stmt->st.exec = statement_function_execute;
	stmt->name = name;
	stmt->body = body;
	return &stmt->st;
}

struct grub2_statement *create_statement_for(struct grub2_parser *parser,
		struct grub2_word *var, struct grub2_argv *list,
		struct grub2_statements *body)
{
	struct grub2_statement_for *stmt =
		talloc(parser, struct grub2_statement_for);
	stmt->st.exec = statement_for_execute;
	stmt->var = var;
	stmt->list = list;
	stmt->body = body;
	return &stmt->st;
}

void statement_append(struct grub2_statements *stmts,
		struct grub2_statement *stmt)
{
	if (stmt)
		list_add_tail(&stmts->list, &stmt->list);
}

struct grub2_word *create_word_text(struct grub2_parser *parser,
		const char *text)
{
	struct grub2_word *word = talloc(parser, struct grub2_word);
	word->type = GRUB2_WORD_TEXT;
	word->split = false;
	word->text = talloc_strdup(word, text);
	word->next = NULL;
	word->last = word;
	return word;
}

struct grub2_word *create_word_var(struct grub2_parser *parser,
		const char *name, bool split)
{
	struct grub2_word *word = talloc(parser, struct grub2_word);
	word->type = GRUB2_WORD_VAR;
	word->name = talloc_strdup(word, name);
	word->split = split;
	word->next = NULL;
	word->last = word;
	return word;
}

struct grub2_argv *create_argv(struct grub2_parser *parser)
{
	struct grub2_argv *argv = talloc(parser, struct grub2_argv);
	list_init(&argv->words);
	return argv;
}

void argv_append(struct grub2_argv *argv, struct grub2_word *word)
{
	list_add_tail(&argv->words, &word->argv_list);
}

void word_append(struct grub2_word *w1, struct grub2_word *w2)
{
	w1->last->next = w2;
	w1->last = w2;
}

struct grub2_parser *grub2_parser_create(struct discover_context *ctx)
{
	struct grub2_parser *parser;

	parser = talloc(ctx, struct grub2_parser);
	yylex_init_extra(parser, &parser->scanner);
	parser->script = create_script(parser, ctx);
	parser->inter_word = false;

	return parser;
}

/* performs a parse on buf, setting parser->script->statements */
int grub2_parser_parse(struct grub2_parser *parser, const char *filename,
		char *buf, int len)
{
	YY_BUFFER_STATE bufstate;
	int rc;

	if (!len)
		return -1;

	parser->script->filename = filename;

	bufstate = yy_scan_bytes(buf, len - 1, parser->scanner);
	yyset_lineno(1, parser->scanner);

	rc = yyparse(parser, parser->scanner);

	yy_delete_buffer(bufstate, parser->scanner);

	parser->inter_word = false;

	return rc;
}

void grub2_parser_parse_and_execute(struct grub2_parser *parser,
		const char *filename, char *buf, int len)
{
	int rc;

	rc = grub2_parser_parse(parser, filename, buf, len);

	if (!rc)
		script_execute(parser->script);
}

