#ifndef ATLAS_BASE_H
#define ATLAS_BASE_H

/*
 * Base de volumen del usuario.
 *
 * El archivo de alsactl del usuario (p. ej. ~/.config/asound.state) es la
 * referencia estable de la tarjeta. Se importa en capas DSP del usuario:
 *
 *   ~/.config/audio-profile/hardware/<id>/dsp/
 *       base.conf      configuración de altavoces (salida, canales, full-range,
 *                      redirección de graves): sustituye al base.conf de fábrica
 *       volumes.conf   volumen y switches de cada canal: se aplica SIEMPRE,
 *                      con cualquier perfil
 *       personal.conf  efectos y ecualizador del archivo -> perfil "personal"
 *
 * Orden al aplicar un perfil:  base -> volumes -> [headphones] -> efectos.
 * Efectos = personal (por defecto, profiles.effects=personal) o el preset
 * del perfil (profiles.effects=profile). El daemon vigila que la tarjeta
 * siga así (base.enforce) y corrige desvíos.
 */

#include <stddef.h>

#define ATLAS_STATE_MAX_CTL 192

typedef struct {
    char iface[16];
    char name[128];
    char value[256];      /* "95,95" / "on" / "Speakers" */
    int count;
} AtlasStateCtl;

typedef struct {
    char card[64];
    int n;
    AtlasStateCtl c[ATLAS_STATE_MAX_CTL];
} AtlasStateCard;

typedef enum {
    ATLAS_CTL_VOLUME = 0,   /* volumen/switch de canales */
    ATLAS_CTL_SPEAKER,      /* configuración de altavoces */
    ATLAS_CTL_EFFECT,       /* FX:, EQ */
    ATLAS_CTL_IGNORE        /* jacks, IEC958, channel maps, capture FX... */
} AtlasCtlClass;

/* Parsea el bloque state.<card> de un archivo alsactl. 0 si lo encontró. */
int atlas_state_parse(const char *text, const char *card, AtlasStateCard *out);
AtlasCtlClass atlas_ctl_classify(const AtlasStateCtl *c);

int atlas_cmd_base(int argc, char **argv);

/* Aplica base de altavoces + volúmenes (+ tus efectos si profiles.effects=personal). */
int atlas_base_apply(void);

/* Vigilancia (daemon): corrige controles que se desvían de tu base.
 * Devuelve cuántos corrigió; `log` describe los cambios. */
int atlas_base_enforce(char *log, size_t logn);
int atlas_value_matches(const char *want, const char *have);
int atlas_base_diff(char names[][128], char values[][128], int n, const AtlasStateCard *live, int *out);

#endif
