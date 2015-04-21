#ifndef _DT_H
#define _DT_H

#include "ipmi.h"

int get_ipmi_sensor(void *t, enum ipmi_sensor_ids sensor_id);

#endif /* _IPMI_H */
