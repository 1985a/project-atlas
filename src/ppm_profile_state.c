#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "atlas.h"
#include "atlas_audio.h"
#include "ppm_profile_state.h"

/*
 * Instantáneas completas del usuario: `audio-profile save|load <nombre>`.
 *
 *   ~/.config/audio-profile/snapshots/<nombre>/
 *       audio.json     defaults, perfiles de tarjeta, volumen/mute por nombre
 *       alsa.state     controles ALSA de cada tarjeta (alsactl store)
 *       pw-dump.json   referencia para diagnóstico (no se restaura)
 *
 * Los nombres se validan: no hay interpolación en shell.
 */

static int snap_dir(const char *name, char *out, size_t n)
{
    if (!atlas_safe_name(name)) {
        fprintf(stderr, "Nombre no válido: usa letras, números, '.', '_' o '-'.\n");
        return -1;
    }
    snprintf(out, n, "%s/snapshots/%s", atlas_config_dir(), name);
    return 0;
}

int ppm_profile_save(const char *name)
{
    char dir[PATH_MAX], path[PATH_MAX];
    char *dump = NULL;
    AtlasAudio *a = malloc(sizeof(*a));
    int r = 0;

    if (!a)
        return -1;
    if (snap_dir(name, dir, sizeof(dir)) != 0 || atlas_mkdirs(dir) != 0) {
        free(a);
        return -1;
    }
    printf("Guardando instantánea: %s\n", dir);

    if (atlas_audio_query(a) != 0) {
        fprintf(stderr, "pipewire-pulse no responde.\n");
        free(a);
        return -1;
    }
    snprintf(path, sizeof(path), "%s/audio.json", dir);
    if (atlas_audio_save_snapshot(a, path) != 0)
        r = -1;
    printf("  [1/3] defaults, perfiles y volúmenes %s\n", r ? "FALLÓ" : "ok");

    snprintf(path, sizeof(path), "%s/alsa.state", dir);
    if (atlas_runl(NULL, 10000, "alsactl", "--file", path, "store", NULL) != 0)
        printf("  [2/3] controles ALSA: alsactl store falló (se omite)\n");
    else
        printf("  [2/3] controles ALSA ok\n");

    if (atlas_runl(&dump, 10000, "pw-dump", NULL) == 0 && dump) {
        snprintf(path, sizeof(path), "%s/pw-dump.json", dir);
        atlas_write_file_atomic(path, dump);
    }
    free(dump);
    printf("  [3/3] pw-dump de referencia ok\n");
    free(a);
    return r;
}

int ppm_profile_load(const char *name)
{
    char dir[PATH_MAX], path[PATH_MAX], log[2048];
    AtlasAudio *a = malloc(sizeof(*a));
    int n;

    if (!a)
        return -1;
    if (snap_dir(name, dir, sizeof(dir)) != 0) {
        free(a);
        return -1;
    }
    if (!atlas_dir_exists(dir)) {
        /* compatibilidad: instantáneas v0.1 en ~/.config/audio-profile/profiles/<nombre> */
        snprintf(dir, sizeof(dir), "%s/profiles/%s", atlas_config_dir(), name);
        if (!atlas_dir_exists(dir)) {
            fprintf(stderr, "No existe la instantánea '%s'.\n", name);
            free(a);
            return -1;
        }
    }
    printf("Cargando instantánea: %s\n", dir);

    snprintf(path, sizeof(path), "%s/alsa.state", dir);
    if (!atlas_file_exists(path))
        snprintf(path, sizeof(path), "%s/asound.state", dir);
    if (atlas_file_exists(path)) {
        int r = atlas_runl(NULL, 10000, "alsactl", "--file", path, "restore", NULL);
        printf("  [1/2] controles ALSA %s\n", r == 0 ? "restaurados" : "restaurados con avisos");
    } else {
        printf("  [1/2] sin estado ALSA\n");
    }

    snprintf(path, sizeof(path), "%s/audio.json", dir);
    if (atlas_audio_load_snapshot(a, path) == 0) {
        n = atlas_audio_restore(a, 1, log, sizeof(log));
        printf("  [2/2] %d ajuste(s) restaurados\n%s", n < 0 ? 0 : n, log);
    } else {
        printf("  [2/2] sin audio.json (instantánea antigua)\n");
    }
    free(a);
    return 0;
}
