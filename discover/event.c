#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <string.h>

#include <log/log.h>
#include <talloc/talloc.h>

#include "event.h"

#define streq(a, b) (!strcasecmp((a), (b)))

/**
 * event_parse_ad_header - Parse an <action>@<device> event header.
 *
 * The buffer is modified in place.
 * Returns zero on success.
 */

static int event_parse_ad_header(char *buf, int len, enum event_action *action,
	char **device)
{
	int headerlen;
	char *sep;

	*action = 0;
	*device = NULL;
	headerlen = strnlen(buf, len);

	if (!headerlen) {
		pb_log_fn("bad header, no data\n");
		return -1;
	}

	/* we should see an <action>@<device>\0 at the head of the buffer */
	sep = strchr(buf, '@');
	if (!sep) {
		pb_log_fn("bad header: %s\n", buf);
		return -1;
	}

	/* terminate the action string */
	*sep = '\0';

	if (streq(buf, "add"))
		*action = EVENT_ACTION_ADD;
	else if (streq(buf, "remove"))
		*action = EVENT_ACTION_REMOVE;
	else if (streq(buf, "url"))
		*action = EVENT_ACTION_URL;
	else if (streq(buf, "dhcp"))
		*action = EVENT_ACTION_DHCP;
	else if (streq(buf, "boot"))
		*action = EVENT_ACTION_BOOT;
	else if (streq(buf, "sync"))
		*action = EVENT_ACTION_SYNC;
	else if (streq(buf, "plugin"))
		*action = EVENT_ACTION_PLUGIN;
	else {
		pb_log_fn("unknown action: %s\n", buf);
		return -1;
	}

	if (!*(sep + 1)) {
		pb_log_fn("bad device: %s\n", buf);
		return -1;
	}

	*device = sep + 1;
	return headerlen;
}

/**
 * event_parse_params - Parse a <name>=<value> buffer.
 *
 * The buffer is not modified.
 */

static void event_parse_params(struct event *event, const char *buf, int len)
{
	int param_len, name_len, value_len;
	struct param *param;
	char *sep;

	for (; len > 0; len -= param_len + 1, buf += param_len + 1) {

		/* find the length of the whole parameter */
		param_len = strnlen(buf, len);
		if (!param_len) {
			/* multiple NULs? skip over */
			param_len = 1;
			continue;
		}

		/* update the params array */
		event->params = talloc_realloc(event, event->params,
					struct param, ++event->n_params);
		param = &event->params[event->n_params - 1];

		sep = memchr(buf, '=', param_len);
		if (!sep) {
			name_len = param_len;
			value_len = 0;
			param->value = "";
		} else {
			name_len = sep - buf;
			value_len = param_len - name_len - 1;
			param->value = talloc_strndup(event, sep + 1,
					value_len);
		}
		param->name = talloc_strndup(event, buf, name_len);
	}
}

int event_parse_ad_message(struct event *event, char *buf, int len)
{
	enum event_action action;
	int headerlen;
	char *device;

	headerlen = event_parse_ad_header(buf, len, &action, &device);

	if (headerlen <= 0)
		return -1;

	/* now we have an action and a device, we can construct the event */
	event->action = action;
	event->device = talloc_strdup(event, device);
	event->n_params = 0;
	event->params = NULL;

	len -= headerlen + 1;
	buf += headerlen + 1;
	event_parse_params(event, buf, len);

	return 0;
}

const char *event_get_param(const struct event *event, const char *name)
{
	int i;

	for (i = 0; i < event->n_params; i++)
		if (!strcasecmp(event->params[i].name, name))
			return event->params[i].value;

	return NULL;
}

void event_set_param(struct event *event, const char *name, const char *value)
{
	struct param *param;
	int i;

	/* if it's already present, replace the value of the old param */
	for (i = 0; i < event->n_params; i++) {
		param = &event->params[i];
		if (!strcasecmp(param->name, name)) {
			talloc_free(param->value);
			param->value = talloc_strdup(event, value);
			return;
		}
	}

	/* not found - create a new param */
	event->params = talloc_realloc(event, event->params,
				struct param, ++event->n_params);
	param = &event->params[event->n_params - 1];

	param->name = talloc_strdup(event, name);
	param->value = talloc_strdup(event, value);
}
