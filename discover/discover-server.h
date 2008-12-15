#ifndef _DISCOVER_SERVER_H
#define _DISCOVER_SERVER_H

struct discover_server;

struct discover_server *discover_server_init(void);

void discover_server_destroy(struct discover_server *server);

#endif /* _DISCOVER_SERVER_H */
