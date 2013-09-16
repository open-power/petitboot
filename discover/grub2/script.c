
#include <sys/types.h>
#include <string.h>

#include <types/types.h>
#include <talloc/talloc.h>

#include "grub2.h"

#define to_stmt_simple(stmt) \
	container_of(stmt, struct grub2_statement_simple, st)
#define to_stmt_if(stmt) \
	container_of(stmt, struct grub2_statement_if, st)
#define to_stmt_menuentry(stmt) \
	container_of(stmt, struct grub2_statement_menuentry, st)

struct env_entry {
	const char		*name;
	const char		*value;
	struct list_item	list;
};

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
		entry->name = name;
		list_add(&script->environment, &entry->list);
	}

	entry->value = value;
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
	struct grub2_command *cmd;
	int rc;

	if (!st->argv)
		return 0;

	process_expansions(script, st->argv);

	if (!st->argv->argc)
		return 0;

	cmd = script_lookup_command(script, st->argv->argv[0]);
	if (!cmd) {
		fprintf(stderr, "undefined command '%s'\n", st->argv->argv[0]);
		return 0;
	}

	rc = cmd->exec(script, st->argv->argc, st->argv->argv);

	return rc;
}

int statement_if_execute(struct grub2_script *script,
		struct grub2_statement *statement)
{
	struct grub2_statement_if *st = to_stmt_if(statement);
	struct grub2_statements *case_stmts;
	int rc;

	rc = st->condition->exec(script, st->condition);

	if (rc == 0)
		case_stmts = st->true_case;
	else
		case_stmts = st->false_case;

	if (case_stmts)
		statements_execute(script, case_stmts);
	else
		rc = 0;

	return rc;
}

int statement_menuentry_execute(struct grub2_script *script,
		struct grub2_statement *statement)
{
	struct grub2_statement_menuentry *st = to_stmt_menuentry(statement);
	struct discover_boot_option *opt;

	process_expansions(script, st->argv);

	opt = discover_boot_option_create(script->ctx, script->ctx->device);
	if (st->argv->argc > 0) {
		opt->option->name = talloc_strdup(opt, st->argv->argv[0]);
	} else {
		opt->option->name = talloc_strdup(opt, "(unknown)");
	}

	script->opt = opt;

	statements_execute(script, st->statements);

	discover_context_add_boot_option(script->ctx, opt);
	script->opt = NULL;

	return 0;
}

static void init_env(struct grub2_script *script)
{
	struct env_entry *env;

	list_init(&script->environment);

	env = talloc(script, struct env_entry);
	env->name = talloc_strdup(env, "prefix");
	env->value = talloc_strdup(env, "/");

	list_add(&script->environment, &env->list);
}

struct grub2_command *script_lookup_command(struct grub2_script *script,
		const char *name)
{
	struct grub2_command *command;

	list_for_each_entry(&script->commands, command, list) {
		if (!strcmp(command->name, name))
			return command;
	}

	return NULL;
}

void script_register_command(struct grub2_script *script,
		struct grub2_command *command)
{
	list_add(&script->commands, &command->list);
}


void script_execute(struct grub2_script *script)
{
	statements_execute(script, script->statements);
}

struct grub2_script *create_script(struct grub2_parser *parser,
		struct discover_context *ctx)
{
	struct grub2_script *script;

	script = talloc(parser, struct grub2_script);

	init_env(script);
	script->ctx = ctx;
	script->opt = NULL;

	list_init(&script->commands);
	register_builtins(script);

	return script;
}

