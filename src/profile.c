#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "atlas.h"
#include "profile.h"

#define SYS_PROFILE_DIR "/usr/local/share/audio-profile/profiles"

static void trim_newline(char *s)
{
    if (!s)
        return;

    s[strcspn(s, "\r\n")] = '\0';
}

static void copy_field(char *dst, size_t dst_size, const char *src)
{
    size_t n;

    if (!dst || dst_size == 0)
        return;

    if (!src)
        src = "";

    n = strlen(src);

    if (n >= dst_size)
        n = dst_size - 1;

    memcpy(dst, src, n);
    dst[n] = '\0';
}

static int parse_bool(const char *v)
{
    if (!v)
        return 0;

    if (!strcmp(v, "1")) return 1;
    if (!strcmp(v, "yes")) return 1;
    if (!strcmp(v, "true")) return 1;
    if (!strcmp(v, "on")) return 1;

    return 0;
}

static void profile_defaults(const char *id, AudioProfile *p)
{
    memset(p, 0, sizeof(*p));

    copy_field(p->id, sizeof(p->id), id);

    p->enabled = 1;
    p->rate = 0;
    p->quantum = 0;
    p->restore_volume = 0;
    p->restore_defaults = 0;
    p->restore_links = 0;
    p->sink_volume = -1;
    p->sink_muted = -1;
    p->source_volume = -1;
    p->source_muted = -1;
    p->priority = 0;
}

static void profile_set_key(AudioProfile *p, const char *key, const char *value)
{
    if (!p || !key || !value)
        return;

    if (!strcmp(key, "name"))
        copy_field(p->name, sizeof(p->name), value);
    else if (!strcmp(key, "description"))
        copy_field(p->description, sizeof(p->description), value);
    else if (!strcmp(key, "enabled"))
        p->enabled = parse_bool(value);
    else if (!strcmp(key, "rate"))
        p->rate = atoi(value);
    else if (!strcmp(key, "quantum"))
        p->quantum = atoi(value);
    else if (!strcmp(key, "default_sink"))
        copy_field(p->default_sink, sizeof(p->default_sink), value);
    else if (!strcmp(key, "default_source"))
        copy_field(p->default_source, sizeof(p->default_source), value);
    else if (!strcmp(key, "alsa_preset"))
        copy_field(p->alsa_preset, sizeof(p->alsa_preset), value);
    else if (!strcmp(key, "restore_volume"))
        p->restore_volume = parse_bool(value);
    else if (!strcmp(key, "restore_defaults"))
        p->restore_defaults = parse_bool(value);
    else if (!strcmp(key, "restore_links"))
        p->restore_links = parse_bool(value);
    else if (!strcmp(key, "sink_volume"))
        p->sink_volume = atoi(value);
    else if (!strcmp(key, "sink_muted"))
        p->sink_muted = parse_bool(value);
    else if (!strcmp(key, "source_volume"))
        p->source_volume = atoi(value);
    else if (!strcmp(key, "source_muted"))
        p->source_muted = parse_bool(value);
    else if (!strcmp(key, "wireplumber_profile"))
        copy_field(p->wireplumber_profile, sizeof(p->wireplumber_profile), value);
    else if (!strcmp(key, "easyeffects_preset"))
        copy_field(p->easyeffects_preset, sizeof(p->easyeffects_preset), value);
    else if (!strcmp(key, "priority"))
        p->priority = atoi(value);
    else if (!strcmp(key, "dsp"))
        copy_field(p->dsp_preset, sizeof(p->dsp_preset), value);
    else if (!strcmp(key, "category"))
        copy_field(p->category, sizeof(p->category), value);
    else if (!strcmp(key, "icon"))
        copy_field(p->icon, sizeof(p->icon), value);
}

int profile_load_from_file(const char *id, const char *path, AudioProfile *p)
{
    FILE *f;
    char line[512];

    if (!id || !path || !p)
        return -1;

    profile_defaults(id, p);

    f = fopen(path, "r");
    if (!f)
        return -1;

    while (fgets(line, sizeof(line), f)) {
        char *eq;

        trim_newline(line);

        if (line[0] == '\0' || line[0] == '#')
            continue;

        eq = strchr(line, '=');
        if (!eq)
            continue;

        *eq = '\0';
        profile_set_key(p, line, eq + 1);
    }

    fclose(f);
    return 0;
}

static int
try_profile_path(const char *id,
                 const char *base,
                 AudioProfile *p)
{
    char path[512];
    int n;

    if (!id || !base || !p)
        return -1;

    n = snprintf(path,
                 sizeof(path),
                 "%s/%s/profile.conf",
                 base,
                 id);

    if (n < 0)
        return -1;

    if ((size_t)n >= sizeof(path))
        return -1;

    return profile_load_from_file(id, path, p);
}

static int id_is_safe(const char *id)
{
    const char *c;
    if (!id || !*id || strlen(id) > 63 || !strcmp(id, ".") || !strcmp(id, ".."))
        return 0;
    for (c = id; *c; c++)
        if (!((*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z') ||
              (*c >= '0' && *c <= '9') || *c == '-' || *c == '_' || *c == '.'))
            return 0;
    return 1;
}

int profile_load(const char *id, AudioProfile *p)
{
    char base[512];

    if (!id || !p || !id_is_safe(id))
        return -1;

    /* 1. perfiles del usuario (sustituyen a los de fábrica con el mismo id) */
    snprintf(base, sizeof(base), "%s/profiles", atlas_config_dir());
    if (try_profile_path(id, base, p) == 0) {
        copy_field(p->source, sizeof(p->source), "user");
        return 0;
    }

    /* 2. datos de Atlas: árbol de desarrollo o <prefix>/share/audio-profile */
    snprintf(base, sizeof(base), "%s/profiles", atlas_data_dir());
    if (try_profile_path(id, base, p) == 0) {
        copy_field(p->source, sizeof(p->source), "system");
        return 0;
    }

    if (try_profile_path(id, SYS_PROFILE_DIR, p) == 0) {
        copy_field(p->source, sizeof(p->source), "system");
        return 0;
    }
    return -1;
}

static void collect_ids(const char *dir, char ids[][64], int *n, int max)
{
    DIR *d = opendir(dir);
    struct dirent *de;
    int i, dup;

    if (!d)
        return;
    while ((de = readdir(d)) && *n < max) {
        char path[768];
        if (de->d_name[0] == '.' || !id_is_safe(de->d_name))
            continue;
        snprintf(path, sizeof(path), "%s/%s/profile.conf", dir, de->d_name);
        if (access(path, R_OK) != 0)
            continue;
        for (dup = 0, i = 0; i < *n; i++)
            if (!strcmp(ids[i], de->d_name))
                dup = 1;
        if (!dup)
            snprintf(ids[(*n)++], 64, "%s", de->d_name);
    }
    closedir(d);
}

int profile_ids(char ids[][64], int max)
{
    char base[512];
    int n = 0, i, j;
    int prio[128];

    snprintf(base, sizeof(base), "%s/profiles", atlas_config_dir());
    collect_ids(base, ids, &n, max);
    snprintf(base, sizeof(base), "%s/profiles", atlas_data_dir());
    collect_ids(base, ids, &n, max);

    for (i = 0; i < n && i < 128; i++) {
        AudioProfile p;
        prio[i] = profile_load(ids[i], &p) == 0 ? p.priority : -1;
    }
    /* orden por prioridad descendente y luego alfabético */
    for (i = 1; i < n && i < 128; i++)
        for (j = i; j > 0 && (prio[j] > prio[j - 1] ||
                              (prio[j] == prio[j - 1] && strcmp(ids[j], ids[j - 1]) < 0)); j--) {
            char t[64];
            int tp = prio[j];
            memcpy(t, ids[j], 64);
            memcpy(ids[j], ids[j - 1], 64);
            memcpy(ids[j - 1], t, 64);
            prio[j] = prio[j - 1];
            prio[j - 1] = tp;
        }
    return n;
}

void profile_list(void)
{
    char ids[128][64];
    int n = profile_ids(ids, 128), i;

    if (atlas_json_mode) {
        JsonBuilder *b = json_builder_new();
        json_builder_begin_array(b);
        for (i = 0; i < n; i++) {
            AudioProfile p;
            if (profile_load(ids[i], &p) != 0)
                continue;
            json_builder_begin_object(b);
            atlas_json_kv_str(b, "id", p.id);
            atlas_json_kv_str(b, "name", p.name);
            atlas_json_kv_str(b, "description", p.description);
            atlas_json_kv_str(b, "category", p.category);
            atlas_json_kv_str(b, "icon", p.icon);
            atlas_json_kv_str(b, "source", p.source);
            atlas_json_kv_str(b, "dsp", p.dsp_preset);
            atlas_json_kv_bool(b, "enabled", p.enabled);
            atlas_json_kv_int(b, "rate", p.rate);
            atlas_json_kv_int(b, "quantum", p.quantum);
            atlas_json_kv_int(b, "priority", p.priority);
            atlas_json_kv_double(b, "latency_ms",
                                 p.quantum > 0 ? 1000.0 * p.quantum / (p.rate > 0 ? p.rate : 48000) : 0);
            json_builder_end_object(b);
        }
        json_builder_end_array(b);
        atlas_json_print(b);
        return;
    }

    for (i = 0; i < n; i++) {
        AudioProfile p;
        if (profile_load(ids[i], &p) != 0)
            continue;
        printf("%-12s %-26s %s%s\n", p.id, p.name[0] ? p.name : "-",
               p.description, !strcmp(p.source, "user") ? "  [usuario]" : "");
    }
}

void profile_print(const AudioProfile *p)
{
    printf("Perfil: %s\n", p->id);
    printf("Nombre: %s\n", p->name[0] ? p->name : "(sin nombre)");
    printf("Descripción: %s\n", p->description[0] ? p->description : "(sin descripción)");
    printf("Activo: %s\n", p->enabled ? "yes" : "no");
    printf("Rate: %d\n", p->rate);
    printf("Quantum: %d\n", p->quantum);

    if (p->default_sink[0])
        printf("Salida: %s\n", p->default_sink);

    if (p->default_source[0])
        printf("Entrada: %s\n", p->default_source);

    if (p->alsa_preset[0])
        printf("Preset ALSA: %s\n", p->alsa_preset);

    printf("Restaurar volumen: %s\n", p->restore_volume ? "yes" : "no");
    printf("Restaurar salida/entrada: %s\n", p->restore_defaults ? "yes" : "no");
    printf("Restaurar enlaces: %s\n", p->restore_links ? "yes" : "no");

    if (p->sink_volume >= 0)
        printf("Sink volume: %d%%\n", p->sink_volume);

    if (p->sink_muted >= 0)
        printf("Sink muted: %s\n",
            p->sink_muted ? "yes" : "no");

    if (p->source_volume >= 0)
        printf("Source volume: %d%%\n", p->source_volume);

    if (p->source_muted >= 0)
        printf("Source muted: %s\n",
            p->source_muted ? "yes" : "no");

    if (p->wireplumber_profile[0])
        printf("WirePlumber profile: %s\n", p->wireplumber_profile);

    if (p->easyeffects_preset[0])
        printf("Preset EasyEffects: %s\n", p->easyeffects_preset);

    if (p->dsp_preset[0])
        printf("DSP: %s\n", p->dsp_preset);

    if (p->category[0])
        printf("Categoría: %s\n", p->category);

    printf("Prioridad: %d\n", p->priority);
}
