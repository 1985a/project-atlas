#ifndef ATLAS_AUDIO_H
#define ATLAS_AUDIO_H

/*
 * Instantánea del estado de audio a través de pipewire-pulse (pactl -f json).
 *
 * Todo se identifica por NOMBRE (node.name / card name), nunca por IDs
 * numéricos de PipeWire, que cambian al reconstruirse el grafo.
 */

#include <json-glib/json-glib.h>

#define ATLAS_MAX_CH 16
#define ATLAS_MAX_CARDS 16
#define ATLAS_MAX_NODES 48
#define ATLAS_MAX_PROFILES 48
#define ATLAS_NAME 256

typedef struct {
    int channels;
    unsigned value[ATLAS_MAX_CH];   /* volumen bruto de Pulse (65536 = 100 %) */
} AtlasVolume;

typedef struct {
    char name[ATLAS_NAME];
    char description[ATLAS_NAME];
    char state[24];                 /* RUNNING / IDLE / SUSPENDED */
    char card_name[ATLAS_NAME];     /* alsa_card.pci-... (vacío si no es ALSA) */
    char api[24];                   /* alsa / bluez5 / ... */
    int alsa_card;                  /* -1 si no aplica */
    int alsa_device;                /* -1 si no aplica */
    int mute;
    int is_monitor;
    AtlasVolume volume;
} AtlasNode;

typedef struct {
    char name[128];
    char description[160];
    int sinks;
    int sources;
    int available;
} AtlasCardProfile;

typedef struct {
    char name[ATLAS_NAME];
    char description[ATLAS_NAME];
    char active_profile[128];
    char bus[24];
    char vendor_id[16];
    char product_id[16];
    char serial[128];
    char bus_path[128];
    int alsa_card;
    int nprofiles;
    AtlasCardProfile profiles[ATLAS_MAX_PROFILES];
} AtlasCard;

typedef struct {
    int server_ok;
    char server_name[128];
    char default_sink[ATLAS_NAME];
    char default_source[ATLAS_NAME];
    char sample_spec[64];

    int ncards;
    AtlasCard cards[ATLAS_MAX_CARDS];
    int nsinks;
    AtlasNode sinks[ATLAS_MAX_NODES];
    int nsources;
    AtlasNode sources[ATLAS_MAX_NODES];

    int playback_streams;           /* sink-inputs */
    int playback_streams_active;    /* sin pausar (corked=false) */
    int capture_streams;
} AtlasAudio;

/* Devuelve 0 si pipewire-pulse respondió. Rellena lo que pueda. */
int atlas_audio_query(AtlasAudio *a);

/* Parsers expuestos para pruebas. */
int atlas_audio_parse_info(AtlasAudio *a, const char *json);
int atlas_audio_parse_cards(AtlasAudio *a, const char *json);
int atlas_audio_parse_nodes(AtlasNode *arr, int max, int *count, const char *json);
int atlas_audio_parse_inputs(AtlasAudio *a, const char *json);

const AtlasNode *atlas_audio_find_sink(const AtlasAudio *a, const char *name);
const AtlasNode *atlas_audio_find_source(const AtlasAudio *a, const char *name);
const AtlasNode *atlas_audio_default_sink(const AtlasAudio *a);
const AtlasNode *atlas_audio_default_source(const AtlasAudio *a);
const AtlasCard *atlas_audio_find_card(const AtlasAudio *a, const char *name);
const AtlasCard *atlas_audio_card_by_alsa(const AtlasAudio *a, int alsa_card);

double atlas_volume_percent(const AtlasVolume *v);   /* promedio */

/* Acciones (por nombre). Devuelven 0 en éxito. */
int atlas_audio_set_card_profile(const char *card, const char *profile);
int atlas_audio_cycle_card(const char *card, const char *profile);
int atlas_audio_set_default_sink(const char *name);
int atlas_audio_set_default_source(const char *name);
int atlas_audio_set_volume(int is_sink, const char *name, const AtlasVolume *v);
int atlas_audio_set_volume_percent(int is_sink, const char *name, int percent);
int atlas_audio_set_mute(int is_sink, const char *name, int mute);

/* Espera a que pactl responda y (opcional) exista el sink indicado. */
int atlas_audio_wait_ready(const char *sink_name, int timeout_ms);

/*
 * Restaura sobre el estado actual lo que contenga la instantánea:
 * perfiles de tarjeta, defaults y volumen/mute. Escribe un resumen en log
 * (puede ser NULL). Devuelve el número de acciones aplicadas.
 */
int atlas_audio_restore(const AtlasAudio *snap, int restore_volumes, char *log, size_t logn);

void atlas_audio_json(JsonBuilder *b, const AtlasAudio *a);

/* Serializa/carga la parte restaurable de una instantánea (archivo JSON). */
int atlas_audio_save_snapshot(const AtlasAudio *a, const char *path);
int atlas_audio_load_snapshot(AtlasAudio *a, const char *path);

#endif
