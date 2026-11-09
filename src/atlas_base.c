#define _GNU_SOURCE
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>

#include "atlas.h"
#include "atlas_audio.h"
#include "atlas_auto.h"
#include "atlas_base.h"
#include "atlas_health.h"
#include "atlas_dsp.h"
#include "atlas_hwdb.h"

/* ============================================================== parser */

static void unquote(char *s)
{
    size_t l = strlen(s);
    if (l >= 2 && ((s[0] == '\'' && s[l - 1] == '\'') || (s[0] == '"' && s[l - 1] == '"'))) {
        memmove(s, s + 1, l - 2);
        s[l - 2] = '\0';
    }
}

static void norm_value(char *v)
{
    unquote(v);
    if (!strcmp(v, "true"))
        strcpy(v, "on");
    else if (!strcmp(v, "false"))
        strcpy(v, "off");
}

int atlas_state_parse(const char *text, const char *card, AtlasStateCard *out)
{
    char *copy, *line, *save = NULL;
    int depth = 0, in_card = 0, card_depth = 0, ctl_depth = -1, comment_depth = -1, found = 0;
    AtlasStateCtl *cur = NULL;
    char want[80];

    memset(out, 0, sizeof(*out));
    if (!text || !card)
        return -1;
    snprintf(out->card, sizeof(out->card), "%s", card);
    snprintf(want, sizeof(want), "state.%s", card);
    copy = strdup(text);
    if (!copy)
        return -1;

    for (line = strtok_r(copy, "\n", &save); line; line = strtok_r(NULL, "\n", &save)) {
        char *p = line, *e;
        int opens, closes;
        while (isspace((unsigned char)*p))
            p++;
        e = p + strlen(p);
        while (e > p && isspace((unsigned char)e[-1]))
            *--e = '\0';
        if (!*p)
            continue;
        opens = strchr(p, '{') != NULL;
        closes = !strcmp(p, "}");

        if (opens) {
            if (depth == 0 && !strncmp(p, want, strlen(want)) &&
                (p[strlen(want)] == ' ' || p[strlen(want)] == '{')) {
                in_card = 1;
                found = 1;
                card_depth = depth + 1;
            } else if (in_card && depth == card_depth && !strncmp(p, "control.", 8)) {
                ctl_depth = depth + 1;
                cur = out->n < ATLAS_STATE_MAX_CTL ? &out->c[out->n++] : NULL;
                if (cur)
                    memset(cur, 0, sizeof(*cur));
            } else if (in_card && depth == ctl_depth && !strncmp(p, "comment", 7)) {
                comment_depth = depth + 1;
            }
            depth++;
            /* "comment { ... }" en una sola línea */
            if (strchr(p, '}'))
                depth--;
            continue;
        }
        if (closes) {
            depth--;
            if (depth < comment_depth)
                comment_depth = -1;
            if (depth < ctl_depth) {
                ctl_depth = -1;
                cur = NULL;
            }
            if (in_card && depth < card_depth)
                in_card = 0;
            continue;
        }

        if (!in_card || !cur || comment_depth > 0 || depth != ctl_depth)
            continue;

        if (!strncmp(p, "iface ", 6)) {
            snprintf(cur->iface, sizeof(cur->iface), "%s", p + 6);
        } else if (!strncmp(p, "name ", 5)) {
            snprintf(cur->name, sizeof(cur->name), "%s", p + 5);
            unquote(cur->name);
        } else if (!strncmp(p, "value", 5) && (p[5] == ' ' || p[5] == '.')) {
            char v[128];
            char *sp = strchr(p, ' ');
            if (!sp)
                continue;
            snprintf(v, sizeof(v), "%s", sp + 1);
            norm_value(v);
            if (cur->count > 0 && strlen(cur->value) + strlen(v) + 2 < sizeof(cur->value))
                strcat(cur->value, ",");
            if (strlen(cur->value) + strlen(v) + 1 < sizeof(cur->value))
                strcat(cur->value, v);
            cur->count++;
        }
    }
    free(copy);
    return found ? 0 : -1;
}

static int has_prefix(const char *s, const char *p)
{
    return !strncmp(s, p, strlen(p));
}

AtlasCtlClass atlas_ctl_classify(const AtlasStateCtl *c)
{
    static const char *speaker[] = {
        "Output Select", "Surround Channel Config", "Full-Range Front Speakers",
        "Full-Range Rear Speakers", "Bass Redirection", "Bass Redirection Crossover",
        "HP/Speaker Auto Detect Playback Switch", NULL };
    /* lado de captura: estable, no depende del perfil */
    static const char *capture[] = {
        "FX: Voice Focus Capture Switch", "FX: Mic SVM Capture Switch",
        "FX: Noise Reduction Capture Switch", "Enable InFX Capture Switch", "VoiceFX Capture Switch",
        "Input Source", "Mic Boost Capture Switch", "SVM Level Capture Volume", NULL };
    int i;

    if (strcmp(c->iface, "MIXER") || !c->name[0] || !c->value[0])
        return ATLAS_CTL_IGNORE;
    if (strstr(c->name, "IEC958") || strstr(c->name, "Jack") || strstr(c->name, "Channel Map") ||
        !strcmp(c->name, "ELD") ||
        /* CA0132-0001: valor guardado fuera de rango, el driver lo rechaza */
        has_prefix(c->name, "Wedge Angle"))
        return ATLAS_CTL_IGNORE;
    for (i = 0; speaker[i]; i++)
        if (!strcmp(c->name, speaker[i]))
            return ATLAS_CTL_SPEAKER;
    for (i = 0; capture[i]; i++)
        if (!strcmp(c->name, capture[i]))
            return ATLAS_CTL_VOLUME;
    if (has_prefix(c->name, "FX: ") || has_prefix(c->name, "EQ Band") ||
        !strcmp(c->name, "Enable OutFX Playback Switch"))
        return ATLAS_CTL_EFFECT;
    if (strstr(c->name, " Volume") || strstr(c->name, " Switch"))
        return ATLAS_CTL_VOLUME;
    return ATLAS_CTL_IGNORE;
}

/* ============================================================ utilidades */

static int card_target(const char *alsa_id, int *card, char *hw_id, size_t n)
{
    AtlasHwDevice devs[32];
    AtlasHwDb *db = malloc(sizeof(*db));
    int i, nd, ok = -1;

    *card = -1;
    hw_id[0] = '\0';
    if (!db)
        return -1;
    atlas_hwdb_load(db);
    nd = atlas_hw_discover(devs, 32);
    for (i = 0; i < nd; i++) {
        const AtlasHwEntry *e;
        if (alsa_id && *alsa_id && strcmp(devs[i].alsa_id, alsa_id))
            continue;
        if (atlas_hwdb_match(db, &devs[i], &e, NULL) == ATLAS_MATCH_UNIQUE) {
            *card = devs[i].alsa_card;
            snprintf(hw_id, n, "%s", e->id);
            ok = 0;
            break;
        }
        if (alsa_id && *alsa_id) {
            *card = devs[i].alsa_card;
            break;
        }
    }
    free(db);
    return ok;
}

static void layer_dir(const char *hw_id, char *out, size_t n)
{
    snprintf(out, n, "%s/hardware/%s/dsp", atlas_config_dir(), hw_id);
}

static int write_layer(const char *path, const char *header, const AtlasStateCard *s, AtlasCtlClass cls,
                       int *written)
{
    FILE *f;
    char tmp[PATH_MAX];
    int i;

    *written = 0;
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    f = fopen(tmp, "w");
    if (!f)
        return -1;
    fputs(header, f);
    if (cls == ATLAS_CTL_VOLUME && !atlas_conf_bool("base.include_master"))
        fputs("# Master no forma parte de la base: es tu mando de volumen (teclas de volumen).\n"
              "# Para incluirlo: base.include_master=yes en ~/.config/audio-profile/atlas.conf\n", f);
    for (i = 0; i < s->n; i++)
        if (atlas_ctl_classify(&s->c[i]) == cls) {
            if (cls == ATLAS_CTL_VOLUME && !atlas_conf_bool("base.include_master") &&
                !strncmp(s->c[i].name, "Master Playback", 15))
                continue;
            fprintf(f, "%s=%s\n", s->c[i].name, s->c[i].value);
            (*written)++;
        }
    if (fclose(f) != 0)
        return -1;
    return rename(tmp, path);
}

static int write_personal_profile(void)
{
    char dir[PATH_MAX], path[PATH_MAX];
    snprintf(dir, sizeof(dir), "%s/profiles/personal", atlas_config_dir());
    snprintf(path, sizeof(path), "%s/profile.conf", dir);
    if (atlas_file_exists(path))
        return 0;   /* no pisar ajustes del usuario */
    atlas_mkdirs(dir);
    return atlas_write_file_atomic(path,
        "name=Mi sonido\n"
        "enabled=yes\n"
        "rate=0\n"
        "quantum=0\n"
        "description=Tu configuración de siempre (importada de tu asound.state): efectos y ecualizador "
        "tal como los dejaste.\n"
        "category=sistema\n"
        "icon=starred-symbolic\n"
        "dsp=personal\n"
        "restore_volume=no\n"
        "restore_defaults=no\n"
        "restore_links=no\n"
        "priority=15\n");
}

/* Estado actual de la tarjeta (amixer) como AtlasStateCard. */
static int live_state(int card, AtlasStateCard *s)
{
    static AtlasDspState d;
    char cards[16], *out = NULL;
    int i;

    memset(s, 0, sizeof(*s));
    snprintf(cards, sizeof(cards), "%d", card);
    if (atlas_runl(&out, 5000, "amixer", "-c", cards, "contents", NULL) != 0) {
        free(out);
        return -1;
    }
    atlas_dsp_parse_contents(out, &d);
    free(out);
    for (i = 0; i < d.n && s->n < ATLAS_STATE_MAX_CTL; i++) {
        AtlasStateCtl *c = &s->c[s->n++];
        memset(c, 0, sizeof(*c));
        snprintf(c->iface, sizeof(c->iface), "MIXER");
        snprintf(c->name, sizeof(c->name), "%s", d.c[i].name);
        if (!strcmp(d.c[i].type, "ENUMERATED") && d.c[i].nitems) {
            int idx = atoi(d.c[i].values);
            snprintf(c->value, sizeof(c->value), "%s",
                     idx >= 0 && idx < d.c[i].nitems ? d.c[i].items[idx] : d.c[i].values);
        } else {
            snprintf(c->value, sizeof(c->value), "%s", d.c[i].values);
        }
        if (!strchr(d.c[i].access, 'w'))
            c->iface[0] = '\0';   /* solo lectura: ignorar */
    }
    return 0;
}

int atlas_base_apply(void)
{
    /* base + volúmenes (+ tus efectos con profiles.effects=personal) */
    return ppm_dsp_apply("volumes");
}

/* ===================================================== vigilancia de la base */

/* ¿El valor vivo de amixer es el esperado? «90» equivale a «90,90»;
 * on/off/true/false se comparan sin distinguir. */
int atlas_value_matches(const char *want, const char *have)
{
    char w[256], h[256], *ws = NULL, *hs = NULL, *wt, *ht;
    const char *first = NULL;

    if (!strcmp(want, have))
        return 1;
    snprintf(w, sizeof(w), "%s", want);
    snprintf(h, sizeof(h), "%s", have);
    norm_value(w);
    norm_value(h);
    wt = strtok_r(w, ",", &ws);
    ht = strtok_r(h, ",", &hs);
    while (ht) {
        char a[64], b[64];
        if (wt)
            first = wt;
        else if (!first)
            return 0;
        snprintf(a, sizeof(a), "%s", wt ? wt : first);
        snprintf(b, sizeof(b), "%s", ht);
        norm_value(a);
        norm_value(b);
        if (strcasecmp(a, b))
            return 0;
        if (wt)
            wt = strtok_r(NULL, ",", &ws);
        ht = strtok_r(NULL, ",", &hs);
    }
    return wt == NULL;
}

/* Controles de `names/values` que no coinciden con `live`. Los que la
 * tarjeta no tiene o son de solo lectura se ignoran. Devuelve cuántos. */
int atlas_base_diff(char names[][128], char values[][128], int n, const AtlasStateCard *live, int *out)
{
    int i, j, nd = 0;

    for (i = 0; i < n; i++) {
        int dup = 0;
        /* las capas posteriores mandan: solo cuenta la última aparición */
        for (j = i + 1; j < n; j++)
            if (!strcmp(names[i], names[j]))
                dup = 1;
        if (dup)
            continue;
        for (j = 0; j < live->n; j++)
            if (!strcmp(live->c[j].name, names[i]))
                break;
        if (j == live->n || !live->c[j].iface[0])
            continue;
        if (!atlas_value_matches(values[i], live->c[j].value))
            out[nd++] = i;
    }
    return nd;
}

/*
 * Llamado periódicamente por el daemon: compara la tarjeta con tu base
 * (altavoces + volúmenes + [auriculares] + efectos si son los tuyos) y
 * corrige lo que se haya movido. Un control que vuelve a desviarse tras
 * 3 correcciones seguidas deja de vigilarse (el driver no lo acepta) y se
 * registra una sola vez.
 */
int atlas_base_enforce(char *log, size_t logn)
{
    static AtlasStateCard live;
    static char names[ATLAS_DSP_MAX_CTL * 4][128], values[ATLAS_DSP_MAX_CTL * 4][128];
    static char strikes_name[32][128];
    static int strikes[32], nstrikes;
    char hw_id[64], hw_dir[PATH_MAX], files[4][PATH_MAX], cards[16], namearg[160];
    const char *preset;
    int card, nf, i, n = 0, nd, fixed = 0, diff[ATLAS_DSP_MAX_CTL * 4];
    size_t used = 0;

    static char c_hw_id[64], c_hw_dir[PATH_MAX];
    static int c_card = -1;
    static long long c_when;

    if (log && logn)
        log[0] = '\0';
    /* la tarjeta destino cuesta 6 llamadas a pactl: se recuerda 5 minutos */
    if (c_card < 0 || atlas_now_ms() - c_when > 300000) {
        if (atlas_dsp_target(&c_card, c_hw_id, sizeof(c_hw_id), c_hw_dir, sizeof(c_hw_dir)) != 0 ||
            !c_hw_id[0]) {
            c_card = -1;
            return 0;
        }
        c_when = atlas_now_ms();
    }
    card = c_card;
    snprintf(hw_id, sizeof(hw_id), "%s", c_hw_id);
    snprintf(hw_dir, sizeof(hw_dir), "%s", c_hw_dir);
    preset = atlas_effects_preset("volumes", hw_id);   /* personal o solo volumes */
    nf = atlas_dsp_layer_files(preset, hw_id, hw_dir, files, 4, NULL);
    for (i = 0; i < nf; i++) {
        char *t = atlas_read_file(files[i], 1 << 16);
        if (t) {
            n += atlas_dsp_parse_preset(t, names + n, values + n, ATLAS_DSP_MAX_CTL * 4 - n);
            free(t);
        }
    }
    if (!n || live_state(card, &live) != 0)
        return 0;
    nd = atlas_base_diff(names, values, n, &live, diff);
    snprintf(cards, sizeof(cards), "%d", card);
    for (i = 0; i < nd; i++) {
        const char *nm = names[diff[i]];
        int k;
        for (k = 0; k < nstrikes && strcmp(strikes_name[k], nm); k++)
            ;
        if (k == nstrikes && nstrikes < 32) {
            snprintf(strikes_name[nstrikes], sizeof(strikes_name[0]), "%s", nm);
            strikes[nstrikes++] = 0;
        }
        if (k < 32 && strikes[k] >= 3)
            continue;
        if (k < 32 && ++strikes[k] == 3 && log)
            used += snprintf(log + used, used < logn ? logn - used : 0,
                             "%s%s (no se mantiene, dejo de corregirlo)", used ? "; " : "", nm);
        snprintf(namearg, sizeof(namearg), "name=%s", nm);
        if (atlas_runl(NULL, 3000, "amixer", "-c", cards, "-q", "cset", namearg, values[diff[i]], NULL) == 0) {
            fixed++;
            if (log && used < logn)
                used += snprintf(log + used, logn - used, "%s%s→%s", used ? ", " : "", nm, values[diff[i]]);
        }
    }
    /* los que ya coinciden recuperan la confianza */
    for (i = 0; i < nstrikes; i++) {
        int j, still = 0;
        for (j = 0; j < nd; j++)
            if (!strcmp(names[diff[j]], strikes_name[i]))
                still = 1;
        if (!still && strikes[i] < 3)
            strikes[i] = 0;
    }
    return fixed;
}

/* ===================================================== bloqueo del mezclador */

/*
 * Por defecto PipeWire (ACP) usa el mezclador de hardware como volumen del
 * sink y lo reescribe cada vez que se activa el perfil de la tarjeta
 * (suspensión, recuperación, cambio a auriculares): Master/PCM/Front cambian.
 * Con api.alsa.soft-mixer=true el volumen del sink es software y el hardware
 * conserva SIEMPRE la base del usuario.
 */
static void hwlock_path(char *buf, size_t n)
{
    const char *x = getenv("XDG_CONFIG_HOME");
    const char *h = getenv("HOME");
    if (x && *x)
        snprintf(buf, n, "%s/wireplumber/wireplumber.conf.d/60-atlas-hw-mixer-lock.conf", x);
    else
        snprintf(buf, n, "%s/.config/wireplumber/wireplumber.conf.d/60-atlas-hw-mixer-lock.conf",
                 h ? h : "");
}

static int hwlock(int argc, char **argv)
{
    const char *sub = argc > 1 ? argv[1] : "status";
    AtlasAudio *a = malloc(sizeof(*a));
    char path[PATH_MAX], hw_id[64], hw_dir[PATH_MAX], text[1024];
    const AtlasCard *c = NULL;
    int card, i, on;

    if (!a)
        return 2;
    hwlock_path(path, sizeof(path));
    on = atlas_file_exists(path);
    if (atlas_dsp_target(&card, hw_id, sizeof(hw_id), hw_dir, sizeof(hw_dir)) == 0 && atlas_audio_query(a) == 0)
        c = atlas_audio_card_by_alsa(a, card);

    if (!strcmp(sub, "status")) {
        printf("Bloqueo del mezclador de hardware: %s\n", on ? "ACTIVADO" : "desactivado");
        printf("  %s\n", on ? "PipeWire no toca Master/PCM/Front: tu base se mantiene siempre."
                            : "PipeWire reescribe el mezclador al reconstruir la tarjeta "
                              "(suspensión, recuperación, auriculares).");
        printf("  Regla: %s\n", path);
        free(a);
        return 0;
    }
    if (strcmp(sub, "on") && strcmp(sub, "off")) {
        fprintf(stderr, "Uso: audio-profile base hw-lock [status|on|off]\n");
        free(a);
        return 2;
    }
    if (!strcmp(sub, "on")) {
        if (!c) {
            fprintf(stderr, "No encuentro la tarjeta en PipeWire.\n");
            free(a);
            return 1;
        }
        snprintf(text, sizeof(text),
                 "# Project Atlas: el volumen del sink es software y el mezclador de hardware\n"
                 "# de %s conserva la base del usuario (audio-profile base).\n"
                 "# Quitar: audio-profile base hw-lock off\n"
                 "monitor.alsa.rules = [\n"
                 "  {\n"
                 "    matches = [ { device.name = \"%s\" } ]\n"
                 "    actions = { update-props = { api.alsa.soft-mixer = true } }\n"
                 "  }\n"
                 "]\n", c->description, c->name);
        {
            char dir[PATH_MAX];
            snprintf(dir, sizeof(dir), "%s", path);
            *strrchr(dir, '/') = '\0';
            atlas_mkdirs(dir);
        }
        if (atlas_write_file_atomic(path, text) != 0) {
            fprintf(stderr, "No pude escribir %s\n", path);
            free(a);
            return 1;
        }
    } else if (on && remove(path) != 0) {
        fprintf(stderr, "No pude borrar %s\n", path);
        free(a);
        return 1;
    }

    puts("Reiniciando WirePlumber para aplicar…");
    atlas_runl(NULL, 15000, "systemctl", "--user", "restart", "wireplumber.service", NULL);
    atlas_audio_wait_ready(NULL, 8000);
    atlas_msleep(1500);
    if (!strcmp(sub, "on") && atlas_audio_query(a) == 0 && (c = atlas_audio_card_by_alsa(a, card))) {
        /* el nivel real lo da la base de hardware: el volumen software a 0 dB */
        for (i = 0; i < a->nsinks; i++)
            if (!strcmp(a->sinks[i].card_name, c->name))
                atlas_audio_set_volume_percent(1, a->sinks[i].name, 100);
    }
    {
        char cur[128];
        atlas_current_profile(cur, sizeof(cur));
        if (!strcmp(cur, "ninguno") || atlas_apply_profile_id(cur, 1) != 0)
            atlas_base_apply();
    }
    printf("Bloqueo del mezclador: %s\n", !strcmp(sub, "on") ? "ACTIVADO" : "desactivado");
    free(a);
    return 0;
}

/* ============================================================== comando */

static void usage(void)
{
    fprintf(stderr,
        "Uso:\n"
        "  audio-profile base import [archivo] [--card ID] [--no-speakers] [--no-personal] [--dry-run]\n"
        "        importa tu asound.state (por defecto ~/.config/asound.state) como base\n"
        "  audio-profile base save      guarda los volúmenes y altavoces actuales como base\n"
        "  audio-profile base show      muestra la base activa\n"
        "  audio-profile base apply     aplica base de altavoces + volúmenes\n"
        "  audio-profile base hw-lock [status|on|off]\n"
        "        impide que PipeWire reescriba el mezclador de hardware\n");
}

int atlas_cmd_base(int argc, char **argv)
{
    static AtlasStateCard s;
    const char *sub = argc > 0 ? argv[0] : "show";
    const char *file = NULL, *card_id = "Creative";
    int speakers = 1, personal = 1, dry = 0, i, card;
    char hw_id[64], dir[PATH_MAX], path[PATH_MAX], def[PATH_MAX];

    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--card") && i + 1 < argc)
            card_id = argv[++i];
        else if (!strcmp(argv[i], "--no-speakers"))
            speakers = 0;
        else if (!strcmp(argv[i], "--no-personal"))
            personal = 0;
        else if (!strcmp(argv[i], "--dry-run"))
            dry = 1;
        else
            file = argv[i];
    }

    if (!strcmp(sub, "apply"))
        return atlas_base_apply() == 0 ? 0 : 1;
    if (!strcmp(sub, "hw-lock"))
        return hwlock(argc, argv);

    if (card_target(card_id, &card, hw_id, sizeof(hw_id)) != 0) {
        fprintf(stderr, "La tarjeta '%s' no está en la Hardware DB (audio-profile devices).\n", card_id);
        return 1;
    }
    layer_dir(hw_id, dir, sizeof(dir));

    if (!strcmp(sub, "show")) {
        const char *layers[] = { "base", "volumes", "personal" };
        for (i = 0; i < 3; i++) {
            char *t;
            snprintf(path, sizeof(path), "%s/%s.conf", dir, layers[i]);
            t = atlas_read_file(path, 1 << 16);
            printf("== %s %s\n", path, t ? "" : "(no existe: se usa el de fábrica o nada)");
            if (t) {
                fputs(t, stdout);
                free(t);
            }
        }
        return 0;
    }

    if (!strcmp(sub, "import") || !strcmp(sub, "save")) {
        int nv, ns, ne;
        char ts[64], header[512];

        if (!strcmp(sub, "import")) {
            char *text;
            if (!file) {
                const char *h = getenv("HOME");
                snprintf(def, sizeof(def), "%s/.config/asound.state", h ? h : "");
                file = def;
            }
            text = atlas_read_file(file, 4 << 20);
            if (!text) {
                fprintf(stderr, "No puedo leer %s\n", file);
                return 1;
            }
            if (atlas_state_parse(text, card_id, &s) != 0) {
                fprintf(stderr, "%s no contiene el bloque state.%s\n", file, card_id);
                free(text);
                return 1;
            }
            free(text);
        } else {
            if (live_state(card, &s) != 0) {
                fprintf(stderr, "No pude leer los controles de la tarjeta %d.\n", card);
                return 1;
            }
            personal = 0;   /* save captura volúmenes y altavoces, no efectos */
            file = "estado actual de la tarjeta";
        }

        atlas_timestamp(ts, sizeof(ts), 0);
        if (dry) {
            for (i = 0; i < s.n; i++) {
                const char *k[] = { "volumen", "altavoces", "efecto", "ignorado" };
                printf("%-10s %s=%s\n", k[atlas_ctl_classify(&s.c[i])], s.c[i].name, s.c[i].value);
            }
            return 0;
        }
        if (atlas_mkdirs(dir) != 0) {
            fprintf(stderr, "No pude crear %s\n", dir);
            return 1;
        }

        snprintf(header, sizeof(header),
                 "# Base de volumen del usuario (se aplica con TODOS los perfiles)\n"
                 "# Origen: %s · %s\n", file, ts);
        snprintf(path, sizeof(path), "%s/volumes.conf", dir);
        if (write_layer(path, header, &s, ATLAS_CTL_VOLUME, &nv) != 0) {
            fprintf(stderr, "No pude escribir %s\n", path);
            return 1;
        }
        printf("✓ Volúmenes base: %d controles → %s\n", nv, path);

        if (speakers) {
            snprintf(header, sizeof(header),
                     "# Configuración de altavoces del usuario (sustituye al base.conf de fábrica)\n"
                     "# Origen: %s · %s\n", file, ts);
            snprintf(path, sizeof(path), "%s/base.conf", dir);
            write_layer(path, header, &s, ATLAS_CTL_SPEAKER, &ns);
            printf("✓ Altavoces: %d controles → %s\n", ns, path);
        }
        if (personal) {
            snprintf(header, sizeof(header),
                     "# Tus efectos y ecualizador (perfil \"Mi sonido\")\n# Origen: %s · %s\n", file, ts);
            snprintf(path, sizeof(path), "%s/personal.conf", dir);
            write_layer(path, header, &s, ATLAS_CTL_EFFECT, &ne);
            write_personal_profile();
            printf("✓ Efectos: %d controles → %s (perfil 'personal')\n", ne, path);
        }

        /* copia íntegra como preset ALSA del usuario, por si se quiere restaurar entero */
        if (!strcmp(sub, "import")) {
            char pdir[PATH_MAX], dst[PATH_MAX], *t = atlas_read_file(file, 4 << 20);
            snprintf(pdir, sizeof(pdir), "%s/presets/alsa", atlas_config_dir());
            snprintf(dst, sizeof(dst), "%s/base-usuario.state", pdir);
            if (t && atlas_mkdirs(pdir) == 0 && atlas_write_file_atomic(dst, t) == 0)
                printf("✓ Copia íntegra: %s\n", dst);
            free(t);
        }
        return 0;
    }

    usage();
    return 2;
}
