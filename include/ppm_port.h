#ifndef PPM_PORT_H
#define PPM_PORT_H

#include <stdint.h>

#define PPM_MAX_PORTS 512

typedef struct
{
    uint32_t id;
    uint32_t version;

    char direction[32];

    char node_id[32];

    char name[128];
    char alias[128];

} PpmPort;

extern PpmPort ppm_ports[PPM_MAX_PORTS];
extern int ppm_port_count;

void ppm_port_add(
        uint32_t id,
        uint32_t version,
        const char *direction,
        const char *node_id,
        const char *name,
        const char *alias);

void ppm_port_dump(void);

#endif
