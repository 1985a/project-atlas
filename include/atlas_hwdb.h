#ifndef ATLAS_HWDB_H
#define ATLAS_HWDB_H

/*
 * Hardware Database + Hardware Matching Engine (RFC-0003, EPIC-04).
 *
 * Identidad estable = bus + IDs de fabricante/producto (+ subsistema PCI o
 * serie USB) + códec. Nunca se usan IDs de nodos de PipeWire ni el número
 * de tarjeta ALSA (cambian entre arranques).
 *
 * Fuentes de la base de datos:
 *   <data>/hardware/<id>/card.json           versionada (solo lectura)
 *   ~/.config/audio-profile/hardware/<id>/   del usuario (registro)
 *   ~/.config/audio-profile/hardware/<id>/match.json
 *        coincidencias extra para una entrada versionada (--link)
 */

#include <json-glib/json-glib.h>

#define ATLAS_HW_MAX 32
#define ATLAS_HW_MAX_MATCH 8

typedef struct {
    int alsa_card;
    char alsa_id[64];
    char longname[256];
    char bus[16];            /* pci / usb / platform / ... */
    char driver[64];
    char vendor[16];         /* 0x1102 */
    char device[16];         /* 0x0012 */
    char subsystem_vendor[16];
    char subsystem_device[16];
    char serial[128];
    char product[160];
    char manufacturer[160];
    char codec[160];
    char sysfs[256];
    char stable_key[256];
    char missing[256];       /* campos no legibles, separados por coma */
} AtlasHwDevice;

typedef struct {
    char bus[16];
    char vendor[16];
    char device[16];
    char subsystem_vendor[16];
    char subsystem_device[16];
    char serial[128];
    char codec[160];
} AtlasHwMatcher;

typedef struct {
    char id[64];
    char name[160];
    char chip[64];
    char dir[512];
    int user;                /* 1 = carpeta del usuario */
    int nmatch;
    AtlasHwMatcher match[ATLAS_HW_MAX_MATCH];
    char aliases[256];
    char quirks[256];        /* separados por coma */
    char speakers[128];
} AtlasHwEntry;

typedef struct {
    int n;
    AtlasHwEntry e[ATLAS_HW_MAX];
    int nerrors;
    char errors[16][256];
} AtlasHwDb;

typedef enum {
    ATLAS_MATCH_UNIQUE = 0,
    ATLAS_MATCH_NONE = 1,
    ATLAS_MATCH_AMBIGUOUS = 2,
    ATLAS_MATCH_PARTIAL = 3,
    ATLAS_MATCH_ERROR = -1
} AtlasMatchResult;

const char *atlas_match_name(AtlasMatchResult r);

int atlas_hw_discover(AtlasHwDevice *out, int max);    /* respeta ATLAS_SYS_ROOT / ATLAS_PROC_ROOT */
int atlas_hwdb_load(AtlasHwDb *db);
int atlas_hwdb_load_dirs(AtlasHwDb *db, const char *data_hw_dir, const char *user_hw_dir);
int atlas_hwdb_parse_entry(const char *json, AtlasHwEntry *e);

/* Devuelve el resultado; best/second apuntan a las entradas candidatas. */
AtlasMatchResult atlas_hwdb_match(const AtlasHwDb *db, const AtlasHwDevice *d,
                                  const AtlasHwEntry **best, const AtlasHwEntry **second);
int atlas_matcher_score(const AtlasHwMatcher *m, const AtlasHwDevice *d);

const AtlasHwEntry *atlas_hwdb_find(const AtlasHwDb *db, const char *id_or_alias);
int atlas_hw_has_quirk(const AtlasHwEntry *e, const char *quirk);

/* Entrada de la tarjeta ALSA n (o NULL). */
const AtlasHwEntry *atlas_hwdb_for_card(const AtlasHwDb *db, int alsa_card);

/* Registro: crea la entrada en user_hw_dir si no existe una compatible.
 * Devuelve el AtlasMatchResult previo; out_id recibe el id usado. */
int atlas_hwdb_register(AtlasHwDb *db, const AtlasHwDevice *d, const char *user_hw_dir,
                        const char *want_id, int force_new, int dry_run,
                        char *out_id, size_t n, char *msg, size_t msgn);

/* Comandos */
int atlas_cmd_devices(void);
int atlas_cmd_hardware_info(const char *id);
int atlas_cmd_hardware_register(int argc, char **argv);
int atlas_cmd_hardware_validate(void);
int atlas_cmd_hardware_scan(void);

void atlas_hw_device_json(JsonBuilder *b, const AtlasHwDevice *d);

#endif
