#ifndef PPM_LINK_H
#define PPM_LINK_H

#include <stdint.h>

#define PPM_MAX_LINKS 512

typedef struct
{
    uint32_t id;
    uint32_t version;

    char output_port[32];
    char input_port[32];

} PpmLink;

extern PpmLink ppm_links[PPM_MAX_LINKS];
extern int ppm_link_count;

void ppm_link_add(
        uint32_t id,
        uint32_t version,
        const char *out_port,
        const char *in_port);

void ppm_link_dump(void);

#endif
