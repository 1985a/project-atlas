#include <stdio.h>
#include <string.h>

#include "ppm_port.h"

PpmPort ppm_ports[PPM_MAX_PORTS];
int ppm_port_count=0;

void ppm_port_add(
        uint32_t id,
        uint32_t version,
        const char *direction,
        const char *node_id,
        const char *name,
        const char *alias)
{
    if(ppm_port_count>=PPM_MAX_PORTS)
        return;

    PpmPort *p=&ppm_ports[ppm_port_count++];

    memset(p,0,sizeof(*p));

    p->id=id;
    p->version=version;

#define COPY(a,b) if(b) snprintf(a,sizeof(a),"%s",b)

    COPY(p->direction,direction);
    COPY(p->node_id,node_id);
    COPY(p->name,name);
    COPY(p->alias,alias);

#undef COPY
}

void ppm_port_dump(void)
{
    int i;

    puts("");
    puts("Ports");
    puts("====================================================================");

    for(i=0;i<ppm_port_count;i++)
    {
        PpmPort *p=&ppm_ports[i];

        printf("%4u node=%-4s %-8s %s\n",
                p->id,
                p->node_id,
                p->direction,
                p->alias[0] ? p->alias : p->name);
    }
}
