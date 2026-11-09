#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#include "atlas.h"
#include "atlas_health.h"
#include "status.h"

/*
 * Compatibilidad con la API v0.1. La lógica real está en atlas_health.c y
 * atlas_doctor.c (salud por niveles con sonda de hardware).
 */

void state_save_profile(const char *profile)
{
    char path[PATH_MAX], line[160];

    if (!profile || !atlas_safe_name(profile))
        return;
    atlas_mkdirs(atlas_state_dir());
    snprintf(path, sizeof(path), "%s/current-profile", atlas_state_dir());
    snprintf(line, sizeof(line), "%s\n", profile);
    atlas_write_file_atomic(path, line);
}

void status_show(void)
{
    atlas_cmd_status(0);
}

void doctor_show(void)
{
    atlas_cmd_doctor();
}
