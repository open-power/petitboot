#ifndef GRUB2_H
#define GRUB2_H

#include <discover/device-handler.h>

#include <stdbool.h>
#include <list/list.h>

struct grub2_script;

struct grub2_word {
	enum {
		GRUB2_WORD_TEXT,
		GRUB2_WORD_VAR,
	} type;
	union {
		char		*text;
		const char	*name;
	};
	bool			split;
	struct grub2_word	*next;
	struct grub2_word	*last;
	struct list_item	argv_list;
};

struct grub2_argv {
	struct list		words;

	/* postprocessing (with process_expansions) populates these to hand to
	 * the grub2_command callbacks */
	char			**argv;
	int			argc;
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
		STMT_TYPE_CONDITIONAL,
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

struct grub2_statement_conditional {
	struct grub2_statement	st;
	struct grub2_statement	*condition;
	struct grub2_statements	*statements;
};

struct grub2_statement_if {
	struct grub2_statement	st;
	struct grub2_statements	*conditionals;
	struct grub2_statements	*else_case;
};

struct grub2_statement_block {
	struct grub2_statement	st;
	struct grub2_statements	*statements;
};

struct grub2_statement_function {
	struct grub2_statement	st;
	struct grub2_word	*name;
	struct grub2_statements	*body;
};

struct grub2_statement_for {
	struct grub2_statement	st;
	struct grub2_word	*var;
	struct grub2_argv	*list;
	struct grub2_statements	*body;
};

struct grub2_script {
	struct grub2_parser		*parser;
	struct grub2_statements		*statements;
	struct list			environment;
	struct list			symtab;
	struct discover_context		*ctx;
	struct discover_boot_option	*opt;
	const char			*filename;
	unsigned int			n_options;
	struct list			options;
};

struct grub2_parser {
	void			*scanner;
	struct grub2_script	*script;
	bool			inter_word;
};

/* References to files in grub2 consist of an optional device and a path
 * (specified here by UUID). If the dev is unspecified, we fall back to a
 * default - usually the 'root' environment variable. */
struct grub2_file {
	char *dev;
	char *path;
};

/* type for builtin functions */
typedef int (*grub2_function)(struct grub2_script *script, void *data,
				int argc, char *argv[]);

struct grub2_statements *create_statements(struct grub2_parser *parser);

struct grub2_statement *create_statement_simple(struct grub2_parser *parser,
		struct grub2_argv *argv);

struct grub2_statement *create_statement_menuentry(struct grub2_parser *parser,
		struct grub2_argv *argv, struct grub2_statements *stmts);

struct grub2_statement *create_statement_conditional(
		struct grub2_parser *parser, struct grub2_statement *condition,
		struct grub2_statements *statements);

struct grub2_statement *create_statement_if(struct grub2_parser *parser,
		struct grub2_statement *conditional,
		struct grub2_statements *elifs,
		struct grub2_statements *else_case);

struct grub2_statement *create_statement_block(struct grub2_parser *parser,
		struct grub2_statements *stmts);

struct grub2_statement *create_statement_function(struct grub2_parser *parser,
		struct grub2_word *name, struct grub2_statements *body);

struct grub2_statement *create_statement_for(struct grub2_parser *parser,
		struct grub2_word *var, struct grub2_argv *list,
		struct grub2_statements *body);

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

int statements_execute(struct grub2_script *script,
		struct grub2_statements *stmts);

int statement_simple_execute(struct grub2_script *script,
		struct grub2_statement *statement);
int statement_block_execute(struct grub2_script *script,
		struct grub2_statement *statement);
int statement_if_execute(struct grub2_script *script,
		struct grub2_statement *statement);
int statement_menuentry_execute(struct grub2_script *script,
		struct grub2_statement *statement);
int statement_function_execute(struct grub2_script *script,
		struct grub2_statement *statement);
int statement_for_execute(struct grub2_script *script,
		struct grub2_statement *statement);

struct grub2_script *create_script(struct grub2_parser *parser,
		struct discover_context *ctx);

const char *script_env_get(struct grub2_script *script, const char *name);

void script_env_set(struct grub2_script *script,
		const char *name, const char *value);

void script_register_function(struct grub2_script *script,
		const char *name, grub2_function fn, void *data);

void register_builtins(struct grub2_script *script);

/* resources */
struct resource *create_grub2_resource(struct grub2_script *script,
		struct discover_boot_option *opt, const char *path);

bool resolve_grub2_resource(struct device_handler *handler,
		struct resource *res);

/* grub-style device+path parsing */
struct grub2_file *grub2_parse_file(struct grub2_script *script,
		const char *str);
struct discover_device *grub2_lookup_device(struct device_handler *handler,
		const char *desc);

/* internal parse api */
int grub2_parser_parse(struct grub2_parser *parser, const char *filename,
		char *buf, int len);

/* external parser api */
struct grub2_parser *grub2_parser_create(struct discover_context *ctx);
void grub2_parser_parse_and_execute(struct grub2_parser *parser,
		const char *filename, char *buf, int len);
#endif /* GRUB2_H */

