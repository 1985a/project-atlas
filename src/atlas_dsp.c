#define _GNU_SOURCE
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "atlas.h"
#include "atlas_audio.h"
#include "atlas_dsp.h"
#include "atlas_hwdb.h"
#include "atlas_output.h"

/* ============================================================ parsers */

static void trim(char *s)
{
    size_t l = strlen(s);
    char *p = s;
    while (l && isspace((unsigned char)s[l - 1]))
        s[--l] = '\0';
    while (*p && isspace((unsigned char)*p))
        p++;
    if (p != s)
        memmove(s, p, strlen(p) + 1);
}

int atlas_dsp_parse_contents(const char *text, AtlasDspState *s)
{
    char *copy, *line, *save = NULL;
    AtlasDspControl *cur = NULL;

    s->n = 0;
    if (!text)
        return -1;
    copy = strdup(text);
    if (!copy)
        return -1;

    for (line = strtok_r(copy, "\n", &save); line; line = strtok_r(NULL, "\n", &save)) {
        char *p;
        if (!strncmp(line, "numid=", 6)) {
            cur = NULL;
            if (!strstr(line, "iface=MIXER") || s->n >= ATLAS_DSP_MAX_CTL)
                continue;
            p = strstr(line, "name='");
            if (!p)
                continue;
            cur = &s->c[s->n++];
            memset(cur, 0, sizeof(*cur));
            p += 6;
            snprintf(cur->name, sizeof(cur->name), "%.*s", (int)strcspn(p, "'"), p);
            continue;
        }
        if (!cur)
            continue;
        while (*line == ' ')
            line++;
        if (!strncmp(line, "; type=", 7)) {
            char *f;
            snprintf(cur->type, sizeof(cur->type), "%.*s", (int)strcspn(line + 7, ","), line + 7);
            if ((f = strstr(line, "access=")))
                snprintf(cur->access, sizeof(cur->access), "%.*s", (int)strcspn(f + 7, ","), f + 7);
            if ((f = strstr(line, "values=")))
                cur->count = atoi(f + 7);
            if ((f = strstr(line, "min=")))
                cur->min = atol(f + 4);
            if ((f = strstr(line, "max=")))
                cur->max = atol(f + 4);
        } else if (!strncmp(line, "; Item #", 8)) {
            char *q = strchr(line, '\'');
            if (q && cur->nitems < 32) {
                q++;
                snprintf(cur->items[cur->nitems++], sizeof(cur->items[0]), "%.*s",
                         (int)strcspn(q, "'"), q);
            }
        } else if (!strncmp(line, ": values=", 9)) {
            snprintf(cur->values, sizeof(cur->values), "%s", line + 9);
            trim(cur->values);
        }
    }
    free(copy);
    return s->n;
}

int atlas_dsp_parse_preset(const char *text, char names[][128], char values[][128], int max)
{
    char *copy, *line, *save = NULL;
    int n = 0;

    if (!text)
        return 0;
    copy = strdup(text);
    for (line = strtok_r(copy, "\n", &save); line && n < max; line = strtok_r(NULL, "\n", &save)) {
        char *eq;
        trim(line);
        if (!*line || *line == '#')
            continue;
        /* el nombre puede contener '=' nunca; el valor tampoco */
        eq = strrchr(line, '=');
        if (!eq)
            continue;
        *eq = '\0';
        snprintf(names[n], 128, "%s", line);
        snprintf(values[n], 128, "%s", eq + 1);
        trim(names[n]);
        trim(values[n]);
        /* sin caracteres de shell (no se usa shell, pero son señal de archivo corrupto) */
        if (!names[n][0] || strpbrk(names[n], "`$;\"") || strpbrk(values[n], "`$;\"'"))
            continue;
        n++;
    }
    free(copy);
    return n;
}

/* ========================================================== objetivo */

int atlas_dsp_target(int *card, char *hw_id, size_t n, char *hw_dir, size_t dn)
{
    AtlasAudio *a = malloc(sizeof(*a));
    AtlasHwDb *db = malloc(sizeof(*db));
    const AtlasNode *sink;
    const AtlasHwEntry *e;
    int rc = -1;

    *card = -1;
    hw_id[0] = hw_dir[0] = '\0';
    if (!a || !db)
        goto out;
    if (atlas_audio_query(a) != 0 || !(sink = atlas_audio_default_sink(a)) || sink->alsa_card < 0) {
        /* sin sink ALSA por defecto: primera tarjeta registrada presente */
        AtlasHwDevice devs[32];
        int i, nd = atlas_hw_discover(devs, 32);
        atlas_hwdb_load(db);
        for (i = 0; i < nd; i++)
            if (atlas_hwdb_match(db, &devs[i], &e, NULL) == ATLAS_MATCH_UNIQUE) {
                *card = devs[i].alsa_card;
                break;
            }
        if (*card < 0)
            goto out;
    } else {
        *card = sink->alsa_card;
        atlas_hwdb_load(db);
    }
    e = atlas_hwdb_for_card(db, *card);
    if (e) {
        snprintf(hw_id, n, "%s", e->id);
        snprintf(hw_dir, dn, "%s", e->dir);
    }
    rc = 0;
out:
    free(a);
    free(db);
    return rc;
}

static int find_preset(const char *hw_id, const char *hw_dir, const char *preset, char *out, size_t n)
{
    snprintf(out, n, "%s/hardware/%s/dsp/%s.conf", atlas_config_dir(), hw_id, preset);
    if (atlas_file_exists(out))
        return 0;
    snprintf(out, n, "%s/dsp/%s.conf", hw_dir, preset);
    return atlas_file_exists(out) ? 0 : -1;
}

int atlas_dsp_apply_file(int card, const char *path, int *applied, int *failed)
{
    static char names[ATLAS_DSP_MAX_CTL][128], values[ATLAS_DSP_MAX_CTL][128];
    char *text = atlas_read_file(path, 1 << 16);
    char cards[16], namearg[160];
    int n, i;

    *applied = *failed = 0;
    if (!text)
        return -1;
    n = atlas_dsp_parse_preset(text, names, values, ATLAS_DSP_MAX_CTL);
    free(text);
    snprintf(cards, sizeof(cards), "%d", card);
    for (i = 0; i < n; i++) {
        snprintf(namearg, sizeof(namearg), "name=%s", names[i]);
        if (atlas_runl(NULL, 3000, "amixer", "-c", cards, "-q", "cset", namearg, values[i], NULL) == 0)
            (*applied)++;
        else {
            (*failed)++;
            if (!atlas_json_mode)
                fprintf(stderr, "  ⚠ control no aplicado: %s=%s\n", names[i], values[i]);
        }
    }
    return *failed ? 1 : 0;
}

/* Escribe o sustituye «name=value» en un archivo de capa (atómico). */
int atlas_dsp_store_value(const char *path, const char *name, const char *value)
{
    char *text = atlas_read_file(path, 1 << 16), *out, *line, *save = NULL;
    size_t cap, len = 0, nl = strlen(name);
    int done = 0, rc;

    cap = (text ? strlen(text) : 0) + strlen(name) + strlen(value) + 64;
    out = malloc(cap);
    if (!out) {
        free(text);
        return -1;
    }
    out[0] = '\0';
    for (line = text ? strtok_r(text, "\n", &save) : NULL; line; line = strtok_r(NULL, "\n", &save)) {
        if (!done && !strncmp(line, name, nl) && line[nl] == '=') {
            len += snprintf(out + len, cap - len, "%s=%s\n", name, value);
            done = 1;
        } else {
            len += snprintf(out + len, cap - len, "%s\n", line);
        }
    }
    if (!done)
        len += snprintf(out + len, cap - len, "%s=%s\n", name, value);
    rc = atlas_write_file_atomic(path, out);
    free(out);
    free(text);
    return rc;
}

/*
 * Efectos que lleva un perfil. Por defecto (profiles.effects=personal) son
 * SIEMPRE los del usuario («Mi sonido», importados de su asound.state): los
 * perfiles solo cambian rate/quantum. Con profiles.effects=profile cada
 * perfil aplica su propio preset de efectos.
 */
const char *atlas_effects_preset(const char *profile_preset, const char *hw_id)
{
    char path[PATH_MAX];

    if (!profile_preset || !strcmp(profile_preset, "base"))
        return profile_preset;
    if (!strcmp(atlas_conf_get("profiles.effects"), "profile"))
        return profile_preset;
    snprintf(path, sizeof(path), "%s/hardware/%s/dsp/personal.conf", atlas_config_dir(), hw_id);
    return atlas_file_exists(path) ? "personal" : profile_preset;
}

/* Archivos que componen un preset, en orden de aplicación:
 * base (altavoces) -> volumes (base de volumen) -> [headphones] -> preset. */
int atlas_dsp_layer_files(const char *preset, const char *hw_id, const char *hw_dir,
                          char files[][PATH_MAX], int max, int *missing_preset)
{
    const char *layers[4];
    char hp_preset[80];
    int nl = 0, i, n = 0, hp = atlas_output_mode() == ATLAS_OUT_HEADPHONES;

    if (missing_preset)
        *missing_preset = 0;
    layers[nl++] = "base";
    if (strcmp(preset, "base"))
        layers[nl++] = "volumes";
    /* auriculares: Output Select=Headphone y variante .hp del preset si existe */
    if (hp && strcmp(preset, "base"))
        layers[nl++] = "headphones";
    if (strcmp(preset, "base") && strcmp(preset, "volumes")) {
        snprintf(hp_preset, sizeof(hp_preset), "%s.hp", preset);
        layers[nl++] = (hp && find_preset(hw_id, hw_dir, hp_preset, files[0], PATH_MAX) == 0)
                           ? hp_preset : preset;
    }
    for (i = 0; i < nl && n < max; i++) {
        if (find_preset(hw_id, hw_dir, layers[i], files[n], PATH_MAX) != 0) {
            if (i == nl - 1 && strcmp(layers[i], "base") && strcmp(layers[i], "volumes") && missing_preset)
                *missing_preset = 1;
            continue;
        }
        n++;
    }
    return n;
}

static int dsp_apply_layers(const char *preset, const char *requested)
{
    char hw_id[64], hw_dir[PATH_MAX], files[4][PATH_MAX];
    int card, ok, bad, tot_ok = 0, tot_bad = 0, nf, i, missing, have_volumes = 0;
    int hp = atlas_output_mode() == ATLAS_OUT_HEADPHONES;

    if (!preset || !atlas_safe_name(preset)) {
        fprintf(stderr, "Preset DSP no válido.\n");
        return -1;
    }
    if (atlas_dsp_target(&card, hw_id, sizeof(hw_id), hw_dir, sizeof(hw_dir)) != 0) {
        fprintf(stderr, "No hay tarjeta ALSA para aplicar el DSP.\n");
        return -1;
    }
    if (!hw_id[0]) {
        /* tarjeta sin entrada en la DB: no es un error del perfil */
        printf("[*] %-28s %s\n", "DSP", "tarjeta sin presets en la Hardware DB (omitido)");
        return 0;
    }
    if (requested)
        preset = atlas_effects_preset(preset, hw_id);
    nf = atlas_dsp_layer_files(preset, hw_id, hw_dir, files, 4, &missing);
    if (missing)
        printf("[*] %-28s %s no existe para %s (omitido)\n", "DSP", preset, hw_id);
    for (i = 0; i < nf; i++) {
        if (strstr(files[i], "/volumes.conf"))
            have_volumes = 1;
        atlas_dsp_apply_file(card, files[i], &ok, &bad);
        tot_ok += ok;
        tot_bad += bad;
    }
    printf("[*] %-28s %s/%s · %d controles%s%s%s%s%s\n", "DSP", hw_id, preset, tot_ok,
           have_volumes ? " · con tu base de volumen" : "", hp ? " · auriculares" : "",
           requested && strcmp(requested, preset) && strcmp(requested, "volumes") ? " · tus efectos (no los de " : "",
           requested && strcmp(requested, preset) && strcmp(requested, "volumes") ? requested : "",
           requested && strcmp(requested, preset) && strcmp(requested, "volumes") ? ")" : "");
    if (tot_bad)
        printf("[*] %-28s algunos controles fallaron\n", "DSP");
    /* un control rechazado no invalida el perfil: el resto se aplicó */
    return tot_ok > 0 || tot_bad == 0 ? 0 : -1;
}

/* Perfiles, salida y base: respeta profiles.effects. */
int ppm_dsp_apply(const char *preset)
{
    return dsp_apply_layers(preset, preset);
}

/* `dsp apply <preset>` explícito: aplica justo ese preset. */
int atlas_dsp_apply_exact(const char *preset)
{
    return dsp_apply_layers(preset, NULL);
}

/* =========================================================== comando */

static int load_state(AtlasDspState *s)
{
    char hw_id[64], hw_dir[PATH_MAX], cards[16];
    char *out = NULL;

    memset(s, 0, sizeof(*s));
    if (atlas_dsp_target(&s->card, hw_id, sizeof(hw_id), hw_dir, sizeof(hw_dir)) != 0)
        return -1;
    snprintf(s->hw_id, sizeof(s->hw_id), "%s", hw_id);
    snprintf(cards, sizeof(cards), "%d", s->card);
    if (atlas_runl(&out, 5000, "amixer", "-c", cards, "contents", NULL) != 0) {
        free(out);
        return -1;
    }
    atlas_dsp_parse_contents(out, s);
    free(out);
    return 0;
}

static void list_presets(JsonBuilder *b, const char *hw_id, const char *hw_dir)
{
    char dirs[2][PATH_MAX];
    int i;
    snprintf(dirs[0], sizeof(dirs[0]), "%s/dsp", hw_dir);
    snprintf(dirs[1], sizeof(dirs[1]), "%s/hardware/%s/dsp", atlas_config_dir(), hw_id);
    for (i = 0; i < 2; i++) {
        char *out = NULL, *line, *save = NULL;
        if (!atlas_dir_exists(dirs[i]))
            continue;
        if (atlas_runl(&out, 2000, "ls", "-1", dirs[i], NULL) == 0)
            for (line = strtok_r(out, "\n", &save); line; line = strtok_r(NULL, "\n", &save)) {
                size_t l = strlen(line);
                if (l > 5 && !strcmp(line + l - 5, ".conf")) {
                    line[l - 5] = '\0';
                    if (b) {
                        json_builder_begin_object(b);
                        atlas_json_kv_str(b, "name", line);
                        atlas_json_kv_bool(b, "user", i == 1);
                        json_builder_end_object(b);
                    } else {
                        printf("  %s%s\n", line, i ? " (usuario)" : "");
                    }
                }
            }
        free(out);
    }
}

int atlas_cmd_dsp(int argc, char **argv)
{
    static AtlasDspState s;
    const char *sub = argc > 0 ? argv[0] : "get";
    char hw_id[64], hw_dir[PATH_MAX], cards[16], namearg[160];
    int i, card;

    if (!strcmp(sub, "apply")) {
        if (argc < 2) {
            fprintf(stderr, "Uso: audio-profile dsp apply <preset>\n");
            return 2;
        }
        return atlas_dsp_apply_exact(argv[1]) == 0 ? 0 : 1;
    }

    if (!strcmp(sub, "set")) {
        if (argc < 3) {
            fprintf(stderr, "Uso: audio-profile dsp set \"<control>\" <valor>\n");
            return 2;
        }
        if (strpbrk(argv[1], "`$;") || strpbrk(argv[2], "`$;")) {
            fprintf(stderr, "Valor no válido.\n");
            return 2;
        }
        if (atlas_dsp_target(&card, hw_id, sizeof(hw_id), hw_dir, sizeof(hw_dir)) != 0)
            return 1;
        snprintf(cards, sizeof(cards), "%d", card);
        snprintf(namearg, sizeof(namearg), "name=%s", argv[1]);
        if (atlas_runl(NULL, 3000, "amixer", "-c", cards, "-q", "cset", namearg, argv[2], NULL) != 0)
            return 1;
        /* Con efectos personales, lo que ajustas (GUI o CLI) pasa a ser tu
         * sonido: si no, el daemon lo devolvería al valor guardado. */
        if (hw_id[0] && !strcmp(atlas_effects_preset("x", hw_id), "personal") &&
            (!strncmp(argv[1], "FX: ", 4) || !strncmp(argv[1], "EQ Band", 7) ||
             !strcmp(argv[1], "Enable OutFX Playback Switch")) && !strstr(argv[1], "Capture")) {
            char path[PATH_MAX];
            snprintf(path, sizeof(path), "%s/hardware/%s/dsp/personal.conf", atlas_config_dir(), hw_id);
            if (atlas_dsp_store_value(path, argv[1], argv[2]) != 0)
                fprintf(stderr, "Aviso: no pude guardar %s en %s\n", argv[1], path);
        }
        return 0;
    }

    if (!strcmp(sub, "save")) {
        /* guarda los controles de efectos actuales como preset del usuario */
        char dir[PATH_MAX], path[PATH_MAX], *buf;
        size_t len = 0, cap = 65536;
        if (argc < 2 || !atlas_safe_name(argv[1])) {
            fprintf(stderr, "Uso: audio-profile dsp save <nombre>\n");
            return 2;
        }
        if (load_state(&s) != 0 || !s.hw_id[0]) {
            fprintf(stderr, "La tarjeta no está en la Hardware DB; registra primero con hardware-register.\n");
            return 1;
        }
        buf = malloc(cap);
        len += snprintf(buf + len, cap - len, "# Preset DSP guardado por audio-profile (%s)\n", s.hw_id);
        for (i = 0; i < s.n; i++) {
            const AtlasDspControl *c = &s.c[i];
            /* solo efectos de reproducción: volumen y altavoces pertenecen a la base */
            if ((strncmp(c->name, "FX:", 3) && strncmp(c->name, "EQ Band", 7)) ||
                strstr(c->name, "Capture"))
                continue;
            if (!strstr(c->access, "w"))
                continue;
            if (!strcmp(c->type, "ENUMERATED") && c->nitems) {
                int idx = atoi(c->values);
                len += snprintf(buf + len, cap - len, "%s=%s\n", c->name,
                                idx >= 0 && idx < c->nitems ? c->items[idx] : c->values);
            } else {
                len += snprintf(buf + len, cap - len, "%s=%s\n", c->name, c->values);
            }
        }
        snprintf(dir, sizeof(dir), "%s/hardware/%s/dsp", atlas_config_dir(), s.hw_id);
        snprintf(path, sizeof(path), "%s/%s.conf", dir, argv[1]);
        i = (atlas_mkdirs(dir) == 0 && atlas_write_file_atomic(path, buf) == 0) ? 0 : 1;
        free(buf);
        printf(i == 0 ? "Preset guardado: %s\n" : "No se pudo guardar %s\n", path);
        return i;
    }

    if (!strcmp(sub, "presets")) {
        if (atlas_dsp_target(&card, hw_id, sizeof(hw_id), hw_dir, sizeof(hw_dir)) != 0 || !hw_id[0]) {
            fprintf(stderr, "La tarjeta por defecto no está en la Hardware DB.\n");
            return 1;
        }
        if (atlas_json_mode) {
            JsonBuilder *b = json_builder_new();
            json_builder_begin_array(b);
            list_presets(b, hw_id, hw_dir);
            json_builder_end_array(b);
            atlas_json_print(b);
        } else {
            printf("Presets DSP de %s:\n", hw_id);
            list_presets(NULL, hw_id, hw_dir);
        }
        return 0;
    }

    /* get */
    if (load_state(&s) != 0) {
        fprintf(stderr, "No pude leer los controles ALSA.\n");
        return 1;
    }
    if (atlas_json_mode) {
        JsonBuilder *b = json_builder_new();
        int k;
        json_builder_begin_object(b);
        atlas_json_kv_int(b, "card", s.card);
        atlas_json_kv_str(b, "hw_id", s.hw_id);
        json_builder_set_member_name(b, "controls");
        json_builder_begin_array(b);
        for (i = 0; i < s.n; i++) {
            const AtlasDspControl *c = &s.c[i];
            json_builder_begin_object(b);
            atlas_json_kv_str(b, "name", c->name);
            atlas_json_kv_str(b, "type", c->type);
            atlas_json_kv_bool(b, "writable", strchr(c->access, 'w') != NULL);
            atlas_json_kv_int(b, "count", c->count);
            atlas_json_kv_int(b, "min", c->min);
            atlas_json_kv_int(b, "max", c->max);
            atlas_json_kv_str(b, "values", c->values);
            json_builder_set_member_name(b, "items");
            json_builder_begin_array(b);
            for (k = 0; k < c->nitems; k++)
                json_builder_add_string_value(b, c->items[k]);
            json_builder_end_array(b);
            json_builder_end_object(b);
        }
        json_builder_end_array(b);
        json_builder_end_object(b);
        atlas_json_print(b);
    } else {
        printf("Controles de la tarjeta %d (%s)\n", s.card, s.hw_id[0] ? s.hw_id : "sin DB");
        for (i = 0; i < s.n; i++)
            printf("  %-42s %-10s %s\n", s.c[i].name, s.c[i].type, s.c[i].values);
    }
    return 0;
}
