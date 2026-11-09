#ifndef PPM_NODE_H
#define PPM_NODE_H

#include <stdint.h>

#define PPM_MAX_NODES 256

typedef struct
{
    uint32_t id;
    uint32_t version;

    char interface[64];

    char media_class[128];

    char name[128];

    char description[256];

    char device_id[32];

    char client_id[32];

    char factory_name[128];

    char object_path[256];

} PpmNode;

extern PpmNode ppm_nodes[PPM_MAX_NODES];
extern int ppm_node_count;

void ppm_node_add(
    uint32_t id,
    uint32_t version,
    const char *interface,
    const char *media_class,
    const char *name,
    const char *description,
    const char *device_id,
    const char *client_id,
    const char *factory_name,
    const char *object_path);

void ppm_node_dump(void);
void ppm_node_dump_sinks(void);
void ppm_node_dump_sources(void);
void ppm_node_dump_streams(void);

#endif
