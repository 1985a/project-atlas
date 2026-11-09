#ifndef PPM_OBJECT_H
#define PPM_OBJECT_H

#include <stdint.h>

typedef enum
{
    PPM_OBJECT_UNKNOWN = 0,
    PPM_OBJECT_CORE,
    PPM_OBJECT_MODULE,
    PPM_OBJECT_FACTORY,
    PPM_OBJECT_DEVICE,
    PPM_OBJECT_NODE,
    PPM_OBJECT_PORT,
    PPM_OBJECT_LINK,
    PPM_OBJECT_CLIENT,
    PPM_OBJECT_METADATA,
    PPM_OBJECT_SECURITY_CONTEXT,
    PPM_OBJECT_PROFILER
} PpmObjectType;

PpmObjectType ppm_object_type_from_interface(const char *interface);

const char *ppm_object_type_name(PpmObjectType type);

#endif
