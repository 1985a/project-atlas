#include "ppm_link.h"
#include "ppm_port.h"
#ifndef PPM_REGISTRY_H
#define PPM_REGISTRY_H

#include <stdint.h>
#include <spa/utils/dict.h>

void ppm_registry_parse(
    uint32_t id,
    uint32_t version,
    const char *type,
    const struct spa_dict *props);

#endif
