#include <stdio.h>
#include <string.h>

#include "ppm_node.h"

PpmNode ppm_nodes[PPM_MAX_NODES];
int ppm_node_count = 0;

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
    const char *object_path)
{
    if (ppm_node_count >= PPM_MAX_NODES)
        return;

    PpmNode *n = &ppm_nodes[ppm_node_count++];

    memset(n,0,sizeof(*n));

    n->id=id;
    n->version=version;

#define COPY(dst,src) if(src) snprintf(dst,sizeof(dst),"%s",src)

    COPY(n->interface,interface);
    COPY(n->media_class,media_class);
    COPY(n->name,name);
    COPY(n->description,description);
    COPY(n->device_id,device_id);
    COPY(n->client_id,client_id);
    COPY(n->factory_name,factory_name);
    COPY(n->object_path,object_path);

#undef COPY
}

void ppm_node_dump(void)
{
    puts("");
    puts("Nodes");
    puts("==============================================================");

    for(int i=0;i<ppm_node_count;i++){

        PpmNode *n=&ppm_nodes[i];

        printf("%3u %-22s %s\n",
            n->id,
            n->media_class,
            n->description[0] ? n->description : n->name);
    }
}

void ppm_node_dump_sinks(void)
{
    puts("");
    puts("Audio Sinks");
    puts("==============================================================");

    for(int i=0;i<ppm_node_count;i++){

        PpmNode *n=&ppm_nodes[i];

        if(strcmp(n->media_class,"Audio/Sink")!=0)
            continue;

        printf("%3u dev=%-4s %s\n",
            n->id,
            n->device_id,
            n->description[0] ? n->description : n->name);
    }
}


void ppm_node_dump_sources(void)
{
    int i;

    puts("");
    puts("Audio Sources");
    puts("============================================================");

    for(i=0;i<ppm_node_count;i++)
    {
        PpmNode *n=&ppm_nodes[i];

        if(strcmp(n->media_class,"Audio/Source")!=0)
            continue;

        printf("%4u  dev=%-4s %s\n",
               n->id,
               n->device_id,
               n->description[0] ?
                    n->description :
                    n->name);
    }
}


void ppm_node_dump_streams(void)
{
    int i;

    puts("");
    puts("Streams");
    puts("======================================================================");

    for(i=0;i<ppm_node_count;i++)
    {
        PpmNode *n=&ppm_nodes[i];

        if(strcmp(n->media_class,"Stream/Output/Audio")!=0 &&
           strcmp(n->media_class,"Stream/Input/Audio")!=0)
            continue;

        printf("%4u  %-20s  %s\n",
               n->id,
               n->media_class,
               n->description[0] ?
                    n->description :
                    n->name);
    }
}
