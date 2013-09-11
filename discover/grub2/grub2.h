#ifndef GRUB2_H
#define GRUB2_H

#include <stdbool.h>
#include <list/list.h>

struct grub2_parser;

struct grub2_word {
	const char		*text;
	bool			expand;
	bool			split;
	struct grub2_word	*next;
	struct list_item	argv_list;
};

struct grub2_argv {
	struct list		words;
};

struct grub2_statement_simple {
	struct grub2_argv	*argv;
};

struct grub2_statement_menuentry {
	struct grub2_argv	*argv;
	struct grub2_statements	*statements;
};

struct grub2_statement_if {
	struct grub2_statement	*condition;
	struct grub2_statements	*true_case;
	struct grub2_statements	*false_case;
};

struct grub2_statement {
	struct list_item	list;
	enum {
		STMT_TYPE_SIMPLE,
		STMT_TYPE_MENUENTRY,
		STMT_TYPE_IF,
	} type;
	union {
		struct grub2_statement_simple		simple;
		struct grub2_statement_menuentry	menuentry;
		struct grub2_statement_if		ifstmt;
	};
};

struct grub2_statements {
	struct list		list;
};

struct grub2_parser {
	void			*scanner;
	struct grub2_statements	*statements;
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

struct grub2_word *create_word(struct grub2_parser *parser, const char *text,
		bool expand, bool split);

struct grub2_argv *create_argv(struct grub2_parser *parser);

void statement_append(struct grub2_statements *stmts,
		struct grub2_statement *stmt);

void argv_append(struct grub2_argv *argv, struct grub2_word *word);

void word_append(struct grub2_word *w1, struct grub2_word *w2);

#endif /* GRUB2_H */

