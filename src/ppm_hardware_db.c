#include "atlas_hwdb.h"
#include "ppm_hardware_db.h"

/* Compatibilidad v0.1: la implementación está en atlas_hwdb.c
 * (id validado, sin interpolación en shell). */
int ppm_hardware_info(const char *id)
{
    return atlas_cmd_hardware_info(id);
}
