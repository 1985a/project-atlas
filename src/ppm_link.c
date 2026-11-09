#include <stdio.h>
#include <string.h>

#include "ppm_link.h"

PpmLink ppm_links[PPM_MAX_LINKS];
int ppm_link_count=0;

void ppm_link_add(
        uint32_t id,
        uint32_t version,
        const char *out_port,
        const char *in_port)
{
    if(ppm_link_count>=PPM_MAX_LINKS)
        return;

    PpmLink *l=&ppm_links[ppm_link_count++];

    memset(l,0,sizeof(*l));

    l->id=id;
    l->version=version;

    if(out_port)
        snprintf(l->output_port,sizeof(l->output_port),"%s",out_port);

    if(in_port)
        snprintf(l->input_port,sizeof(l->input_port),"%s",in_port);
}

void ppm_link_dump(void)
{
    int i;

    puts("");
    puts("Links");
    puts("==========================================================");

    for(i=0;i<ppm_link_count;i++)
    {
        printf("%4u   %s  --->  %s\n",
            ppm_links[i].id,
            ppm_links[i].output_port,
            ppm_links[i].input_port);
    }
}
