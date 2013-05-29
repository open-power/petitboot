#ifndef STORAGE_H
#define STORAGE_H

#include <stdbool.h>

struct config;

struct config_storage {
	int	(*load)(struct config_storage *st, struct config *config);
};

struct config_storage *create_powerpc_nvram_storage(void *ctx);
struct config_storage *create_test_storage(void *ctx);
struct config_storage *create_null_storage(void *ctx);

#endif /* STORAGE_H */

