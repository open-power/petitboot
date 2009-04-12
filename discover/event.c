#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#define _GNU_SOURCE
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
	char *sep;

	*action = 0;
	*device = NULL;

	/* we should see an <action>@<device>\0 at the head of the buffer */
	sep = strchr(buf, '@');
	if (!sep) {
		pb_log("%s: bad header: %s\n", __func__, buf);
		return -1;
	}

	/* terminate the action string */
	*sep = '\0';
	len -= sep - buf + 1;

	if (streq(buf, "add"))
		*action = EVENT_ACTION_ADD;
	else if (streq(buf, "remove"))
		*action = EVENT_ACTION_REMOVE;
	else {
		pb_log("%s: unknown action: %s\n", __func__, buf);
		return -1;
	}

	if (!*(sep + 1)) {
		pb_log("%s: bad device: %s\n", __func__, buf);
		return -1;
	}

	*device = sep + 1;
	return 0;
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

		/* find the separator */
		sep = memchr(buf, '=', param_len);
		if (!sep)
			continue;

		name_len = sep - buf;
		value_len = param_len - name_len - 1;

		/* update the params array */
		event->params = talloc_realloc(event, event->params,
					struct param, ++event->n_params);
		param = &event->params[event->n_params - 1];

		param->name = talloc_strndup(event, buf, name_len);
		param->value = talloc_strndup(event, sep + 1, value_len);
	}
}

int event_parse_ad_message(struct event *event, char *buf, int len)
{
	int result;
	char *device;
	enum event_action action;
	int device_len;

	result = event_parse_ad_header(buf, len, &action, &device);

	if (result)
		return -1;

	device_len = strlen(device);

	/* now we have an action and a device, we can construct the event */
	event->action = action;
	event->device = talloc_strndup(event, device, device_len);
	event->n_params = 0;
	event->params = NULL;

	len -= device_len + 1;
	event_parse_params(event, device + device_len + 1, len);

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
