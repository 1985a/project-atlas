#ifndef ATLAS_DSP_H
#define ATLAS_DSP_H

/*
 * DSP por hardware (controles ALSA de la tarjeta).
 *
 * Los presets viven en la Hardware DB, por tarjeta:
 *   <data>/hardware/<id>/dsp/<preset>.conf
 *   ~/.config/audio-profile/hardware/<id>/dsp/<preset>.conf   (prioridad)
 *
 * Formato: una línea por control ALSA, "Nombre exacto del control=valor".
 * Antes de cualquier preset se aplica base.conf si existe (configuración
 * de altavoces: 5.1, crossover, full-range...).
 */

#include <limits.h>

#include <json-glib/json-glib.h>

#define ATLAS_DSP_MAX_CTL 128

typedef struct {
    char name[128];
    char type[16];       /* INTEGER / BOOLEAN / ENUMERATED / ... */
    char access[16];
    int count;
    long min, max;
    char values[128];    /* tal cual de amixer: "on,on" / "237,237" / "4" */
    int nitems;
    char items[32][48];
} AtlasDspControl;

typedef struct {
    int card;
    char hw_id[64];
    int n;
    AtlasDspControl c[ATLAS_DSP_MAX_CTL];
} AtlasDspState;

/* Usado por profile_apply (mockeable en pruebas). Respeta profiles.effects:
 * por defecto los efectos son siempre los del usuario (personal). */
int ppm_dsp_apply(const char *preset);
/* Aplica exactamente ese preset (comando `dsp apply`). */
int atlas_dsp_apply_exact(const char *preset);
int atlas_dsp_store_value(const char *path, const char *name, const char *value);
const char *atlas_effects_preset(const char *profile_preset, const char *hw_id);
/* Archivos de capa de un preset en orden; devuelve cuántos existen. */
int atlas_dsp_layer_files(const char *preset, const char *hw_id, const char *hw_dir,
                          char files[][PATH_MAX], int max, int *missing_preset);

int atlas_dsp_parse_contents(const char *amixer_contents, AtlasDspState *s);
int atlas_dsp_parse_preset(const char *text, char names[][128], char values[][128], int max);

/* Tarjeta del sink por defecto y su entrada en la DB. */
int atlas_dsp_target(int *card, char *hw_id, size_t n, char *hw_dir, size_t dn);

int atlas_dsp_apply_file(int card, const char *path, int *applied, int *failed);

int atlas_cmd_dsp(int argc, char **argv);

#endif
