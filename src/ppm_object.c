#include <string.h>

#include "ppm_object.h"

typedef struct
{
    const char *iface;
    PpmObjectType type;
    const char *name;
} Map;

static const Map map[] =
{
    {"PipeWire:Interface:Core",PPM_OBJECT_CORE,"Core"},
    {"PipeWire:Interface:Module",PPM_OBJECT_MODULE,"Module"},
    {"PipeWire:Interface:Factory",PPM_OBJECT_FACTORY,"Factory"},
    {"PipeWire:Interface:Device",PPM_OBJECT_DEVICE,"Device"},
    {"PipeWire:Interface:Node",PPM_OBJECT_NODE,"Node"},
    {"PipeWire:Interface:Port",PPM_OBJECT_PORT,"Port"},
    {"PipeWire:Interface:Link",PPM_OBJECT_LINK,"Link"},
    {"PipeWire:Interface:Client",PPM_OBJECT_CLIENT,"Client"},
    {"PipeWire:Interface:Metadata",PPM_OBJECT_METADATA,"Metadata"},
    {"PipeWire:Interface:Profiler",PPM_OBJECT_PROFILER,"Profiler"},
    {"PipeWire:Interface:SecurityContext",PPM_OBJECT_SECURITY_CONTEXT,"SecurityContext"},
    {NULL,PPM_OBJECT_UNKNOWN,NULL}
};

PpmObjectType
ppm_object_type_from_interface(const char *interface)
{
    for(int i=0; map[i].iface; i++)
        if(strcmp(interface,map[i].iface)==0)
            return map[i].type;

    return PPM_OBJECT_UNKNOWN;
}

const char *
ppm_object_type_name(PpmObjectType type)
{
    for(int i=0; map[i].iface; i++)
        if(map[i].type==type)
            return map[i].name;

    return "Unknown";
}
