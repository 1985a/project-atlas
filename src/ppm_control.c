#include <stdio.h>
#include <stdlib.h>

#include "ppm_control.h"
#include "ppm_lookup.h"
#include "ppm_metadata.h"

int ppm_move_prepare(const char *stream,const char *sink)
{
    PpmNode *s=ppm_find_stream(stream);
    PpmNode *d=ppm_find_sink(sink);

    if(!s){
        puts("Stream no encontrado.");
        return -1;
    }

    if(!d){
        puts("Sink no encontrado.");
        return -1;
    }

    printf("Moviendo stream:\n");
    printf("  Stream: %s  node=%u\n",s->name,s->id);
    printf("  Sink:   %s  node=%u\n",d->description,d->id);
    printf("\n");

    int ret=ppm_metadata_set_target(
        s->id,
        d->name);

    if(ret==0)
        puts("Move enviado a WirePlumber.");
    else
        puts("Error enviando move.");

    return ret;
}
