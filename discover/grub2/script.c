
#include <sys/types.h>
#include <string.h>

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

	val = script_env_get(script, word->var.name);
	if (!val)
		val = "";

	word->type = GRUB2_WORD_TEXT;
	word->text = talloc_strdup(script, val);

	return true;
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

		/* expand vars and squash the list of words into the top struct.
		 * todo: splitting
		 */
		for (word = top_word; word; word = word->next) {
			if (word->type == GRUB2_WORD_VAR)
				expand_var(script, word);

			if (word == top_word)
				continue;

			top_word->text = talloc_asprintf_append(top_word->text,
					"%s", word->text);
		}
		top_word->next = NULL;
	}

	argv->argv = talloc_array(script, char *, argv->argc);
	i = 0;

	list_for_each_entry(&argv->words, word, argv_list)
		argv->argv[i++] = word->text;
}

int statements_execute(struct grub2_script *script,
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

	process_expansions(script, st->argv);
	statements_execute(script, st->statements);

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

struct grub2_script *create_script(void *ctx)
{
	struct grub2_script *script;

	script = talloc(ctx, struct grub2_script);

	init_env(script);
	list_init(&script->commands);
	register_builtins(script);

	return script;
}

