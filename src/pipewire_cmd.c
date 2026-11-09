#include <stdio.h>

#include "atlas.h"
#include "pipewire_cmd.h"

/*
 * Fija clock.force-rate / clock.force-quantum en la metadata "settings".
 * 0 significa "no forzar" (PipeWire decide). Sin shell: argv directo.
 */
static int set_setting(const char *key, int value)
{
    char v[16];
    int r;

    snprintf(v, sizeof(v), "%d", value);
    r = atlas_runl(NULL, 4000, "pw-metadata", "-n", "settings", "0", key, v, NULL);
    if (r != 0)
        fprintf(stderr, "Advertencia: pw-metadata %s=%s falló (%d)\n", key, v, r);
    return r;
}

int pipewire_apply_rate_quantum(int rate, int quantum)
{
    int r = 0;

    if (rate < 0 || quantum < 0 || rate > 768000 || quantum > 8192)
        return -1;
    r |= set_setting("clock.force-rate", rate);
    r |= set_setting("clock.force-quantum", quantum);
    return r ? -1 : 0;
}
