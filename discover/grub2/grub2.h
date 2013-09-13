#ifndef GRUB2_H
#define GRUB2_H

#include <stdbool.h>
#include <list/list.h>

struct grub2_script;

struct grub2_word {
	enum {
		GRUB2_WORD_TEXT,
		GRUB2_WORD_VAR,
	} type;
	union {
		char	*text;
		struct {
			const char	*name;
			bool		split;
		} var;
	};
	struct grub2_word	*next;
	struct grub2_word	*last;
	struct list_item	argv_list;
};

struct grub2_argv {
	struct list		words;
};

struct grub2_statements {
	struct list		list;
};

struct grub2_statement {
	struct list_item	list;
	enum {
		STMT_TYPE_SIMPLE,
		STMT_TYPE_MENUENTRY,
		STMT_TYPE_IF,
		STMT_TYPE_BLOCK,
	} type;
	int			(*exec)(struct grub2_script *,
					struct grub2_statement *);
};

struct grub2_statement_simple {
	struct grub2_statement	st;
	struct grub2_argv	*argv;
};

struct grub2_statement_menuentry {
	struct grub2_statement	st;
	struct grub2_argv	*argv;
	struct grub2_statements	*statements;
};

struct grub2_statement_if {
	struct grub2_statement	st;
	struct grub2_statement	*condition;
	struct grub2_statements	*true_case;
	struct grub2_statements	*false_case;
};

struct grub2_statement_block {
	struct grub2_statement	st;
	struct grub2_statements	*statements;
};

struct grub2_script {
	struct grub2_statements	*statements;
	struct list		environment;
};

struct grub2_parser {
	void			*scanner;
	struct grub2_script	*script;
};

struct grub2_statements *create_statements(struct grub2_parser *parser);

struct grub2_statement *create_statement_simple(struct grub2_parser *parser,
		struct grub2_argv *argv);

struct grub2_statement *create_statement_menuentry(struct grub2_parser *parser,
		struct grub2_argv *argv, struct grub2_statements *stmts);

struct grub2_statement *create_statement_if(struct grub2_parser *parser,
		struct grub2_statement *condition,
		struct grub2_statements *true_case,
		struct grub2_statements *false_case);

struct grub2_statement *create_statement_block(struct grub2_parser *parser,
		struct grub2_statements *stmts);

struct grub2_word *create_word_text(struct grub2_parser *parser,
		const char *text);

struct grub2_word *create_word_var(struct grub2_parser *parser,
		const char *name, bool split);

struct grub2_argv *create_argv(struct grub2_parser *parser);

void statement_append(struct grub2_statements *stmts,
		struct grub2_statement *stmt);

void argv_append(struct grub2_argv *argv, struct grub2_word *word);

void word_append(struct grub2_word *w1, struct grub2_word *w2);

/* script interface */
void script_execute(struct grub2_script *script);

int statement_simple_execute(struct grub2_script *script,
		struct grub2_statement *statement);
int statement_if_execute(struct grub2_script *script,
		struct grub2_statement *statement);
int statement_menuentry_execute(struct grub2_script *script,
		struct grub2_statement *statement);

struct grub2_script *create_script(void *ctx);

#endif /* GRUB2_H */

