#ifndef _PB_EVENT_H
#define _PB_EVENT_H

enum event_type {
	EVENT_TYPE_UDEV = 10,
	EVENT_TYPE_USER,
};

enum event_action {
	EVENT_ACTION_ADD = 20,
	EVENT_ACTION_REMOVE,
};

struct event {
	enum event_type type;
	enum event_action action;
	char *device;

	struct param {
		char *name;
		char *value;
	} *params;
	int n_params;
};

int event_parse_ad_message(struct event *event, char *buf, int len);
const char *event_get_param(const struct event *event, const char *name);

#endif /* _PB_EVENT_H */
