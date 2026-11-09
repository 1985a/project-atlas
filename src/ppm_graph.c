#include "ppm_graph.h"
#include <stdio.h>
#include <string.h>

#include "ppm_device.h"
#include "ppm_node.h"
#include "ppm_port.h"
#include "ppm_link.h"

static PpmPort *find_port(const char *id)
{
    int i;

    for(i=0;i<ppm_port_count;i++)
    {
        char tmp[32];

        snprintf(tmp,sizeof(tmp),"%u",ppm_ports[i].id);

        if(strcmp(tmp,id)==0)
            return &ppm_ports[i];
    }

    return NULL;
}

static PpmNode *find_node(const char *id)
{
    int i;

    for(i=0;i<ppm_node_count;i++)
    {
        char tmp[32];

        snprintf(tmp,sizeof(tmp),"%u",ppm_nodes[i].id);

        if(strcmp(tmp,id)==0)
            return &ppm_nodes[i];
    }

    return NULL;
}

void ppm_graph_dump(void)
{
    int d,n,p,l;

    puts("");
    puts("GRAPH");
    puts("================================================================");

    for(d=0; d<ppm_device_count; d++)
    {
        printf("\nDevice %u : %s\n",
            ppm_devices[d].id,
            ppm_devices[d].description);

        char devid[32];
        snprintf(devid,sizeof(devid),"%u",ppm_devices[d].id);

        for(n=0;n<ppm_node_count;n++)
        {
            if(strcmp(ppm_nodes[n].device_id,devid))
                continue;

            printf("  └── Node %-3u %s\n",
                ppm_nodes[n].id,
                ppm_nodes[n].description);

            char nodeid[32];
            snprintf(nodeid,sizeof(nodeid),"%u",ppm_nodes[n].id);

            for(p=0;p<ppm_port_count;p++)
            {
                if(strcmp(ppm_ports[p].node_id,nodeid))
                    continue;

                printf("        └── Port %-3u %s\n",
                    ppm_ports[p].id,
                    ppm_ports[p].alias);

                char portid[32];
                snprintf(portid,sizeof(portid),"%u",ppm_ports[p].id);

                for(l=0;l<ppm_link_count;l++)
                {
                    const char *remote=NULL;

                    if(!strcmp(ppm_links[l].output_port,portid))
                        remote=ppm_links[l].input_port;

                    else if(!strcmp(ppm_links[l].input_port,portid))
                        remote=ppm_links[l].output_port;

                    if(!remote)
                        continue;

                    PpmPort *rp=find_port(remote);

                    printf("               └── Link %-3u\n",
                        ppm_links[l].id);

                    if(rp)
                    {
                        printf("                     └── Remote Port %-3u %s\n",
                            rp->id,
                            rp->alias);

                        PpmNode *rn=find_node(rp->node_id);

                        if(rn)
                        {
                            printf("                           └── Remote Node %-3u %s\n",
                                rn->id,
                                rn->description);

                            if(!strcmp(rn->media_class,"Stream/Output/Audio") ||
                               !strcmp(rn->media_class,"Stream/Input/Audio"))
                            {
                                printf("                                 └── Stream : %s\n",
                                    rn->name);
                            }
                        }
                    }
                }
            }
        }
    }
}


