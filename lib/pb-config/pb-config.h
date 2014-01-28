#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#include <stdbool.h>
#include <stdint.h>

#include <types/types.h>


int config_init(void *ctx);
const struct config *config_get(void);
int config_set(struct config *config);
void config_set_autoboot(bool autoboot_enabled);
int config_fini(void);

/* for use by the storage backends */
void config_set_defaults(struct config *config);

struct config *config_copy(void *ctx, const struct config *src);

#endif /* CONFIGURATION_H */

