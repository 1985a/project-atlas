#define _GNU_SOURCE
#include <ctype.h>
#include <dirent.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "atlas.h"
#include "atlas_audio.h"
#include "atlas_hwdb.h"

const char *atlas_match_name(AtlasMatchResult r)
{
    switch (r) {
    case ATLAS_MATCH_UNIQUE: return "unique";
    case ATLAS_MATCH_NONE: return "none";
    case ATLAS_MATCH_AMBIGUOUS: return "ambiguous";
    case ATLAS_MATCH_PARTIAL: return "partial";
    default: return "error";
    }
}

static void cpy(char *d, size_t n, const char *s)
{
    snprintf(d, n, "%s", s ? s : "");
}

static void trim(char *s)
{
    size_t l = strlen(s);
    while (l && isspace((unsigned char)s[l - 1]))
        s[--l] = '\0';
    while (*s && isspace((unsigned char)*s))
        memmove(s, s + 1, strlen(s));
}

static const char *root_or(const char *env, const char *def)
{
    const char *r = getenv(env);
    return (r && *r) ? r : def;
}

/* Lee la primera línea de un archivo en buf. 0 si OK. */
static int read_line(const char *path, char *buf, size_t n)
{
    char *s = atlas_read_file(path, 4096);
    if (!s) {
        buf[0] = '\0';
        return -1;
    }
    s[strcspn(s, "\n")] = '\0';
    cpy(buf, n, s);
    trim(buf);
    free(s);
    return buf[0] ? 0 : -1;
}

static void norm_id(char *id, size_t n)
{
    char tmp[32];
    size_t i;
    if (!id[0])
        return;
    if (strncasecmp(id, "0x", 2) != 0) {
        snprintf(tmp, sizeof(tmp), "0x%s", id);
        cpy(id, n, tmp);
    }
    for (i = 0; id[i]; i++)
        id[i] = (char)tolower((unsigned char)id[i]);
}

static void add_missing(AtlasHwDevice *d, const char *field)
{
    size_t l = strlen(d->missing);
    snprintf(d->missing + l, sizeof(d->missing) - l, "%s%s", l ? "," : "", field);
}

static void link_base(const char *path, char *out, size_t n)
{
    char buf[PATH_MAX];
    ssize_t l = readlink(path, buf, sizeof(buf) - 1);
    const char *b;
    out[0] = '\0';
    if (l <= 0)
        return;
    buf[l] = '\0';
    b = strrchr(buf, '/');
    cpy(out, n, b ? b + 1 : buf);
}

int atlas_hw_discover(AtlasHwDevice *out, int max)
{
    const char *sys = root_or("ATLAS_SYS_ROOT", "/sys");
    const char *proc = root_or("ATLAS_PROC_ROOT", "/proc");
    char *cards = NULL, path[PATH_MAX];
    int n = 0, card;

    snprintf(path, sizeof(path), "%s/asound/cards", proc);
    cards = atlas_read_file(path, 16384);

    for (card = 0; card < 32 && n < max; card++) {
        AtlasHwDevice *d = &out[n];
        char base[PATH_MAX], dev[PATH_MAX];
        int k;

        snprintf(base, sizeof(base), "%s/class/sound/card%d", sys, card);
        if (!atlas_dir_exists(base))
            continue;
        memset(d, 0, sizeof(*d));
        d->alsa_card = card;
        snprintf(d->sysfs, sizeof(d->sysfs), "%s", base);
        snprintf(dev, sizeof(dev), "%s/device", base);

        snprintf(path, sizeof(path), "%s/asound/card%d/id", proc, card);
        read_line(path, d->alsa_id, sizeof(d->alsa_id));

        /* nombre largo desde /proc/asound/cards: " 0 [Creative ]: HDA-Intel - HDA Creative\n   largo" */
        if (cards) {
            char needle[16], *p;
            snprintf(needle, sizeof(needle), "%2d [", card);
            p = strstr(cards, needle);
            if (p && (p = strchr(p, '\n'))) {
                char *e = strchr(p + 1, '\n');
                snprintf(d->longname, sizeof(d->longname), "%.*s",
                         (int)(e ? e - p - 1 : (long)strlen(p + 1)), p + 1);
                trim(d->longname);
            }
        }

        snprintf(path, sizeof(path), "%s/subsystem", dev);
        link_base(path, d->bus, sizeof(d->bus));
        snprintf(path, sizeof(path), "%s/driver", dev);
        link_base(path, d->driver, sizeof(d->driver));
        if (!d->bus[0])
            add_missing(d, "bus");

        if (!strcmp(d->bus, "pci")) {
            snprintf(path, sizeof(path), "%s/vendor", dev);
            if (read_line(path, d->vendor, sizeof(d->vendor)) != 0) add_missing(d, "vendor");
            snprintf(path, sizeof(path), "%s/device", dev);
            if (read_line(path, d->device, sizeof(d->device)) != 0) add_missing(d, "device");
            snprintf(path, sizeof(path), "%s/subsystem_vendor", dev);
            if (read_line(path, d->subsystem_vendor, sizeof(d->subsystem_vendor)) != 0)
                add_missing(d, "subsystem_vendor");
            snprintf(path, sizeof(path), "%s/subsystem_device", dev);
            if (read_line(path, d->subsystem_device, sizeof(d->subsystem_device)) != 0)
                add_missing(d, "subsystem_device");
        } else if (!strcmp(d->bus, "usb")) {
            /* device -> interfaz; los atributos están en el padre */
            snprintf(path, sizeof(path), "%s/../idVendor", dev);
            if (read_line(path, d->vendor, sizeof(d->vendor)) != 0) add_missing(d, "idVendor");
            snprintf(path, sizeof(path), "%s/../idProduct", dev);
            if (read_line(path, d->device, sizeof(d->device)) != 0) add_missing(d, "idProduct");
            snprintf(path, sizeof(path), "%s/../serial", dev);
            read_line(path, d->serial, sizeof(d->serial));
            snprintf(path, sizeof(path), "%s/../product", dev);
            read_line(path, d->product, sizeof(d->product));
            snprintf(path, sizeof(path), "%s/../manufacturer", dev);
            read_line(path, d->manufacturer, sizeof(d->manufacturer));
        }
        norm_id(d->vendor, sizeof(d->vendor));
        norm_id(d->device, sizeof(d->device));
        norm_id(d->subsystem_vendor, sizeof(d->subsystem_vendor));
        norm_id(d->subsystem_device, sizeof(d->subsystem_device));

        for (k = 0; k < 4 && !d->codec[0]; k++) {
            char *s, *c;
            snprintf(path, sizeof(path), "%s/asound/card%d/codec#%d", proc, card, k);
            if ((s = atlas_read_file(path, 2048)) && (c = strstr(s, "Codec:"))) {
                c[strcspn(c, "\n")] = '\0';
                cpy(d->codec, sizeof(d->codec), c + 6);
                trim(d->codec);
            }
            free(s);
        }

        if (d->subsystem_vendor[0])
            snprintf(d->stable_key, sizeof(d->stable_key), "%s:%s:%s:%s:%s", d->bus,
                     d->vendor + 2, d->device + 2, d->subsystem_vendor + 2, d->subsystem_device + 2);
        else
            snprintf(d->stable_key, sizeof(d->stable_key), "%s:%s:%s%s%s", d->bus[0] ? d->bus : "?",
                     d->vendor[0] ? d->vendor + 2 : "?", d->device[0] ? d->device + 2 : "?",
                     d->serial[0] ? ":" : "", d->serial);
        n++;
    }
    free(cards);
    return n;
}

/* ============================================================= carga */

static const char *jstr(JsonObject *o, const char *k)
{
    JsonNode *n;
    if (!o || !json_object_has_member(o, k))
        return "";
    n = json_object_get_member(o, k);
    if (JSON_NODE_TYPE(n) != JSON_NODE_VALUE || json_node_get_value_type(n) != G_TYPE_STRING)
        return "";
    return json_node_get_string(n);
}

static void join_array(JsonObject *o, const char *k, char *out, size_t n)
{
    JsonNode *an;
    JsonArray *a;
    guint i;
    out[0] = '\0';
    if (!o || !json_object_has_member(o, k))
        return;
    an = json_object_get_member(o, k);
    if (!JSON_NODE_HOLDS_ARRAY(an))
        return;
    a = json_node_get_array(an);
    for (i = 0; i < json_array_get_length(a); i++) {
        const char *s = json_array_get_string_element(a, i);
        size_t l = strlen(out);
        if (s)
            snprintf(out + l, n - l, "%s%s", l ? "," : "", s);
    }
}

static void parse_matchers(JsonObject *o, AtlasHwEntry *e)
{
    JsonNode *mn;
    JsonArray *a;
    guint i;

    if (!o || !json_object_has_member(o, "match"))
        return;
    mn = json_object_get_member(o, "match");
    if (!JSON_NODE_HOLDS_ARRAY(mn))
        return;
    a = json_node_get_array(mn);
    for (i = 0; i < json_array_get_length(a) && e->nmatch < ATLAS_HW_MAX_MATCH; i++) {
        JsonObject *mo = json_array_get_object_element(a, i);
        AtlasHwMatcher *m = &e->match[e->nmatch++];
        memset(m, 0, sizeof(*m));
        cpy(m->bus, sizeof(m->bus), jstr(mo, "bus"));
        cpy(m->vendor, sizeof(m->vendor), jstr(mo, "vendor"));
        cpy(m->device, sizeof(m->device), jstr(mo, "device"));
        cpy(m->subsystem_vendor, sizeof(m->subsystem_vendor), jstr(mo, "subsystem_vendor"));
        cpy(m->subsystem_device, sizeof(m->subsystem_device), jstr(mo, "subsystem_device"));
        cpy(m->serial, sizeof(m->serial), jstr(mo, "serial"));
        cpy(m->codec, sizeof(m->codec), jstr(mo, "codec"));
        norm_id(m->vendor, sizeof(m->vendor));
        norm_id(m->device, sizeof(m->device));
        norm_id(m->subsystem_vendor, sizeof(m->subsystem_vendor));
        norm_id(m->subsystem_device, sizeof(m->subsystem_device));
    }
}

int atlas_hwdb_parse_entry(const char *json, AtlasHwEntry *e)
{
    JsonParser *p = json_parser_new();
    JsonObject *o;
    JsonNode *root;

    if (!json_parser_load_from_data(p, json, -1, NULL)) {
        g_object_unref(p);
        return -1;
    }
    root = json_parser_get_root(p);
    if (!JSON_NODE_HOLDS_OBJECT(root)) {
        g_object_unref(p);
        return -1;
    }
    o = json_node_get_object(root);
    cpy(e->id, sizeof(e->id), jstr(o, "id"));
    cpy(e->name, sizeof(e->name), jstr(o, "name"));
    cpy(e->chip, sizeof(e->chip), jstr(o, "chip"));
    cpy(e->speakers, sizeof(e->speakers), jstr(o, "speaker_profile"));
    join_array(o, "aliases", e->aliases, sizeof(e->aliases));
    join_array(o, "quirks", e->quirks, sizeof(e->quirks));
    parse_matchers(o, e);
    g_object_unref(p);
    return e->id[0] ? 0 : -1;
}

/* Guarda un error de carga de la base de datos (máx. 16) para mostrarlo. */
static void db_error(AtlasHwDb *db, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
static void db_error(AtlasHwDb *db, const char *fmt, ...)
{
    va_list ap;

    if (db->nerrors >= 16)
        return;
    va_start(ap, fmt);
    vsnprintf(db->errors[db->nerrors++], sizeof(db->errors[0]), fmt, ap);
    va_end(ap);
}

static void load_dir(AtlasHwDb *db, const char *dir, int user)
{
    DIR *d = opendir(dir);
    struct dirent *de;

    if (!d)
        return;
    while ((de = readdir(d)) && db->n < ATLAS_HW_MAX) {
        char path[PATH_MAX], edir[PATH_MAX];
        char *json;
        AtlasHwEntry *e;
        int i, dup = -1;

        if (de->d_name[0] == '.' || !atlas_safe_name(de->d_name))
            continue;
        snprintf(edir, sizeof(edir), "%s/%s", dir, de->d_name);
        snprintf(path, sizeof(path), "%s/card.json", edir);

        /* overlay de coincidencias para una entrada versionada */
        if (user && !atlas_file_exists(path)) {
            snprintf(path, sizeof(path), "%s/match.json", edir);
            json = atlas_read_file(path, 65536);
            for (i = 0; json && i < db->n; i++)
                if (!strcmp(db->e[i].id, de->d_name)) {
                    AtlasHwEntry tmp;
                    char wrapped[70000];
                    int k;
                    memset(&tmp, 0, sizeof(tmp));
                    snprintf(wrapped, sizeof(wrapped), "{\"id\":\"%s\",\"match\":%s}", de->d_name, json);
                    if (atlas_hwdb_parse_entry(wrapped, &tmp) == 0)
                        for (k = 0; k < tmp.nmatch && db->e[i].nmatch < ATLAS_HW_MAX_MATCH; k++)
                            db->e[i].match[db->e[i].nmatch++] = tmp.match[k];
                    else
                        db_error(db, "match.json inválido: %s", path);
                }
            free(json);
            continue;
        }

        json = atlas_read_file(path, 1 << 20);
        if (!json)
            continue;
        e = &db->e[db->n];
        memset(e, 0, sizeof(*e));
        if (atlas_hwdb_parse_entry(json, e) != 0) {
            db_error(db, "card.json inválido: %s", path);
            free(json);
            continue;
        }
        free(json);
        cpy(e->dir, sizeof(e->dir), edir);
        e->user = user;
        for (i = 0; i < db->n; i++)
            if (!strcmp(db->e[i].id, e->id))
                dup = i;
        if (dup >= 0) {
            db_error(db, "id duplicado ignorado: %s", e->id);
            continue;
        }
        db->n++;
    }
    closedir(d);
}

int atlas_hwdb_load_dirs(AtlasHwDb *db, const char *data_hw, const char *user_hw)
{
    memset(db, 0, sizeof(*db));
    if (data_hw)
        load_dir(db, data_hw, 0);
    if (user_hw)
        load_dir(db, user_hw, 1);
    return db->n;
}

static void user_hw_dir(char *buf, size_t n)
{
    snprintf(buf, n, "%s/hardware", atlas_config_dir());
}

int atlas_hwdb_load(AtlasHwDb *db)
{
    char data[PATH_MAX], user[PATH_MAX];
    snprintf(data, sizeof(data), "%s/hardware", atlas_data_dir());
    user_hw_dir(user, sizeof(user));
    return atlas_hwdb_load_dirs(db, data, user);
}

/* ========================================================== matching */

static int eq_ci(const char *a, const char *b)
{
    return strcasecmp(a, b) == 0;
}

int atlas_matcher_score(const AtlasHwMatcher *m, const AtlasHwDevice *d)
{
    int s = 0;

    if (!m->vendor[0] && !m->codec[0])
        return 0;   /* matcher sin identidad útil */
#define F(field) \
    if (m->field[0]) { if (!eq_ci(m->field, d->field)) return 0; s++; }
    F(bus)
    F(vendor)
    F(device)
    F(subsystem_vendor)
    F(subsystem_device)
    F(serial)
#undef F
    if (m->codec[0]) {
        if (!d->codec[0] || !strcasestr(d->codec, m->codec))
            return 0;
        s++;
    }
    return s;
}

AtlasMatchResult atlas_hwdb_match(const AtlasHwDb *db, const AtlasHwDevice *d,
                                  const AtlasHwEntry **best, const AtlasHwEntry **second)
{
    int i, k, top = 0, top2 = 0;
    const AtlasHwEntry *b = NULL, *s2 = NULL, *partial = NULL;

    if (best)
        *best = NULL;
    if (second)
        *second = NULL;
    if (!db || !d)
        return ATLAS_MATCH_ERROR;

    for (i = 0; i < db->n; i++) {
        const AtlasHwEntry *e = &db->e[i];
        int sc = 0;
        for (k = 0; k < e->nmatch; k++) {
            int v = atlas_matcher_score(&e->match[k], d);
            if (v > sc)
                sc = v;
        }
        if (sc > top) {
            top2 = top;
            s2 = b;
            top = sc;
            b = e;
        } else if (sc > 0 && sc == top) {
            top2 = sc;
            s2 = e;
        } else if (sc > top2) {
            top2 = sc;
            s2 = e;
        }

        if (!sc && !partial) {
            /* mismo chip PCI/USB con otro subsistema, o chip declarado en el nombre */
            for (k = 0; k < e->nmatch && !partial; k++)
                if (e->match[k].vendor[0] && eq_ci(e->match[k].vendor, d->vendor) &&
                    e->match[k].device[0] && eq_ci(e->match[k].device, d->device))
                    partial = e;
            if (!partial && e->chip[0] && !e->nmatch &&
                (strcasestr(d->codec, e->chip) || strcasestr(d->longname, e->chip)))
                partial = e;
        }
    }

    if (top > 0) {
        if (best)
            *best = b;
        if (top2 == top && s2 && s2 != b) {
            if (second)
                *second = s2;
            return ATLAS_MATCH_AMBIGUOUS;
        }
        return ATLAS_MATCH_UNIQUE;
    }
    if (partial) {
        if (best)
            *best = partial;
        return ATLAS_MATCH_PARTIAL;
    }
    return ATLAS_MATCH_NONE;
}

const AtlasHwEntry *atlas_hwdb_find(const AtlasHwDb *db, const char *q)
{
    int i;
    if (!q)
        return NULL;
    for (i = 0; i < db->n; i++)
        if (eq_ci(db->e[i].id, q))
            return &db->e[i];
    for (i = 0; i < db->n; i++) {
        char list[256], *t, *save = NULL;
        cpy(list, sizeof(list), db->e[i].aliases);
        for (t = strtok_r(list, ",", &save); t; t = strtok_r(NULL, ",", &save))
            if (eq_ci(t, q))
                return &db->e[i];
    }
    return NULL;
}

int atlas_hw_has_quirk(const AtlasHwEntry *e, const char *q)
{
    char list[256], *t, *save = NULL;
    if (!e)
        return 0;
    cpy(list, sizeof(list), e->quirks);
    for (t = strtok_r(list, ",", &save); t; t = strtok_r(NULL, ",", &save))
        if (!strcmp(t, q))
            return 1;
    return 0;
}

const AtlasHwEntry *atlas_hwdb_for_card(const AtlasHwDb *db, int alsa_card)
{
    AtlasHwDevice devs[32];
    int n = atlas_hw_discover(devs, 32), i;
    for (i = 0; i < n; i++)
        if (devs[i].alsa_card == alsa_card) {
            const AtlasHwEntry *e;
            if (atlas_hwdb_match(db, &devs[i], &e, NULL) == ATLAS_MATCH_UNIQUE)
                return e;
        }
    return NULL;
}

/* ========================================================== registro */

static void slugify(const char *src, char *out, size_t n)
{
    size_t j = 0;
    int dash = 0;
    for (; *src && j + 1 < n && j < 40; src++) {
        unsigned char c = (unsigned char)*src;
        if (isalnum(c)) {
            out[j++] = (char)tolower(c);
            dash = 0;
        } else if (!dash && j > 0) {
            out[j++] = '-';
            dash = 1;
        }
    }
    while (j > 0 && out[j - 1] == '-')
        j--;
    out[j] = '\0';
    if (!out[0])
        snprintf(out, n, "device");
}

static void capabilities_json(JsonBuilder *b, const AtlasCard *c)
{
    int i, maxch = 0, cap = 0, digital = 0, hdmi = 0;
    for (i = 0; c && i < c->nprofiles; i++) {
        const char *p = c->profiles[i].name;
        if (strstr(p, "surround-71") && maxch < 8) maxch = 8;
        else if (strstr(p, "surround-51") && maxch < 6) maxch = 6;
        else if (strstr(p, "surround-40") && maxch < 4) maxch = 4;
        else if (strstr(p, "stereo") && strstr(p, "output") && maxch < 2) maxch = 2;
        if (strstr(p, "input:"))
            cap = 1;
        if (strstr(p, "iec958"))
            digital = 1;
        if (strstr(p, "hdmi"))
            hdmi = 1;
    }
    json_builder_set_member_name(b, "capabilities");
    json_builder_begin_object(b);
    atlas_json_kv_int(b, "max_output_channels", maxch);
    atlas_json_kv_bool(b, "playback_5_1", maxch >= 6);
    atlas_json_kv_bool(b, "playback_7_1", maxch >= 8);
    atlas_json_kv_bool(b, "capture", cap);
    atlas_json_kv_bool(b, "spdif", digital);
    atlas_json_kv_bool(b, "hdmi", hdmi);
    atlas_json_kv_str(b, "source", c ? "pipewire card profiles" : "unavailable");
    json_builder_end_object(b);
}

int atlas_hwdb_register(AtlasHwDb *db, const AtlasHwDevice *d, const char *user_hw,
                        const char *want_id, int force_new, int dry_run,
                        char *out_id, size_t n, char *msg, size_t msgn)
{
    const AtlasHwEntry *best, *second;
    AtlasMatchResult r = atlas_hwdb_match(db, d, &best, &second);
    char slug[64], dir[PATH_MAX], path[PATH_MAX], ts[64];
    AtlasAudio *a;
    JsonBuilder *b;
    JsonGenerator *g;
    JsonNode *root;
    gchar *json;
    int k;

    out_id[0] = '\0';
    if (r == ATLAS_MATCH_UNIQUE) {
        cpy(out_id, n, best->id);
        snprintf(msg, msgn, "ya registrada como '%s' (%s); no se crea un duplicado",
                 best->id, best->user ? "base del usuario" : "base versionada");
        return r;
    }
    if (r == ATLAS_MATCH_AMBIGUOUS) {
        snprintf(msg, msgn, "coincide con '%s' y '%s' a la vez: corrige los bloques \"match\" "
                 "o usa `audio-profile hardware-validate` para ver los duplicados",
                 best->id, second ? second->id : "?");
        return r;
    }
    if (r == ATLAS_MATCH_PARTIAL && !force_new) {
        snprintf(msg, msgn, "posible coincidencia con '%s' (mismo chip, distinto modelo/subsistema). "
                 "Si es la misma tarjeta: `audio-profile hardware-register %d --link %s`. "
                 "Si es otra: añade --new", best->id, d->alsa_card, best->id);
        return r;
    }
    if (!d->vendor[0] || !d->device[0]) {
        snprintf(msg, msgn, "faltan identificadores estables (%s); no se registra. "
                 "Obtén los datos con: udevadm info -a /sys/class/sound/card%d | head -40",
                 d->missing[0] ? d->missing : "vendor/device", d->alsa_card);
        return ATLAS_MATCH_ERROR;
    }

    if (want_id && *want_id) {
        if (!atlas_safe_name(want_id)) {
            snprintf(msg, msgn, "id no válido: usa letras, números, '.', '_' o '-'");
            return ATLAS_MATCH_ERROR;
        }
        cpy(slug, sizeof(slug), want_id);
    } else {
        slugify(d->codec[0] ? d->codec : (d->product[0] ? d->product : d->longname), slug, sizeof(slug));
    }
    for (k = 2; atlas_hwdb_find(db, slug) && k < 100; k++) {
        char base[48];
        if (want_id && *want_id) {
            snprintf(msg, msgn, "ya existe una entrada con id '%s'", want_id);
            return ATLAS_MATCH_ERROR;
        }
        slugify(d->codec[0] ? d->codec : d->longname, base, sizeof(base));
        snprintf(slug, sizeof(slug), "%s-%d", base, k);
    }
    cpy(out_id, n, slug);

    a = malloc(sizeof(*a));
    if (a && atlas_audio_query(a) != 0)
        a->ncards = 0;

    atlas_timestamp(ts, sizeof(ts), 0);
    b = json_builder_new();
    json_builder_begin_object(b);
    atlas_json_kv_str(b, "id", slug);
    atlas_json_kv_str(b, "name", d->product[0] ? d->product : (d->longname[0] ? d->longname : d->codec));
    atlas_json_kv_str(b, "chip", d->codec);
    atlas_json_kv_str(b, "manufacturer", d->manufacturer);
    atlas_json_kv_str(b, "bus", d->bus);
    atlas_json_kv_str(b, "driver", d->driver);
    atlas_json_kv_str(b, "stable_key", d->stable_key);
    atlas_json_kv_str(b, "source", "auto-registered");
    atlas_json_kv_str(b, "registered", ts);
    json_builder_set_member_name(b, "match");
    json_builder_begin_array(b);
    json_builder_begin_object(b);
    atlas_json_kv_str(b, "bus", d->bus);
    atlas_json_kv_str(b, "vendor", d->vendor);
    atlas_json_kv_str(b, "device", d->device);
    if (d->subsystem_vendor[0]) {
        atlas_json_kv_str(b, "subsystem_vendor", d->subsystem_vendor);
        atlas_json_kv_str(b, "subsystem_device", d->subsystem_device);
    }
    json_builder_end_object(b);
    json_builder_end_array(b);
    capabilities_json(b, a ? atlas_audio_card_by_alsa(a, d->alsa_card) : NULL);
    json_builder_set_member_name(b, "quirks");
    json_builder_begin_array(b);
    json_builder_end_array(b);
    json_builder_end_object(b);
    free(a);

    root = json_builder_get_root(b);
    g = json_generator_new();
    json_generator_set_pretty(g, TRUE);
    json_generator_set_root(g, root);
    json = json_generator_to_data(g, NULL);

    snprintf(dir, sizeof(dir), "%s/%s", user_hw, slug);
    snprintf(path, sizeof(path), "%s/card.json", dir);
    if (dry_run) {
        snprintf(msg, msgn, "simulación: se crearía %s", path);
    } else if (atlas_mkdirs(dir) != 0 || atlas_write_file_atomic(path, json) != 0) {
        snprintf(msg, msgn, "no se pudo escribir %s", path);
        r = ATLAS_MATCH_ERROR;
    } else {
        snprintf(msg, msgn, "registrada como '%s' en %s", slug, path);
        atlas_hwdb_load(db);
    }
    g_free(json);
    json_node_unref(root);
    g_object_unref(g);
    g_object_unref(b);
    return r;
}

/* Añade un matcher de este dispositivo a una entrada existente sin tocar
 * archivos versionados: escribe ~/.config/audio-profile/hardware/<id>/match.json */
static int link_device(AtlasHwDb *db, const AtlasHwDevice *d, const char *id, char *msg, size_t n)
{
    const AtlasHwEntry *e = atlas_hwdb_find(db, id);
    char dir[PATH_MAX], path[PATH_MAX];
    JsonBuilder *b;
    JsonGenerator *g;
    JsonNode *root;
    gchar *json;
    int r;

    if (!e) {
        snprintf(msg, n, "no existe la entrada '%s'", id);
        return -1;
    }
    user_hw_dir(dir, sizeof(dir));
    snprintf(dir + strlen(dir), sizeof(dir) - strlen(dir), "/%s", e->id);
    snprintf(path, sizeof(path), "%s/%s", dir, e->user ? "card.json" : "match.json");
    if (e->user) {
        snprintf(msg, n, "'%s' es una entrada del usuario: edita su bloque \"match\" en %s", e->id, path);
        return -1;
    }

    b = json_builder_new();
    json_builder_begin_array(b);
    json_builder_begin_object(b);
    atlas_json_kv_str(b, "bus", d->bus);
    atlas_json_kv_str(b, "vendor", d->vendor);
    atlas_json_kv_str(b, "device", d->device);
    if (d->subsystem_vendor[0]) {
        atlas_json_kv_str(b, "subsystem_vendor", d->subsystem_vendor);
        atlas_json_kv_str(b, "subsystem_device", d->subsystem_device);
    }
    json_builder_end_object(b);
    json_builder_end_array(b);
    root = json_builder_get_root(b);
    g = json_generator_new();
    json_generator_set_pretty(g, TRUE);
    json_generator_set_root(g, root);
    json = json_generator_to_data(g, NULL);
    r = (atlas_mkdirs(dir) == 0 && atlas_write_file_atomic(path, json) == 0) ? 0 : -1;
    if (r == 0)
        snprintf(msg, n, "tarjeta %d asociada a '%s' (%s)", d->alsa_card, e->id, path);
    else
        snprintf(msg, n, "no se pudo escribir %s", path);
    g_free(json);
    json_node_unref(root);
    g_object_unref(g);
    g_object_unref(b);
    return r;
}

/* ========================================================== comandos */

void atlas_hw_device_json(JsonBuilder *b, const AtlasHwDevice *d)
{
    json_builder_begin_object(b);
    atlas_json_kv_int(b, "alsa_card", d->alsa_card);
    atlas_json_kv_str(b, "alsa_id", d->alsa_id);
    atlas_json_kv_str(b, "longname", d->longname);
    atlas_json_kv_str(b, "bus", d->bus);
    atlas_json_kv_str(b, "driver", d->driver);
    atlas_json_kv_str(b, "vendor", d->vendor);
    atlas_json_kv_str(b, "device", d->device);
    atlas_json_kv_str(b, "subsystem_vendor", d->subsystem_vendor);
    atlas_json_kv_str(b, "subsystem_device", d->subsystem_device);
    atlas_json_kv_str(b, "serial", d->serial);
    atlas_json_kv_str(b, "product", d->product);
    atlas_json_kv_str(b, "codec", d->codec);
    atlas_json_kv_str(b, "stable_key", d->stable_key);
    atlas_json_kv_str(b, "missing", d->missing);
    json_builder_end_object(b);
}

int atlas_cmd_devices(void)
{
    AtlasHwDevice devs[32];
    AtlasHwDb *db = malloc(sizeof(*db));
    AtlasAudio *a = malloc(sizeof(*a));
    JsonBuilder *b = NULL;
    int n, i;

    if (!db || !a) {
        free(db);
        free(a);
        return 2;
    }
    n = atlas_hw_discover(devs, 32);
    atlas_hwdb_load(db);
    if (atlas_audio_query(a) != 0)
        a->ncards = 0;

    if (atlas_json_mode) {
        b = json_builder_new();
        json_builder_begin_array(b);
    } else {
        puts("Dispositivos de audio");
        puts("=====================");
        puts("(el número ALSA y los IDs de PipeWire son temporales; la identidad estable es la clave)");
    }

    for (i = 0; i < n; i++) {
        const AtlasHwDevice *d = &devs[i];
        const AtlasHwEntry *e, *e2;
        AtlasMatchResult r = atlas_hwdb_match(db, d, &e, &e2);
        const AtlasCard *c = atlas_audio_card_by_alsa(a, d->alsa_card);

        if (b) {
            json_builder_begin_object(b);
            json_builder_set_member_name(b, "hardware");
            atlas_hw_device_json(b, d);
            atlas_json_kv_str(b, "description", c ? c->description : d->longname);
            atlas_json_kv_str(b, "pipewire_card", c ? c->name : "");
            atlas_json_kv_str(b, "active_profile", c ? c->active_profile : "");
            atlas_json_kv_str(b, "match", atlas_match_name(r));
            atlas_json_kv_str(b, "db_id", e ? e->id : "");
            atlas_json_kv_str(b, "db_name", e ? e->name : "");
            atlas_json_kv_bool(b, "db_user", e ? e->user : 0);
            capabilities_json(b, c);
            json_builder_end_object(b);
            continue;
        }

        printf("\n[%d] %s\n", d->alsa_card, c ? c->description : d->longname);
        printf("    Identidad  : %s%s%s\n", d->stable_key, d->codec[0] ? " · códec " : "", d->codec);
        printf("    Driver     : %s · bus %s\n", d->driver[0] ? d->driver : "?", d->bus[0] ? d->bus : "?");
        if (c)
            printf("    PipeWire   : %s · perfil %s\n", c->name, c->active_profile);
        else
            puts("    PipeWire   : sin tarjeta en pipewire-pulse");
        switch (r) {
        case ATLAS_MATCH_UNIQUE:
            printf("    Hardware DB: %s (%s)%s\n", e->id, e->name, e->user ? " · usuario" : "");
            printf("                 audio-profile hardware-info %s\n", e->id);
            break;
        case ATLAS_MATCH_PARTIAL:
            printf("    Hardware DB: posible coincidencia con '%s' (confirmar)\n", e->id);
            printf("                 audio-profile hardware-register %d --link %s   (misma tarjeta)\n",
                   d->alsa_card, e->id);
            printf("                 audio-profile hardware-register %d --new       (modelo distinto)\n",
                   d->alsa_card);
            break;
        case ATLAS_MATCH_AMBIGUOUS:
            printf("    Hardware DB: AMBIGUA ('%s' y '%s') → audio-profile hardware-validate\n",
                   e->id, e2 ? e2->id : "?");
            break;
        default:
            puts("    Hardware DB: no registrada");
            printf("                 audio-profile hardware-register %d\n", d->alsa_card);
            break;
        }
        if (d->missing[0])
            printf("    Faltan     : %s → udevadm info -a /sys/class/sound/card%d\n", d->missing,
                   d->alsa_card);
    }

    if (b) {
        json_builder_end_array(b);
        atlas_json_print(b);
    } else if (db->nerrors) {
        puts("");
        for (i = 0; i < db->nerrors; i++)
            printf("⚠ %s\n", db->errors[i]);
    }
    free(db);
    free(a);
    return 0;
}

static void print_json_file(const char *title, const char *path)
{
    char *s = atlas_read_file(path, 1 << 20);
    if (!s)
        return;
    printf("\n%s (%s)\n", title, path);
    fputs(s, stdout);
    if (s[0] && s[strlen(s) - 1] != '\n')
        putchar('\n');
    free(s);
}

static void list_dir_files(const char *dir, const char *suffix, char *out, size_t n)
{
    DIR *d = opendir(dir);
    struct dirent *de;
    out[0] = '\0';
    if (!d)
        return;
    while ((de = readdir(d))) {
        size_t l = strlen(de->d_name), sl = strlen(suffix);
        if (l > sl && !strcmp(de->d_name + l - sl, suffix) && de->d_name[0] != '.') {
            size_t o = strlen(out);
            snprintf(out + o, n - o, "%s%.*s", o ? ", " : "", (int)(l - sl), de->d_name);
        }
    }
    closedir(d);
}

int atlas_cmd_hardware_info(const char *q)
{
    AtlasHwDb *db = malloc(sizeof(*db));
    AtlasHwDevice devs[32];
    const AtlasHwEntry *e;
    char path[PATH_MAX], list[512];
    int n, i, present = -1, k;

    if (!db)
        return 2;
    atlas_hwdb_load(db);
    n = atlas_hw_discover(devs, 32);

    e = atlas_hwdb_find(db, q);
    if (!e && q && isdigit((unsigned char)q[0])) {
        int card = atoi(q);
        for (i = 0; i < n; i++)
            if (devs[i].alsa_card == card)
                atlas_hwdb_match(db, &devs[i], &e, NULL);
    }
    if (!e) {
        fprintf(stderr, "No hay entrada '%s' en la Hardware DB.\n"
                        "Mira `audio-profile devices` para ver ids y tarjetas.\n", q ? q : "");
        free(db);
        return 1;
    }
    for (i = 0; i < n; i++) {
        const AtlasHwEntry *m;
        if (atlas_hwdb_match(db, &devs[i], &m, NULL) == ATLAS_MATCH_UNIQUE && m == e)
            present = devs[i].alsa_card;
    }

    if (atlas_json_mode) {
        JsonBuilder *b = json_builder_new();
        char *s;
        JsonParser *p = json_parser_new();
        json_builder_begin_object(b);
        atlas_json_kv_str(b, "id", e->id);
        atlas_json_kv_str(b, "name", e->name);
        atlas_json_kv_str(b, "chip", e->chip);
        atlas_json_kv_str(b, "dir", e->dir);
        atlas_json_kv_bool(b, "user", e->user);
        atlas_json_kv_int(b, "present_alsa_card", present);
        atlas_json_kv_str(b, "quirks", e->quirks);
        atlas_json_kv_str(b, "speakers", e->speakers);
        snprintf(path, sizeof(path), "%s/dsp", e->dir);
        list_dir_files(path, ".conf", list, sizeof(list));
        atlas_json_kv_str(b, "dsp_presets", list);
        snprintf(path, sizeof(path), "%s/known_bugs.json", e->dir);
        s = atlas_read_file(path, 1 << 20);
        json_builder_set_member_name(b, "known_bugs");
        if (s && json_parser_load_from_data(p, s, -1, NULL))
            json_builder_add_value(b, json_node_copy(json_parser_get_root(p)));
        else
            json_builder_add_null_value(b);
        free(s);
        g_object_unref(p);
        json_builder_end_object(b);
        atlas_json_print(b);
        free(db);
        return 0;
    }

    printf("Hardware DB · %s\n", e->id);
    printf("========================================\n");
    printf("Nombre     : %s\n", e->name);
    if (e->chip[0])
        printf("Chip       : %s\n", e->chip);
    if (e->speakers[0])
        printf("Altavoces  : %s\n", e->speakers);
    printf("Origen     : %s (%s)\n", e->user ? "usuario" : "versionada", e->dir);
    printf("Presente   : %s", present >= 0 ? "sí, tarjeta ALSA " : "no detectada ahora\n");
    if (present >= 0)
        printf("%d\n", present);
    printf("Coincide con:\n");
    for (k = 0; k < e->nmatch; k++) {
        const AtlasHwMatcher *m = &e->match[k];
        printf("  - %s %s:%s%s%s%s%s%s%s\n", m->bus, m->vendor, m->device,
               m->subsystem_vendor[0] ? " subsys " : "", m->subsystem_vendor,
               m->subsystem_vendor[0] ? ":" : "", m->subsystem_device,
               m->codec[0] ? " códec~" : "", m->codec);
    }
    if (!e->nmatch)
        puts("  (sin bloque \"match\": solo coincidencias parciales por chip)");
    if (e->quirks[0])
        printf("Quirks     : %s\n", e->quirks);
    snprintf(path, sizeof(path), "%s/dsp", e->dir);
    list_dir_files(path, ".conf", list, sizeof(list));
    if (list[0])
        printf("Presets DSP: %s\n", list);
    snprintf(path, sizeof(path), "%s/profiles", e->dir);
    list_dir_files(path, ".json", list, sizeof(list));
    if (list[0])
        printf("Perfiles HW: %s\n", list);
    snprintf(path, sizeof(path), "%s/known_bugs.json", e->dir);
    print_json_file("Errores conocidos", path);
    free(db);
    return 0;
}

int atlas_cmd_hardware_register(int argc, char **argv)
{
    AtlasHwDb *db = malloc(sizeof(*db));
    AtlasHwDevice devs[32];
    const char *want_id = NULL, *link = NULL, *target = NULL;
    int force_new = 0, dry = 0, n, i, rc = 0, done = 0;
    char user[PATH_MAX], id[64], msg[768];

    if (!db)
        return 2;
    for (i = 0; i < argc; i++) {
        if (!strcmp(argv[i], "--id") && i + 1 < argc)
            want_id = argv[++i];
        else if (!strcmp(argv[i], "--link") && i + 1 < argc)
            link = argv[++i];
        else if (!strcmp(argv[i], "--new"))
            force_new = 1;
        else if (!strcmp(argv[i], "--dry-run"))
            dry = 1;
        else
            target = argv[i];
    }
    if (!target) {
        fprintf(stderr, "Uso: audio-profile hardware-register <tarjeta-alsa|all> "
                        "[--id nombre] [--new] [--link id] [--dry-run]\n");
        free(db);
        return 2;
    }
    atlas_hwdb_load(db);
    user_hw_dir(user, sizeof(user));
    n = atlas_hw_discover(devs, 32);

    for (i = 0; i < n; i++) {
        AtlasMatchResult r;
        if (strcmp(target, "all") && devs[i].alsa_card != atoi(target))
            continue;
        done++;
        if (link) {
            rc |= link_device(db, &devs[i], link, msg, sizeof(msg)) != 0;
            printf("[%d] %s\n", devs[i].alsa_card, msg);
            continue;
        }
        r = atlas_hwdb_register(db, &devs[i], user, want_id, force_new, dry, id, sizeof(id),
                                msg, sizeof(msg));
        printf("[%d] %s: %s\n", devs[i].alsa_card,
               devs[i].codec[0] ? devs[i].codec : devs[i].longname, msg);
        if (r == ATLAS_MATCH_ERROR || r == ATLAS_MATCH_AMBIGUOUS ||
            (r == ATLAS_MATCH_PARTIAL && !force_new))
            rc = 1;
    }
    if (!done) {
        fprintf(stderr, "No existe la tarjeta ALSA '%s'. Mira `audio-profile devices`.\n", target);
        rc = 1;
    }
    free(db);
    return rc;
}

/* ======================================================== validación */

typedef struct {
    int errors, warnings;
    JsonBuilder *b;
} Val;

static void vreport(Val *v, int err, const char *where, const char *fmt, ...)
    __attribute__((format(printf, 4, 5)));
static void vreport(Val *v, int err, const char *where, const char *fmt, ...)
{
    char msg[512];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);
    if (err)
        v->errors++;
    else
        v->warnings++;
    if (v->b) {
        json_builder_begin_object(v->b);
        atlas_json_kv_str(v->b, "level", err ? "error" : "warning");
        atlas_json_kv_str(v->b, "where", where);
        atlas_json_kv_str(v->b, "message", msg);
        json_builder_end_object(v->b);
    } else {
        printf("  %s %s: %s\n", err ? "✗" : "⚠", where, msg);
    }
}

static void validate_json_file(Val *v, const char *path, int required)
{
    JsonParser *p;
    char *s = atlas_read_file(path, 1 << 20);
    GError *err = NULL;
    if (!s) {
        if (required)
            vreport(v, 1, path, "no existe%s", "");
        return;
    }
    p = json_parser_new();
    if (!json_parser_load_from_data(p, s, -1, &err)) {
        vreport(v, 1, path, "JSON inválido: %s", err ? err->message : "?");
        g_clear_error(&err);
    }
    g_object_unref(p);
    free(s);
}

static void validate_dsp_file(Val *v, const char *path)
{
    char *s = atlas_read_file(path, 1 << 16), *line, *save = NULL;
    int ln = 0;
    if (!s)
        return;
    for (line = strtok_r(s, "\n", &save); line; line = strtok_r(NULL, "\n", &save)) {
        char where[600];
        ln++;
        while (*line == ' ' || *line == '\t')
            line++;
        if (!*line || *line == '#')
            continue;
        snprintf(where, sizeof(where), "%s:%d", path, ln);
        if (!strchr(line, '='))
            vreport(v, 1, where, "línea sin '=': %s", line);
        else if (strchr(line, '`') || strchr(line, '$') || strchr(line, ';'))
            vreport(v, 1, where, "caracteres no permitidos: %s", line);
    }
    free(s);
}

static void validate_entry_dir(Val *v, const AtlasHwEntry *e)
{
    char path[PATH_MAX], base[128];
    const char *slash = strrchr(e->dir, '/');
    DIR *d;
    struct dirent *de;

    cpy(base, sizeof(base), slash ? slash + 1 : e->dir);
    snprintf(path, sizeof(path), "%s/card.json", e->dir);
    validate_json_file(v, path, 1);
    if (strcmp(base, e->id))
        vreport(v, 1, path, "el id no coincide con el nombre de la carpeta (%s)", base);
    if (!atlas_safe_name(e->id))
        vreport(v, 1, path, "id con caracteres no permitidos: %s", e->id);
    if (!e->name[0])
        vreport(v, 0, path, "falta \"name\"%s", "");
    if (!e->nmatch)
        vreport(v, 0, path, "sin bloque \"match\": no se puede asociar de forma estable%s", "");

    snprintf(path, sizeof(path), "%s/known_bugs.json", e->dir);
    validate_json_file(v, path, 0);

    snprintf(path, sizeof(path), "%s/profiles", e->dir);
    if ((d = opendir(path))) {
        while ((de = readdir(d))) {
            char f[PATH_MAX];
            char *s;
            if (de->d_name[0] == '.' || !strstr(de->d_name, ".json"))
                continue;
            snprintf(f, sizeof(f), "%s/%s", path, de->d_name);
            validate_json_file(v, f, 1);
            /* referencias alsa_state */
            if ((s = atlas_read_file(f, 1 << 16))) {
                char *k = strstr(s, "\"alsa_state\"");
                if (k && (k = strchr(k + 12, '"'))) {
                    char *e2 = strchr(k + 1, '"');
                    if (e2) {
                        char ref[PATH_MAX], rel[512];
                        snprintf(rel, sizeof(rel), "%.*s", (int)(e2 - k - 1), k + 1);
                        snprintf(ref, sizeof(ref), "%s/%s", atlas_data_dir(), rel);
                        if (!atlas_file_exists(ref))
                            vreport(v, 1, f, "alsa_state apunta a un archivo inexistente: %s", rel);
                    }
                }
                free(s);
            }
        }
        closedir(d);
    }

    snprintf(path, sizeof(path), "%s/dsp", e->dir);
    if ((d = opendir(path))) {
        while ((de = readdir(d))) {
            char f[PATH_MAX];
            if (de->d_name[0] == '.' || !strstr(de->d_name, ".conf"))
                continue;
            snprintf(f, sizeof(f), "%s/%s", path, de->d_name);
            validate_dsp_file(v, f);
        }
        closedir(d);
    }
}

static void validate_profiles(Val *v)
{
    char dirs[2][PATH_MAX];
    int i;
    snprintf(dirs[0], sizeof(dirs[0]), "%s/profiles", atlas_data_dir());
    snprintf(dirs[1], sizeof(dirs[1]), "%s/profiles", atlas_config_dir());
    for (i = 0; i < 2; i++) {
        DIR *d = opendir(dirs[i]);
        struct dirent *de;
        if (!d)
            continue;
        while ((de = readdir(d))) {
            char f[PATH_MAX], *s, *k;
            if (de->d_name[0] == '.')
                continue;
            snprintf(f, sizeof(f), "%s/%s/profile.conf", dirs[i], de->d_name);
            if (!(s = atlas_read_file(f, 1 << 16)))
                continue;
            if ((k = strstr(s, "\nalsa_preset=")) || (!strncmp(s, "alsa_preset=", 12) && (k = s - 1))) {
                char name[128], p1[PATH_MAX], p2[PATH_MAX];
                k += 13;
                snprintf(name, sizeof(name), "%.*s", (int)strcspn(k, "\r\n"), k);
                snprintf(p1, sizeof(p1), "%s/presets/alsa/%s", atlas_data_dir(), name);
                snprintf(p2, sizeof(p2), "%s/presets/alsa/%s", atlas_config_dir(), name);
                if (name[0] && !atlas_file_exists(p1) && !atlas_file_exists(p2))
                    vreport(v, 1, f, "alsa_preset inexistente: %s", name);
            }
            free(s);
        }
        closedir(d);
    }
}

int atlas_cmd_hardware_validate(void)
{
    AtlasHwDb *db = malloc(sizeof(*db));
    Val v = { 0, 0, NULL };
    int i, j, a, b;

    if (!db)
        return 2;
    atlas_hwdb_load(db);
    if (atlas_json_mode) {
        v.b = json_builder_new();
        json_builder_begin_object(v.b);
        json_builder_set_member_name(v.b, "issues");
        json_builder_begin_array(v.b);
    } else {
        printf("Validando Hardware DB (%d entradas) y perfiles...\n", db->n);
    }

    for (i = 0; i < db->nerrors; i++)
        vreport(&v, 1, "hardware", "%s", db->errors[i]);
    for (i = 0; i < db->n; i++)
        validate_entry_dir(&v, &db->e[i]);

    /* matchers idénticos en entradas distintas = duplicado */
    for (i = 0; i < db->n; i++)
        for (j = i + 1; j < db->n; j++)
            for (a = 0; a < db->e[i].nmatch; a++)
                for (b = 0; b < db->e[j].nmatch; b++)
                    if (!memcmp(&db->e[i].match[a], &db->e[j].match[b], sizeof(AtlasHwMatcher))) {
                        char where[200];
                        snprintf(where, sizeof(where), "%s / %s", db->e[i].id, db->e[j].id);
                        vreport(&v, 1, where, "misma regla de coincidencia en dos entradas%s", "");
                    }

    validate_profiles(&v);

    if (v.b) {
        json_builder_end_array(v.b);
        atlas_json_kv_int(v.b, "errors", v.errors);
        atlas_json_kv_int(v.b, "warnings", v.warnings);
        atlas_json_kv_int(v.b, "entries", db->n);
        json_builder_end_object(v.b);
        atlas_json_print(v.b);
    } else {
        printf("\n%s: %d error(es), %d advertencia(s)\n", v.errors ? "FALLO" : "OK", v.errors, v.warnings);
    }
    free(db);
    return v.errors ? 1 : 0;
}

int atlas_cmd_hardware_scan(void)
{
    return atlas_cmd_devices();
}
