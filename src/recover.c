#include <stddef.h>

#include "atlas_recover.h"
#include "recover.h"

/* Compatibilidad v0.1: recuperación escalonada en atlas_recover.c */
void recover_run(void)
{
    atlas_cmd_recover(0, NULL);
}
