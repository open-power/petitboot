
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
#define to_stmt_for(stmt) \
	container_of(stmt, struct grub2_statement_for, st)
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

static char *expand_var(struct grub2_script *script, struct grub2_word *word)
{
	const char *val;

	val = script_env_get(script, word->name);
	if (!val)
		val = "";

	return talloc_strdup(script, val);
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

	if (id && !strcmp(id, var))
		return true;

	return !strcmp(opt->option->name, var);
}

static void append_text_to_current_arg(struct grub2_argv *argv,
		const char *text, int len)
{
	char *cur = argv->argv[argv->argc - 1];

	if (cur) {
		int curlen = strlen(cur);
		cur = talloc_realloc(argv->argv, cur, char, len + curlen + 1);
		memcpy(cur + curlen, text, len);
		cur[len + curlen] = '\0';

	} else {
		cur = talloc_strndup(argv->argv, text, len);
	}

	argv->argv[argv->argc-1] = cur;
}

/* Add a word to the argv array. Depending on the word type, and presence of
 * delimiter characters, we may add multiple items to the array.
 */
static void append_word_to_argv(struct grub2_script *script,
		struct grub2_argv *argv, struct grub2_word *word)
{
	const char *text, *pos;
	int i, len;

	/* If it's a variable, perform substitution */
	if (word->type == GRUB2_WORD_VAR)
		text = expand_var(script, word);
	else
		text = word->text;

	len = strlen(text);

	/* If we have no text, we leave the current word as-is. The caller
	 * has allocated an empty string for the case where this is the
	 * first text token */
	if (!len)
		return;

	/* If we're not splitting, we just add the entire block to the
	 * current argv item */
	if (!word->split) {
		append_text_to_current_arg(argv, text, len);
		return;
	}

	/* Scan for delimiters. If we find a word-end boundary, add the word
	 * to the argv array, and start a new argv item */
	pos = !is_delim(text[0]) ? text : NULL;
	for (i = 0; i < len; i++) {

		/* first delimiter after a word: add the accumulated word to
		 * argv */
		if (pos && is_delim(text[i])) {
			append_text_to_current_arg(argv, pos,
					text + i - pos);
			pos = NULL;

		/* first non-delimeter after a delimiter: record the starting
		 * position, and create another argv item */
		} else if (!pos && !is_delim(text[i])) {
			pos = text + i;
			argv->argc++;
			argv->argv = talloc_realloc(argv, argv->argv, char *,
					argv->argc);
			argv->argv[argv->argc - 1] = NULL;
		}
	}

	/* add remaining word characters */
	if (pos)
		append_text_to_current_arg(argv, pos, text + len - pos);
}

/* Transform an argv word-token list (returned from the parser) into an
 * expanded argv array (as used by the script execution code). We do this by
 * iterating through the words in an argv_list, looking for GRUB2_WORD_VAR
 * expansions.
 */
static void process_expansions(struct grub2_script *script,
		struct grub2_argv *argv)
{
	struct grub2_word *top_word, *word;

	argv->argc = 0;
	argv->argv = NULL;

	list_for_each_entry(&argv->words, top_word, argv_list) {
		argv->argc++;
		argv->argv = talloc_realloc(argv, argv->argv, char *,
				argv->argc);
		/* because we've parsed a separate word here, we know that
		 * we need at least an empty string */
		argv->argv[argv->argc - 1] = talloc_strdup(argv->argv, "");

		for (word = top_word; word; word = word->next)
			append_word_to_argv(script, argv, word);
	}

	/* we may have allocated an extra argv element but not populated it */
	if (argv->argv && !argv->argv[argv->argc - 1])
		argv->argc--;
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
	bool executed = false;
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
		if (strncmp("--id", st->argv->argv[i], strlen("--id")) == 0) {
			if (strlen(st->argv->argv[i]) > strlen("--id=")) {
				id = st->argv->argv[i] + strlen("--id=");
				break;
			}

			if (i + 1 < st->argv->argc) {
				id = st->argv->argv[i + 1];
				break;
			}
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

	if (!opt->boot_image)
		return -1;

	opt->option->is_default = option_is_default(script, opt, id);

	list_add_tail(&script->options, &opt->list);
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
	for (i = 1; i < argc; i++) {
		name = talloc_asprintf(script, "%d", i);
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
		name = expand_var(script, st->name);
	else
		name = st->name->text;

	script_register_function(script, name, function_invoke, st);

	return 0;
}

int statement_for_execute(struct grub2_script *script,
		struct grub2_statement *statement)
{
	struct grub2_statement_for *st = to_stmt_for(statement);
	const char *varname;
	int i, rc = 0;

	if (st->var->type == GRUB2_WORD_VAR)
		expand_var(script, st->var);
	varname = st->var->text;

	process_expansions(script, st->list);

	for (i = 0; i < st->list->argc; ++i) {
		script_env_set(script, varname, st->list->argv[i]);
		rc = statements_execute(script, st->body);
	}

	return rc;
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

	/* establish feature settings */
	script_env_set(script, "feature_menuentry_id", "y");
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

static void set_fallback_default(struct grub2_script *script)
{
	struct discover_boot_option *opt, *first = NULL;
	bool have_default = false;

	list_for_each_entry(&script->options, opt, list) {
		if (!first)
			first = opt;
		have_default = have_default || opt->option->is_default;
	}

	if (!have_default && first) {
		const char *env = script_env_get(script, "default");

		pb_log("grub: no explicit default (env default=%s), "
				"falling back to first option (%s)\n",
				env ?: "unset", first->option->name);

		first->option->is_default = true;
	}
}

void script_execute(struct grub2_script *script)
{
	struct discover_boot_option *opt, *tmp;

	if (!script)
		return;

	init_env(script);
	statements_execute(script, script->statements);

	set_fallback_default(script);

	list_for_each_entry_safe(&script->options, opt, tmp, list)
		discover_context_add_boot_option(script->ctx, opt);

	/* Our option list will be invalid, as we've added all options to the
	 * discover context */
	list_init(&script->options);
}

struct grub2_script *create_script(struct grub2_parser *parser,
		struct discover_context *ctx)
{
	struct grub2_script *script;

	script = talloc_zero(parser, struct grub2_script);

	script->ctx = ctx;

	list_init(&script->symtab);
	list_init(&script->options);
	register_builtins(script);

	return script;
}

