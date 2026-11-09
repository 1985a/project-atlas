#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "atlas.h"
#include "ppm_alsa_preset.h"

/*
 * Presets ALSA completos (alsactl). Reglas de seguridad (informe 26-10-2026):
 *  - los presets versionados de <data>/presets/alsa NUNCA se sobrescriben;
 *  - antes de restaurar se guarda el estado actual de la tarjeta en
 *    ~/.local/state/audio-profile/alsa-backups/;
 *  - la tarjeta se toma del propio archivo (state.<ID> {), no se supone.
 */

static int preset_path(const char *preset, char *path, size_t n)
{
    if (!atlas_safe_name(preset))
        return -1;
    /* preset del usuario primero, luego versionado */
    snprintf(path, n, "%s/presets/alsa/%s", atlas_config_dir(), preset);
    if (atlas_file_exists(path))
        return 0;
    snprintf(path, n, "%s/presets/alsa/%s", atlas_data_dir(), preset);
    return atlas_file_exists(path) ? 0 : -1;
}

static int card_from_state(const char *path, char *card, size_t n)
{
    char *s = atlas_read_file(path, 1 << 20), *p;
    int r = -1;

    if (!s)
        return -1;
    p = strstr(s, "state.");
    if (p) {
        size_t l;
        p += 6;
        l = strcspn(p, " {\t\n");
        if (l > 0 && l < n) {
            memcpy(card, p, l);
            card[l] = '\0';
            r = atlas_safe_name(card) ? 0 : -1;
        }
    }
    free(s);
    return r;
}

static int backup_card(const char *card, char *out, size_t n)
{
    char dir[PATH_MAX], ts[64];

    snprintf(dir, sizeof(dir), "%s/alsa-backups", atlas_state_dir());
    if (atlas_mkdirs(dir) != 0)
        return -1;
    atlas_timestamp(ts, sizeof(ts), 1);
    snprintf(out, n, "%s/%s-%s.state", dir, card, ts);
    return atlas_runl(NULL, 8000, "alsactl", "--file", out, "store", card, NULL);
}

int ppm_alsa_apply_preset(const char *preset)
{
    char path[PATH_MAX], card[64], backup[PATH_MAX];
    int r;

    if (preset_path(preset, path, sizeof(path)) != 0) {
        fprintf(stderr, "No existe o no es válido el preset ALSA: %s\n", preset ? preset : "(null)");
        return -1;
    }
    if (card_from_state(path, card, sizeof(card)) != 0) {
        fprintf(stderr, "El preset %s no indica la tarjeta (state.<ID>).\n", path);
        return -1;
    }
    if (backup_card(card, backup, sizeof(backup)) == 0)
        printf("[*] %-28s %s\n", "Copia ALSA previa", backup);
    else
        fprintf(stderr, "Advertencia: no pude guardar copia de %s antes de restaurar.\n", card);

    printf("[*] %-28s %s → %s\n", "Restaurando preset ALSA", preset, card);
    r = atlas_runl(NULL, 10000, "alsactl", "--file", path, "restore", card, NULL);
    if (r != 0) {
        /* CA0132-0001: algunos controles (Wedge Angle) se rechazan; el resto sí se aplica */
        fprintf(stderr, "Advertencia: alsactl restore devolvió %d (puede ser un control "
                        "rechazado por el driver; el resto se aplicó).\n", r);
        return r == 99 ? 0 : -1;
    }
    return 0;
}

int ppm_alsa_apply_ca0132_optimal(void)
{
    return ppm_alsa_apply_preset("ca0132-optimal.state");
}

/* Guarda el estado actual como preset del USUARIO (nunca en la carpeta versionada). */
int ppm_alsa_save_preset(const char *name, const char *card, int force)
{
    char dir[PATH_MAX], path[PATH_MAX], file[96];

    if (!atlas_safe_name(name) || !atlas_safe_name(card)) {
        fprintf(stderr, "Nombre o tarjeta no válidos.\n");
        return -1;
    }
    snprintf(file, sizeof(file), "%s%s", name, strstr(name, ".state") ? "" : ".state");
    snprintf(path, sizeof(path), "%s/presets/alsa/%s", atlas_data_dir(), file);
    if (atlas_file_exists(path))
        printf("Nota: existe un preset versionado %s; el tuyo tendrá prioridad solo para tu usuario.\n", file);
    snprintf(dir, sizeof(dir), "%s/presets/alsa", atlas_config_dir());
    atlas_mkdirs(dir);
    snprintf(path, sizeof(path), "%s/%s", dir, file);
    if (atlas_file_exists(path) && !force) {
        fprintf(stderr, "Ya existe %s. Usa --force para reemplazarlo.\n", path);
        return -1;
    }
    if (atlas_runl(NULL, 8000, "alsactl", "--file", path, "store", card, NULL) != 0) {
        fprintf(stderr, "alsactl store falló.\n");
        return -1;
    }
    printf("Preset ALSA guardado: %s\n", path);
    return 0;
}
