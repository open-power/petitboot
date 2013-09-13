
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
	stmt->st.exec = NULL;
	stmt->argv = argv;
	stmt->statements = stmts;
	return &stmt->st;
}

struct grub2_statement *create_statement_if(struct grub2_parser *parser,
		struct grub2_statement *condition,
		struct grub2_statements *true_case,
		struct grub2_statements *false_case)
{
	struct grub2_statement_if *stmt =
		talloc(parser, struct grub2_statement_if);
	stmt->st.type = STMT_TYPE_IF;
	stmt->st.exec = statement_if_execute;
	stmt->condition = condition;
	stmt->true_case = true_case;
	stmt->false_case = false_case;
	return &stmt->st;
}

struct grub2_statement *create_statement_block(struct grub2_parser *parser,
		struct grub2_statements *stmts)
{
	struct grub2_statement_block *stmt =
		talloc(parser, struct grub2_statement_block);
	stmt->st.type = STMT_TYPE_BLOCK;
	stmt->st.exec = NULL;
	stmt->statements = stmts;
	return &stmt->st;
}

void statement_append(struct grub2_statements *stmts,
		struct grub2_statement *stmt)
{
	if (!stmt)
		return;
	list_add_tail(&stmts->list, &stmt->list);
}

struct grub2_word *create_word_text(struct grub2_parser *parser,
		const char *text)
{
	struct grub2_word *word = talloc(parser, struct grub2_word);
	word->type = GRUB2_WORD_TEXT;
	word->text = talloc_strdup(word, text);
	word->next = NULL;
	return word;
}

struct grub2_word *create_word_var(struct grub2_parser *parser,
		const char *name, bool split)
{
	struct grub2_word *word = talloc(parser, struct grub2_word);
	word->type = GRUB2_WORD_VAR;
	word->var.name = talloc_strdup(word, name);
	word->var.split = split;
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
