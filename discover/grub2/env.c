
#include <stdio.h>
#include <string.h>

#include <log/log.h>
#include <types/types.h>
#include <talloc/talloc.h>
#include <array-size/array-size.h>

#include <discover/parser.h>
#include <discover/file.h>

#include "grub2.h"

static const char *default_envfile = "grubenv";
static const char *signature = "# GRUB Environment Block\n";

static int parse_buf_to_env(struct grub2_script *script, void *buf, int len)
{
	char *tmp, *line, *sep;
	int siglen;

	siglen = strlen(signature);

	if (len < siglen) {
		pb_log("grub environment block too small\n");
		return -1;
	}

	if (memcmp(buf, signature, siglen)) {
		pb_log("grub environment block has invalid signature\n");
		return -1;
	}

	buf += siglen;

	for (line = strtok_r(buf, "\n", &tmp); line;
				line = strtok_r(NULL, "\n", &tmp)) {

		if (*line == '#')
			continue;

		sep = strchr(line, '=');
		if (!sep)
			continue;
		if (sep == line)
			continue;

		*sep = '\0';
		script_env_set(script, line, sep + 1);
	}

	return 0;
}

int builtin_load_env(struct grub2_script *script,
		void *data __attribute__((unused)),
		int argc, char *argv[]);

int builtin_load_env(struct grub2_script *script,
		void *data __attribute__((unused)),
		int argc, char *argv[])
{
	struct discover_device *dev = script->ctx->device;
	const char *envfile;
	char *buf, *envpath;
	int rc, len;

	/* we only support local filesystems */
	if (!dev->mounted) {
		pb_log("load_env: can't load from a non-mounted device (%s)\n",
				dev->device->id);
		return -1;
	}

	if (argc == 3 && !strcmp(argv[1], "-f"))
		envfile = argv[2];
	else
		envfile = default_envfile;

	envpath = talloc_asprintf(script, "%s/%s",
				script_env_get(script, "prefix") ? : "",
				envfile);

	rc = parser_request_file(script->ctx, dev, envpath, &buf, &len);

	if (!rc)
		rc = parse_buf_to_env(script, buf, len);

	talloc_free(buf);

	return 0;
}

static int update_env(char *buf, int buflen, const char *name,
		const char *value)
{
	/* head and tail define the pointers within the existing data that we
	 * can insert our env entry, end is the last byte of valid environment
	 * data. if tail - head != entrylen, when we'll memmove to create (or
	 * remove) space */
	int i, j, linelen, namelen, valuelen, entrylen, delta, space;
	char *head, *tail, *end;
	bool replace;

	namelen = strlen(name);
	valuelen = strlen(value);
	entrylen = namelen + strlen("=") + valuelen + strlen("\n");

	head = tail = end = NULL;
	replace = false;

	/* For each line (where linelen includes the trailing \n). Find
	 * head, tail and end.
	 */
	for (i = 0; i < buflen; i += linelen) {
		char *eol = NULL, *sep = NULL, *line = buf + i;

		/* find eol and sep */
		for (j = 0; !eol && i + j < buflen; j++) {
			switch (line[j]) {
			case '\n':
				eol = line + j;
				break;
			case '=':
				if (!sep)
					sep = line + j;
				break;
			}
		}

		/* no eol, put the new value at the start of this line,
		 * no need to shift data. This will also match the
		 * padding '#' chars at the end of the entries. */
		if (!eol) {
			if (!head) {
				head = line;
				tail = head + entrylen;
			}
			end = line;
			break;
		}

		linelen = (eol - line) + 1;
		end = line + linelen;

		/* invalid line? may as well overwrite it with valid data */
		if (!sep && !replace) {
			head = line;
			tail = line + linelen;
		}

		/* an existing entry for this var? We set replace, as we prefer
		 * to overwrite an existing entry (the first one, in
		 * particular) rather than use an invalid line.
		 */
		if (sep && !replace && sep - line == namelen &&
				!strncmp(name, line, namelen)) {
			head = line;
			tail = line + linelen;
			replace = true;
		}
	}

	if (!head || !tail || !end) {
		pb_log("grub save_env: can't parse buffer space\n");
		return -1;
	}

	/* how much extra space do we need? */
	delta = entrylen - (tail - head);
	/* how much space do we have? */
	space = (buf + buflen) - end;

	/* check we have enough space. the tail > buf-end check is required
	 * for the case where there was no eol, and we set
	 * tail = head + entrylen
	 */
	if (delta > space || tail > buf + buflen) {
		pb_log("grub save_env: not enough buffer space\n");
		return -1;
	}

	/* create space between head & tail */
	if (delta) {
		/* for positive delta, we need to reduce the copied data size */
		int shiftlen = delta > 0 ? delta : 0;
		memmove(tail + delta, tail, (buf + buflen) - tail - shiftlen);
	}

	/* if we've shifted data down towards head, we'll need to append
	 * padding */
	if (delta < 0)
		memset(buf + buflen + delta, '#', 0 - delta);

	/* set the entry data */
	memcpy(head, name, namelen);
	memcpy(head + namelen, "=", 1);
	memcpy(head + namelen + 1, value, valuelen);
	memcpy(head + namelen + 1 + valuelen, "\n", 1);

	return 0;
}

int builtin_save_env(struct grub2_script *script,
		void *data __attribute__((unused)),
		int argc, char *argv[]);

int builtin_save_env(struct grub2_script *script,
		void *data __attribute__((unused)),
		int argc, char *argv[])
{
	struct discover_device *dev = script->ctx->device;
	int i, rc, len, siglen;
	char *buf, *envpath;
	const char *envfile;

	/* we only support local filesystems */
	if (!dev->mounted) {
		pb_log("save_env: can't save to a non-mounted device (%s)\n",
				dev->device->id);
		return -1;
	}

	if (argc == 3 && !strcmp(argv[1], "-f"))
		envfile = argv[2];
	else
		envfile = default_envfile;

	envpath = talloc_asprintf(script, "%s/%s",
				script_env_get(script, "prefix") ? : "",
				envfile);
	buf = NULL;

	rc = parser_request_file(script->ctx, dev, envpath, &buf, &len);

	siglen = strlen(signature);

	/* we require the environment to be pre-allocated, so abort if
	 * the file isn't present and valid */
	if (rc || len < siglen || memcmp(buf, signature, siglen))
		goto err;

	for (i = 1; i < argc; i++) {
		const char *name, *value;

		name = argv[i];
		value = script_env_get(script, name);

		update_env(buf + siglen, len - siglen, name, value);
	}

	rc = parser_replace_file(script->ctx, dev, envpath, buf, len);

err:
	talloc_free(buf);
	talloc_free(envpath);
	return rc;
}

