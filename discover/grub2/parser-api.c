
#include <talloc/talloc.h>

#include "grub2.h"

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
	struct grub2_statement *stmt = talloc(parser, struct grub2_statement);
	stmt->type = STMT_TYPE_SIMPLE;
	stmt->simple.argv = argv;
	return stmt;
}

struct grub2_statement *create_statement_menuentry(struct grub2_parser *parser,
		struct grub2_argv *argv, struct grub2_statements *stmts)
{
	struct grub2_statement *stmt = talloc(parser, struct grub2_statement);
	stmt->type = STMT_TYPE_MENUENTRY;
	stmt->menuentry.argv = argv;
	stmt->menuentry.statements = stmts;
	return stmt;
}

struct grub2_statement *create_statement_if(struct grub2_parser *parser,
		struct grub2_statement *condition,
		struct grub2_statements *true_case,
		struct grub2_statements *false_case)
{
	struct grub2_statement *stmt = talloc(parser, struct grub2_statement);
	stmt->type = STMT_TYPE_IF;
	stmt->ifstmt.condition = condition;
	stmt->ifstmt.true_case = true_case;
	stmt->ifstmt.false_case = false_case;
	return stmt;
}
void statement_append(struct grub2_statements *stmts,
		struct grub2_statement *stmt)
{
	if (!stmt)
		return;
	list_add_tail(&stmts->list, &stmt->list);
}

struct grub2_word *create_word(struct grub2_parser *parser, const char *text,
		bool expand, bool split)
{
	struct grub2_word *word = talloc(parser, struct grub2_word);
	word->text = talloc_strdup(word, text);
	word->expand = expand;
	word->split = split;
	word->next = NULL;
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
	w1->next = w2;
}
