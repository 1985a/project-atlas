#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "atlas.h"
#include "backup.h"

/* Rutas relativas a $HOME que se guardan si existen. */
static const char *const BACKUP_PATHS[] = {
    ".config/audio-profile",
    ".local/state/audio-profile",
    ".config/asound.state",
    ".asoundrc",
    ".config/pipewire",
    ".config/wireplumber",
    ".config/easyeffects",
    NULL
};

#define BACKUP_MAX_ARGS 32
#define TAR_TIMEOUT_MS  60000

int backup_create(void)
{
    const char *home = getenv("HOME");
    const char *argv[BACKUP_MAX_ARGS];
    char dir[PATH_MAX], out[PATH_MAX], cards_state[PATH_MAX], ts[32], probe[PATH_MAX];
    int argc = 0, i, rc;

    if (!home || !*home) {
        fprintf(stderr, "HOME no está definido.\n");
        return -1;
    }
    atlas_timestamp(ts, sizeof(ts), 1);
    snprintf(dir, sizeof(dir), "%s/backups", atlas_state_dir());
    if (atlas_mkdirs(dir) != 0) {
        fprintf(stderr, "No pude crear %s\n", dir);
        return -1;
    }
    snprintf(out, sizeof(out), "%s/atlas-backup-%s.tar.gz", dir, ts);

    /* estado actual de las tarjetas (alsactl), junto al archivo final */
    snprintf(cards_state, sizeof(cards_state), "%s/alsa-actual-%s.state", dir, ts);
    if (atlas_runl(NULL, 10000, "alsactl", "--file", cards_state, "store", NULL) != 0)
        cards_state[0] = '\0';

    argv[argc++] = "tar";
    argv[argc++] = "-czf";
    argv[argc++] = out;
    /* las copias viven dentro del estado de Atlas: no meter copias en copias */
    argv[argc++] = "--exclude=.local/state/audio-profile/backups";
    argv[argc++] = "-C";
    argv[argc++] = home;
    for (i = 0; BACKUP_PATHS[i] && argc < BACKUP_MAX_ARGS - 4; i++) {
        snprintf(probe, sizeof(probe), "%s/%s", home, BACKUP_PATHS[i]);
        if (access(probe, R_OK) == 0)
            argv[argc++] = BACKUP_PATHS[i];
    }
    if (cards_state[0]) {
        argv[argc++] = "-C";
        argv[argc++] = dir;
        argv[argc++] = strrchr(cards_state, '/') + 1;
    }
    argv[argc] = NULL;

    rc = atlas_run(argv, NULL, TAR_TIMEOUT_MS, ATLAS_RUN_SHOW_STDERR);
    if (cards_state[0])
        unlink(cards_state);
    if (rc != 0) {
        fprintf(stderr, "tar falló (código %d); no se creó la copia.\n", rc);
        unlink(out);
        return -1;
    }
    printf("Copia creada: %s\n", out);
    return 0;
}
