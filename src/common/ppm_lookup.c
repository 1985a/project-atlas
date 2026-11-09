#include <string.h>

#include "ppm_lookup.h"

static PpmNode *find_class(
        const char *media_class,
        const char *name)
{
    for(int i=0;i<ppm_node_count;i++)
    {
        PpmNode *n=&ppm_nodes[i];

        if(strcmp(n->media_class,media_class))
            continue;

        if(strstr(n->name,name))
            return n;

        if(strstr(n->description,name))
            return n;
    }

    return NULL;
}

PpmNode *ppm_find_stream(const char *name)
{
    return find_class("Stream/Output/Audio",name);
}

PpmNode *ppm_find_sink(const char *name)
{
    return find_class("Audio/Sink",name);
}

PpmNode *ppm_find_source(const char *name)
{
    return find_class("Audio/Source",name);
}
