#ifndef PPM_DEVICE_H
#define PPM_DEVICE_H

#include <stdint.h>

#define PPM_MAX_DEVICES 128

typedef struct
{
    uint32_t id;
    uint32_t version;

    char api[64];
    char name[128];
    char description[256];
    char object_path[256];
} PpmDevice;

extern PpmDevice ppm_devices[PPM_MAX_DEVICES];
extern int ppm_device_count;

void ppm_device_add(
    uint32_t id,
    uint32_t version,
    const char *api,
    const char *name,
    const char *description,
    const char *object_path);

void ppm_device_dump(void);

#endif
