#ifndef ATLAS_HEALTH_H
#define ATLAS_HEALTH_H

/*
 * Salud del audio por niveles. Cada nivel responde una pregunta distinta:
 *
 *   1. services   ¿están vivos pipewire, pipewire-pulse y wireplumber?
 *   2. connection ¿responde el servidor, existe un sink por defecto real?
 *   3. hardware   ¿la tarjeta consume muestras? (sonda hw_ptr)
 *   4. acoustic   ¿se oye? -> no medible automáticamente; requiere persona
 *
 * Un nivel superior solo es fiable si los inferiores están bien, pero el
 * informe nunca deduce un nivel a partir de otro.
 */

#include <json-glib/json-glib.h>

#include "atlas_audio.h"
#include "atlas_probe.h"

typedef enum { ATLAS_OK = 0, ATLAS_WARN, ATLAS_FAIL, ATLAS_UNKNOWN } AtlasLevel;

#define ATLAS_MAX_UNITS 8

typedef struct {
    char unit[96];
    char active[24];      /* active / inactive / failed */
    char sub[24];
    char result[32];
    int loaded;
} AtlasUnit;

typedef struct {
    AtlasLevel services;
    AtlasLevel connection;
    AtlasLevel hardware;
    AtlasLevel acoustic;
    AtlasLevel overall;

    int nunits;
    AtlasUnit units[ATLAS_MAX_UNITS];     /* audio */
    int nportals;
    AtlasUnit portals[ATLAS_MAX_UNITS];   /* xdg-desktop-portal-* */

    char services_detail[256];
    char connection_detail[512];
    char hardware_detail[640];

    AtlasAudio audio;
    AtlasProbe probe;
    int probed;
} AtlasHealth;

const char *atlas_level_name(AtlasLevel l);
const char *atlas_level_icon(AtlasLevel l);

int atlas_unit_query(const char *unit, AtlasUnit *u);
int atlas_health_check(AtlasHealth *h, int allow_active_probe);
void atlas_health_json(JsonBuilder *b, const AtlasHealth *h);
void atlas_health_print(const AtlasHealth *h);

/* Lectura de clock.* en la metadata "settings" de PipeWire. */
typedef struct {
    int rate, quantum, min_quantum, max_quantum;
    int force_rate, force_quantum;
    char allowed_rates[64];
    int ok;
} AtlasClock;
int atlas_clock_parse(const char *pw_metadata_out, AtlasClock *c);
int atlas_clock_query(AtlasClock *c);

/* Comandos */
int atlas_cmd_status(int probe);
int atlas_cmd_doctor(void);
int atlas_cmd_check(void);
int atlas_cmd_test_tone(int argc, char **argv);

/* Perfil actual (estado) */
void atlas_current_profile(char *buf, size_t n);

#endif
