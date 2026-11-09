#ifndef PROFILE_H
#define PROFILE_H

typedef struct {
    char id[64];
    char name[128];
    char description[256];

    int enabled;
    int rate;
    int quantum;

    char default_sink[128];
    char default_source[128];

    char alsa_preset[128];

    int restore_volume;
    int restore_defaults;
    int restore_links;

    int sink_volume;
    int sink_muted;
    int source_volume;
    int source_muted;

    char wireplumber_profile[128];
    char easyeffects_preset[128];

    int priority;

    /* v0.2 */
    char dsp_preset[64];     /* preset DSP de la Hardware DB (hardware/<id>/dsp/<n>.conf) */
    char category[32];       /* juegos / música / vídeo / sistema ... (GUI) */
    char icon[64];           /* icono simbólico (GUI) */
    char source[16];         /* "user" o "system" (al cargar) */
} AudioProfile;

int profile_load(const char *id, AudioProfile *p);
int profile_load_from_file(const char *id, const char *path, AudioProfile *p);

/* ids ordenados por prioridad descendente */
int profile_ids(char ids[][64], int max);
void profile_list(void);
void profile_print(const AudioProfile *p);

#endif
