#ifndef PLATFORM_H
#define PLATFORM_H

#include <types/types.h>

struct platform {
	const char	*name;
	bool		(*probe)(struct platform *, void *);
	int		(*load_config)(struct platform *, struct config *);
	int		(*save_config)(struct platform *, struct config *);
	void		(*finalise_config)(struct platform *);
	int		(*get_sysinfo)(struct platform *, struct system_info *);
	uint16_t	dhcp_arch_id;
	void		*platform_data;
};

int platform_init(void *ctx);
int platform_fini(void);
const struct platform *platform_get(void);
int platform_get_sysinfo(struct system_info *info);
void platform_finalise_config(void);

/* configuration interface */
const struct config *config_get(void);
int config_set(struct config *config);
void config_set_autoboot(bool autoboot_enabled);

/* for use by the platform-specific storage code */
void config_set_defaults(struct config *config);

#define __platform_ptrname(_n) __platform_ ## _n
#define  _platform_ptrname(_n) __platform_ptrname(_n)

#define register_platform(p) \
	static __attribute__((section("platforms"))) \
		__attribute__((used)) \
		struct platform * _platform_ptrname(__COUNTER__) = &p;

#endif /* PLATFORM_H */

