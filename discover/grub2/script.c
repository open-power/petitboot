
#include <sys/types.h>
#include <string.h>
#include <stdlib.h>

#include <log/log.h>
#include <types/types.h>
#include <talloc/talloc.h>

#include "grub2.h"

#define to_stmt_simple(stmt) \
	container_of(stmt, struct grub2_statement_simple, st)
#define to_stmt_block(stmt) \
	container_of(stmt, struct grub2_statement_block, st)
#define to_stmt_if(stmt) \
	container_of(stmt, struct grub2_statement_if, st)
#define to_stmt_menuentry(stmt) \
	container_of(stmt, struct grub2_statement_menuentry, st)
#define to_stmt_function(stmt) \
	container_of(stmt, struct grub2_statement_function, st)
#define to_stmt_conditional(stmt) \
	container_of(stmt, struct grub2_statement_conditional, st)

struct env_entry {
	char			*name;
	char			*value;
	struct list_item	list;
};

struct grub2_symtab_entry {
	const char		*name;
	grub2_function		fn;
	void			*data;
	struct list_item	list;
};

static const char *default_prefix = "/boot/grub";

static struct grub2_symtab_entry *script_lookup_function(
		struct grub2_script *script, const char *name)
{
	struct grub2_symtab_entry *entry;

	list_for_each_entry(&script->symtab, entry, list) {
		if (!strcmp(entry->name, name))
			return entry;
	}

	return NULL;
}

const char *script_env_get(struct grub2_script *script, const char *name)
{
	struct env_entry *entry;

	list_for_each_entry(&script->environment, entry, list)
		if (!strcmp(entry->name, name))
			return entry->value;

	return NULL;
}

void script_env_set(struct grub2_script *script,
		const char *name, const char *value)
{
	struct env_entry *e, *entry = NULL;

	list_for_each_entry(&script->environment, e, list) {
		if (!strcmp(e->name, name)) {
			entry = e;
			break;
		}
	}

	if (!entry) {
		entry = talloc(script, struct env_entry);
		entry->name = talloc_strdup(entry, name);
		list_add(&script->environment, &entry->list);
	} else {
		talloc_free(entry->value);
	}

	entry->value = talloc_strdup(entry, value);
}

static bool expand_var(struct grub2_script *script, struct grub2_word *word)
{
	const char *val;

	val = script_env_get(script, word->name);
	if (!val)
		val = "";

	word->text = talloc_strdup(script, val);

	return true;
}

static bool is_delim(char c)
{
	return c == ' ' || c == '\t';
}

static bool option_is_default(struct grub2_script *script,
		struct discover_boot_option *opt, const char *id)
{
	unsigned int default_idx;
	const char *var;
	char *end;

	var = script_env_get(script, "default");
	if (!var)
		return false;

	default_idx = strtoul(var, &end, 10);
	if (end != var && *end == '\0')
		return default_idx == script->n_options;

	/* if we don't have an explicit id for this option, fall back to
	 * the name */
	if (!id)
		id = opt->option->name;

	return !strcmp(id, var);
}

/* For non-double-quoted variable expansions, we may need to split the
 * variable's value into multiple argv items.
 *
 * This function sets the word->text to the initial set of non-delimiter chars
 * in the expanded var value. We then skip any delimiter chars, and (if
 * required), create the new argv item with the remaining text, and
 * add it to the argv list, after top_word.
 */
static void process_split(struct grub2_script *script,
		struct grub2_word *top_word,
		struct grub2_word *word)
{
	int i, len, delim_start = -1, delim_end = -1;
	struct grub2_word *new_word;
	char *remainder;

	len = strlen(word->text);

	/* Scan our string for the start of a delim (delim_start), and the
	 * start of any new text (delim_end). */
	for (i = 0; i < len; i++) {
		if (is_delim(word->text[i])) {
			if (delim_start == -1)
				delim_start = i;
		} else if (delim_start != -1) {
			delim_end = i;
			break;
		}
	}

	/* No delim? nothing to do. The process_expansions loop will
	 * append this word's text to the top word, if necessary
	 */
	if (delim_start == -1)
		return;

	/* Set this word's text value to the text before the delimiter.
	 * this will get appended to the top word
	 */
	word->text[delim_start] = '\0';

	/* No trailing text? If there are no following word tokens, we're done.
	 * Otherwise, we need to start a new argv item with those tokens */
	if (delim_end == -1) {
		if (!word->next)
			return;
		remainder = "";
	} else {
		remainder = word->text + delim_end;
	}

	new_word = talloc(script, struct grub2_word);
	new_word->type = GRUB2_WORD_TEXT;
	/* if there's no trailing text, we know we don't need to re-split */
	new_word->split = delim_end != -1;
	new_word->next = word->next;
	new_word->last = NULL;
	new_word->text = talloc_strdup(new_word, remainder);

	/* stitch it into the argv list before this word */
	list_insert_after(&top_word->argv_list,
			   &new_word->argv_list);

	/* terminate this word */
	word->next = NULL;
}

/* iterate through the words in an argv_list, looking for GRUB2_WORD_VAR
 * expansions.
 *
 * Once that's done, we may (if split == true) have to split the word to create
 * new argv items
 */
static void process_expansions(struct grub2_script *script,
		struct grub2_argv *argv)
{
	struct grub2_word *top_word, *word;
	int i;

	argv->argc = 0;

	list_for_each_entry(&argv->words, top_word, argv_list) {
		argv->argc++;

		/* expand vars and squash the list of words into the head
		 * of the argv word list */
		for (word = top_word; word; word = word->next) {

			/* if it's a variable, perform the substitution */
			if (word->type == GRUB2_WORD_VAR) {
				expand_var(script, word);
				word->type = GRUB2_WORD_TEXT;
			}

			/* split; this will potentially insert argv
			 * entries after top_word. */
			if (word->split)
				process_split(script, top_word, word);

			/* accumulate word text into the top word, so
			 * we end up with a shallow tree of argv data */
			/* todo: don't do this in process_split */
			if (word != top_word) {
				top_word->text = talloc_asprintf_append(
							top_word->text,
							"%s", word->text);
			}


		}
		top_word->next = NULL;
	}

	/* convert the list to an argv array, to pass to the function */
	argv->argv = talloc_array(script, char *, argv->argc);
	i = 0;

	list_for_each_entry(&argv->words, word, argv_list)
		argv->argv[i++] = word->text;
}

static int statements_execute(struct grub2_script *script,
		struct grub2_statements *stmts)
{
	struct grub2_statement *stmt;
	int rc = 0;

	list_for_each_entry(&stmts->list, stmt, list) {
		if (stmt->exec)
			rc = stmt->exec(script, stmt);
	}
	return rc;
}

int statement_simple_execute(struct grub2_script *script,
		struct grub2_statement *statement)
{
	struct grub2_statement_simple *st = to_stmt_simple(statement);
	struct grub2_symtab_entry *entry;
	char *pos;
	int rc;

	if (!st->argv)
		return 0;

	process_expansions(script, st->argv);

	if (!st->argv->argc)
		return 0;

	/* is this a var=value assignment? */
	pos = strchr(st->argv->argv[0], '=');
	if (pos) {
		char *name, *value;
		name = st->argv->argv[0];
		name = talloc_strndup(st, name, pos - name);
		value = pos + 1;
		script_env_set(script, name, value);
		return 0;
	}

	entry = script_lookup_function(script, st->argv->argv[0]);
	if (!entry) {
		pb_log("grub2: undefined function '%s'\n", st->argv->argv[0]);
		return 1;
	}

	rc = entry->fn(script, entry->data, st->argv->argc, st->argv->argv);

	return rc;
}

int statement_block_execute(struct grub2_script *script,
		struct grub2_statement *statement)
{
	struct grub2_statement_block *st = to_stmt_block(statement);
	return statements_execute(script, st->statements);
}

/* returns 0 if the statement was executed, 1 otherwise */
static int statement_conditional_execute(struct grub2_script *script,
		struct grub2_statement *statement, bool *executed)
{
	struct grub2_statement_conditional *st = to_stmt_conditional(statement);
	int rc;

	rc = st->condition->exec(script, st->condition);
	*executed = (!rc);
	if (*executed)
		rc = statements_execute(script, st->statements);

	return rc;
}

int statement_if_execute(struct grub2_script *script,
		struct grub2_statement *statement)
{
	struct grub2_statement_if *st = to_stmt_if(statement);
	struct grub2_statement *conditional;
	bool executed;
	int rc = 0;

	list_for_each_entry(&st->conditionals->list, conditional, list) {
		rc = statement_conditional_execute(script,
				conditional, &executed);
		if (executed)
			break;
	}

	if (!executed && st->else_case)
		rc = statements_execute(script, st->else_case);

	return rc;
}

int statement_menuentry_execute(struct grub2_script *script,
		struct grub2_statement *statement)
{
	struct grub2_statement_menuentry *st = to_stmt_menuentry(statement);
	struct discover_boot_option *opt;
	const char *id = NULL;
	int i;

	process_expansions(script, st->argv);

	opt = discover_boot_option_create(script->ctx, script->ctx->device);

	/* XXX: --options=values need to be parsed properly; this is a simple
	 * implementation to get --id= working.
	 */
	for (i = 1; i < st->argv->argc; ++i) {
		if (strncmp("--id=", st->argv->argv[i], 5) == 0) {
			id = st->argv->argv[i] + 5;
			break;
		}
	}
	if (st->argv->argc > 0)
		opt->option->name = talloc_strdup(opt, st->argv->argv[0]);
	else
		opt->option->name = talloc_strdup(opt, "(unknown)");

	opt->option->id = talloc_asprintf(opt->option, "%s#%s",
			script->ctx->device->device->id,
			id ? id : opt->option->name);

	script->opt = opt;

	statements_execute(script, st->statements);

	opt->option->is_default = option_is_default(script, opt, id);

	discover_context_add_boot_option(script->ctx, opt);
	script->n_options++;
	script->opt = NULL;

	return 0;
}

static int function_invoke(struct grub2_script *script,
		void *data, int argc, char **argv)
{
	struct grub2_statement_function *fn = data;
	char *name;
	int i;

	/* set positional parameters */
	for (i = 0; i < argc; i++) {
		name = talloc_asprintf(script, "$%d", i);
		script_env_set(script, name, argv[i]);
	}

	return statements_execute(script, fn->body);
}

int statement_function_execute(struct grub2_script *script,
		struct grub2_statement *statement)
{
	struct grub2_statement_function *st = to_stmt_function(statement);
	const char *name;

	if (st->name->type == GRUB2_WORD_VAR)
		expand_var(script, st->name);

	name = st->name->text;
	script_register_function(script, name, function_invoke, st);

	return 0;
}

static void init_env(struct grub2_script *script)
{
	struct env_entry *env;
	char *prefix, *sep;

	list_init(&script->environment);

	/* use location of the parsed config file to determine the prefix */
	env = talloc(script, struct env_entry);

	prefix = NULL;
	if (script->filename) {
		sep = strrchr(script->filename, '/');
		if (sep)
			prefix = talloc_strndup(env, script->filename,
					sep - script->filename);
	}

	script_env_set(script, "prefix", prefix ? : default_prefix);
	if (prefix)
		talloc_free(prefix);
}

void script_register_function(struct grub2_script *script,
		const char *name, grub2_function fn,
		void *data)
{
	struct grub2_symtab_entry *entry;

	entry = talloc(script, struct grub2_symtab_entry);
	entry->fn = fn;
	entry->name = name;
	entry->data = data;
	list_add(&script->symtab, &entry->list);
}


void script_execute(struct grub2_script *script)
{
	init_env(script);
	statements_execute(script, script->statements);
}

struct grub2_script *create_script(struct grub2_parser *parser,
		struct discover_context *ctx)
{
	struct grub2_script *script;

	script = talloc_zero(parser, struct grub2_script);

	script->ctx = ctx;

	list_init(&script->symtab);
	register_builtins(script);

	return script;
}

