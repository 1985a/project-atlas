#ifndef ATLAS_AUTO_H
#define ATLAS_AUTO_H

/*
 * Perfiles automáticos según lo que suena.
 *
 * Reglas (primera coincidencia por stream; el usuario primero):
 *   ~/.config/audio-profile/auto.rules
 *   <data>/rules/auto.rules
 *
 *   # perfil      condiciones (todas deben cumplirse)
 *   retro         binary=retroarch
 *   fps           steam=1172470
 *   gaming        steam=*
 *   video         binary=brave title~YouTube
 *
 * Condiciones: binary=<glob>  app~<texto>  media~<texto>  role=<rol>
 *              title~<texto> (ventana en Sway o MPRIS en reproducción)
 *              url~<texto> (MPRIS)  steam=<appid|*>  genre=<racing|fps|adventure|gaming>
 *
 * Entre varios streams gana el perfil con mayor "priority".
 */

#include <json-glib/json-glib.h>

#define ATLAS_AUTO_MAX_STREAMS 32

typedef struct {
    char binary[96];
    char app[128];
    char media[160];
    char role[32];
    char title[256];
    int pid;
    int corked;
    char steam_appid[24];
    char genre[16];         /* clase del juego de Steam (racing/fps/adventure/gaming) */
    char url[256];          /* URL publicada por MPRIS (si el reproductor la da) */
    char mpris[256];        /* títulos MPRIS en reproducción del mismo proceso */
    char profile[64];       /* perfil elegido por las reglas ("" = ninguno) */
    char rule[256];
} AtlasAutoStream;

typedef struct {
    int enabled;
    int manual_lock;
    char lock_winner[64];   /* lo que el automático habría elegido al fijar el perfil a mano */
    char current[64];
    char reason[256];
    char updated[64];
} AtlasAutoState;

int atlas_auto_state_load(AtlasAutoState *s);
int atlas_auto_state_save(const AtlasAutoState *s);
void atlas_auto_mark_manual(const char *profile);
/* Elección manual temporal: se suelta cuando cambia lo que suena, es decir,
 * cuando el automático elegiría algo distinto de lo que elegía al fijarla. */
int atlas_auto_should_release(const AtlasAutoState *s, const char *winner);
/* "auto" | "auto-paused" | "manual" */
const char *atlas_auto_mode(const AtlasAutoState *s);

/* Coincidencia de una regla con un stream (pura, para pruebas). */
int atlas_auto_rule_matches(const char *conds, const AtlasAutoStream *st);

int atlas_auto_streams(AtlasAutoStream *out, int max);
/* Evalúa y devuelve el perfil ganador ("" si no hay streams). */
int atlas_auto_evaluate(AtlasAutoStream *streams, int n, char *winner, size_t wn,
                        char *reason, size_t rn);
int atlas_auto_assign(const char *profile, const char *conds);

/* Pasada del daemon: aplica el perfil si cambió el ganador (con histéresis
 * de 3 s). Devuelve en cuántos ms conviene volver a mirar para confirmar un
 * cambio que aún no se ha aplicado, o 0 si no hace falta. */
int atlas_auto_tick(void);

int atlas_cmd_auto(int argc, char **argv);

/* Aplica un perfil por id. by_auto=1 no activa el bloqueo manual. */
int atlas_apply_profile_id(const char *id, int by_auto);

#endif
