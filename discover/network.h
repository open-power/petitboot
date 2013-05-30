#ifndef NETWORK_H
#define NETWORK_H

struct network;
struct waitset;

struct network *network_init(void *ctx, struct waitset *waitset, bool dry_run);
int network_shutdown(struct network *network);

#endif /* NETWORK_H */

