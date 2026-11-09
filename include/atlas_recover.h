#ifndef ATLAS_RECOVER_H
#define ATLAS_RECOVER_H

/*
 * Recuperación escalonada y verificable (BUG-001).
 *
 *   0  evidencia   informe antes de tocar nada
 *   1  card-cycle  perfil de la tarjeta off -> anterior (reabre el PCM;
 *                  no reinicia servicios ni tumba el portal)
 *   2  pulse       reinicia pipewire-pulse
 *   3  pipewire    reinicia pipewire + pipewire-pulse + wireplumber y
 *                  repara los portales que dependan de PipeWire
 *
 * Tras cada nivel se repite la sonda funcional. Solo se sube de nivel si
 * la sonda sigue fallando. Tras 2/3 se restauran defaults, perfiles y
 * volúmenes por nombre.
 */

#include <stddef.h>

typedef struct {
    int dry_run;
    int force;            /* actuar aunque la sonda diga OK */
    int max_level;        /* 1..3 */
    int start_level;      /* empezar como mínimo en este nivel (pruebas) */
    int confirm;          /* pedir confirmación auditiva (tty) */
    const char *trigger;  /* "manual", "resume", "gui" */
    const char *snapshot; /* instantánea a restaurar (opcional) */
} AtlasRecoverOpts;

/* Devuelve 0 si al final el audio está sano. */
int atlas_recover(const AtlasRecoverOpts *o, char *report_path, size_t n);

/* Escribe un informe de evidencia (servicios, pactl, wpctl, /proc, journal). */
int atlas_report_capture(const char *path, const char *title, int append);

int atlas_cmd_recover(int argc, char **argv);
int atlas_cmd_report(void);
int atlas_cmd_history(int argc, char **argv);

/* Repara portales (reset-failed + restart) si están caídos. */
int atlas_repair_portals(char *log, size_t n);

#endif
