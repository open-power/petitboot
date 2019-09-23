#ifndef PLATFORM_H
#define PLATFORM_H

#include <types/types.h>
#include <param_list/param_list.h>

struct platform {
	const char	*name;
	bool		(*probe)(struct platform *, void *);
	int		(*load_config)(struct platform *, struct config *);
	int		(*save_config)(struct platform *, struct config *);
	void		(*pre_boot)(struct platform *,
				const struct config *);
	int		(*get_sysinfo)(struct platform *, struct system_info *);
	bool		(*restrict_clients)(struct platform *);
	int		(*set_password)(struct platform *, const char *hash);
	bool		(*preboot_check)(struct platform *,
					const struct config *,
					const char *image,
					char **err_msg);
	uint16_t	dhcp_arch_id;
	void		*platform_data;
};

int platform_init(void *ctx);
int platform_fini(void);
const struct platform *platform_get(void);
int platform_get_sysinfo(struct system_info *info);
bool platform_restrict_clients(void);
int platform_set_password(const char *hash);
void platform_pre_boot(void);
bool platform_preboot_check(const char *image, char **err_msg);

/* configuration interface */
const struct config *config_get(void);
int config_set(struct config *config);
void config_set_defaults(struct config *config);
void config_set_autoboot(bool autoboot_enabled);
void config_populate_bootdev(struct config *config,
	const struct param_list *pl);
void config_populate_all(struct config *config, const struct param_list *pl);

void params_update_network_values(struct param_list *pl,
	const char *param_name, const struct config *config);
void params_update_bootdev_values(struct param_list *pl,
	const char *param_name, const struct config *config);

#define __platform_ptrname(_n) __platform_ ## _n
#define  _platform_ptrname(_n) __platform_ptrname(_n)

#define register_platform(p) \
	static __attribute__((section("platforms"))) \
		__attribute__((used)) \
		struct platform * _platform_ptrname(__COUNTER__) = &p;

#endif /* PLATFORM_H */

