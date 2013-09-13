
#include <sys/types.h>
#include <regex.h>
#include <string.h>

#include <talloc/talloc.h>

#include "grub2.h"

#define to_stmt_simple(stmt) \
	container_of(stmt, struct grub2_statement_simple, st)
#define to_stmt_if(stmt) \
	container_of(stmt, struct grub2_statement_if, st)

struct env_entry {
	const char		*name;
	const char		*value;
	struct list_item	list;
};

static const char *env_lookup(struct grub2_script *script,
		const char *name, int name_len)
{
	struct env_entry *entry;

	printf("%s: %.*s\n", __func__, name_len, name);

	list_for_each_entry(&script->environment, entry, list)
		if (!strncmp(entry->name, name, name_len)
				&& entry->name[name_len] == '\0')
			return entry->value;

	return NULL;
}

static bool expand_word(struct grub2_script *script, struct grub2_word *word)
{
	const char *val, *src;
	char *dest = NULL;
	regmatch_t match;
	int n, i;

	src = word->text;

	n = regexec(&script->var_re, src, 1, &match, 0);
	printf("%s %s: %d\n", __func__, word->text, n);
	if (n != 0)
		return false;

	i = 0;
	if (src[match.rm_so + 1] == '{')
		i++;

	val = env_lookup(script, src + match.rm_so + 1 + i,
				 match.rm_eo - match.rm_so - 1 - (i * 2));
	if (!val)
		val = "";

	printf("repl: %s\n", val);

	dest = talloc_strndup(script, src, match.rm_so);
	dest = talloc_asprintf_append(dest, "%s%s", val, src + match.rm_eo);

	word->text = dest;
	return true;
}

/* iterate through the words in an argv, looking for expansions. If a
 * word is marked with expand == true, then we process any variable
 * substitutions.
 *
 * Once that's done, we may (if split == true) have to split the word to create
 * new argv items
 */
static void process_expansions(struct grub2_script *script,
		struct grub2_argv *argv)
{
	struct grub2_word *word;

	list_for_each_entry(&argv->words, word, argv_list) {
		if (!word->expand)
			continue;

		expand_word(script, word);
	}
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

	if (!st->argv)
		return 0;

	process_expansions(script, st->argv);

	return 0;
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

static void init_env(struct grub2_script *script)
{
	struct env_entry *env;

	list_init(&script->environment);

	env = talloc(script, struct env_entry);
	env->name = talloc_strdup(env, "prefix");
	env->value = talloc_strdup(env, "/");

	list_add(&script->environment, &env->list);
}


void script_execute(struct grub2_script *script)
{
	statements_execute(script, script->statements);
}

static int script_destroy(void *p)
{
	struct grub2_script *script = p;
	regfree(&script->var_re);
	return 0;
}

struct grub2_script *create_script(void *ctx)
{
	struct grub2_script *script;
	int rc;

	script = talloc(ctx, struct grub2_script);

	rc = regcomp(&script->var_re,
		"\\$\\{?([[:alpha:]][_[:alnum:]]*|[0-9]|[\\?@\\*#])\\}?",
			REG_EXTENDED);
	if (rc) {
		char err[200];
		regerror(rc, &script->var_re, err, sizeof(err));
		fprintf(stderr, "RE error %d: %s\n", rc, err);
		talloc_free(script);
		return NULL;

	}
	talloc_set_destructor(script, script_destroy);

	init_env(script);

	return script;
}

