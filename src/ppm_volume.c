#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ppm_volume.h"
#include "ppm_lookup.h"
#include "ppm_exec.h"

static PpmNode *find_audio_target(const char *name)
{
    PpmNode *n;

    n=ppm_find_sink(name);
    if(n) return n;

    n=ppm_find_source(name);
    if(n) return n;

    n=ppm_find_stream(name);
    if(n) return n;

    return NULL;
}

/* Porcentaje entero o decimal entre 0 y 150 (lo que acepta wpctl con '%'). */
static int volume_is_valid(const char *volume)
{
    char *end;
    double v;

    if(!volume || !*volume)
        return 0;
    v=strtod(volume,&end);
    return *end=='\0' && v>=0.0 && v<=150.0;
}

int ppm_set_volume(const char *target,const char *volume)
{
    PpmNode *n=find_audio_target(target);
    char id[16],level[32];

    if(!n){
        puts("Destino no encontrado.");
        return -1;
    }

    if(!volume_is_valid(volume)){
        printf("Volumen no válido: '%s' (usa un porcentaje entre 0 y 150).\n",volume ? volume : "");
        return -1;
    }

    printf("Volumen de:\n");
    printf("  %s\n",n->description[0] ? n->description : n->name);
    printf("  node=%u\n",n->id);
    printf("  volume=%s\n\n",volume);

    snprintf(id,sizeof(id),"%u",n->id);
    snprintf(level,sizeof(level),"%s%%",volume);
    return ppm_exec("wpctl","set-volume",id,level,NULL);
}

int ppm_set_mute(const char *target,const char *state)
{
    PpmNode *n=find_audio_target(target);
    char id[16];
    int mute;

    if(!n){
        puts("Destino no encontrado.");
        return -1;
    }

    if(!strcmp(state,"on") || !strcmp(state,"1") || !strcmp(state,"true"))
        mute=1;
    else if(!strcmp(state,"off") || !strcmp(state,"0") || !strcmp(state,"false"))
        mute=0;
    else{
        puts("Estado no válido. Usa: on/off");
        return -1;
    }

    printf("Silencio de:\n");
    printf("  %s\n",n->description[0] ? n->description : n->name);
    printf("  node=%u\n",n->id);
    printf("  mute=%s\n\n",mute ? "on" : "off");

    snprintf(id,sizeof(id),"%u",n->id);
    return ppm_exec("wpctl","set-mute",id,mute ? "1" : "0",NULL);
}
