#define _GNU_SOURCE
#include <ctype.h>
#include <dirent.h>
#include <fnmatch.h>
#include <glob.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "atlas.h"
#include <gio/gio.h>

#include "atlas_auto.h"
#include "atlas_steam.h"
#include "profile.h"
#include "profile_apply.h"

/* ============================================================== estado */

static void state_path(char *buf, size_t n)
{
    snprintf(buf, n, "%s/auto.json", atlas_state_dir());
}

int atlas_auto_state_load(AtlasAutoState *s)
{
    char path[PATH_MAX], *data;
    JsonParser *p;

    memset(s, 0, sizeof(*s));
    s->enabled = 1;
    state_path(path, sizeof(path));
    data = atlas_read_file(path, 65536);
    if (!data)
        return -1;
    p = json_parser_new();
    if (json_parser_load_from_data(p, data, -1, NULL) &&
        JSON_NODE_HOLDS_OBJECT(json_parser_get_root(p))) {
        JsonObject *o = json_node_get_object(json_parser_get_root(p));
        if (json_object_has_member(o, "enabled"))
            s->enabled = json_object_get_boolean_member(o, "enabled");
        if (json_object_has_member(o, "manual_lock"))
            s->manual_lock = json_object_get_boolean_member(o, "manual_lock");
        if (json_object_has_member(o, "lock_winner"))
            snprintf(s->lock_winner, sizeof(s->lock_winner), "%s",
                     json_object_get_string_member(o, "lock_winner"));
        if (json_object_has_member(o, "current"))
            snprintf(s->current, sizeof(s->current), "%s", json_object_get_string_member(o, "current"));
        if (json_object_has_member(o, "reason"))
            snprintf(s->reason, sizeof(s->reason), "%s", json_object_get_string_member(o, "reason"));
        if (json_object_has_member(o, "updated"))
            snprintf(s->updated, sizeof(s->updated), "%s", json_object_get_string_member(o, "updated"));
    }
    g_object_unref(p);
    free(data);
    return 0;
}

int atlas_auto_state_save(const AtlasAutoState *s)
{
    char path[PATH_MAX];
    JsonBuilder *b = json_builder_new();
    JsonGenerator *g = json_generator_new();
    JsonNode *root;
    gchar *txt;
    int r;

    atlas_mkdirs(atlas_state_dir());
    json_builder_begin_object(b);
    atlas_json_kv_bool(b, "enabled", s->enabled);
    atlas_json_kv_bool(b, "manual_lock", s->manual_lock);
    atlas_json_kv_str(b, "lock_winner", s->lock_winner);
    atlas_json_kv_str(b, "current", s->current);
    atlas_json_kv_str(b, "reason", s->reason);
    atlas_json_kv_str(b, "updated", s->updated);
    json_builder_end_object(b);
    root = json_builder_get_root(b);
    json_generator_set_root(g, root);
    json_generator_set_pretty(g, TRUE);
    txt = json_generator_to_data(g, NULL);
    state_path(path, sizeof(path));
    r = atlas_write_file_atomic(path, txt);
    g_free(txt);
    json_node_unref(root);
    g_object_unref(g);
    g_object_unref(b);
    return r;
}

int atlas_auto_should_release(const AtlasAutoState *s, const char *winner)
{
    return s->enabled && s->manual_lock && winner && winner[0] && strcmp(winner, s->lock_winner);
}

const char *atlas_auto_mode(const AtlasAutoState *s)
{
    if (!s->enabled)
        return "manual";
    return s->manual_lock ? "auto-paused" : "auto";
}

void atlas_auto_mark_manual(const char *profile)
{
    AtlasAutoState s;
    AtlasAutoStream st[ATLAS_AUTO_MAX_STREAMS];
    char winner[64] = "", reason[256];
    int n = atlas_auto_streams(st, ATLAS_AUTO_MAX_STREAMS);

    if (n <= 0 || atlas_auto_evaluate(st, n, winner, sizeof(winner), reason, sizeof(reason)) != 0)
        winner[0] = '\0';
    atlas_auto_state_load(&s);
    s.manual_lock = 1;
    snprintf(s.lock_winner, sizeof(s.lock_winner), "%s", winner);
    snprintf(s.current, sizeof(s.current), "%s", profile);
    snprintf(s.reason, sizeof(s.reason), "aplicado manualmente");
    atlas_timestamp(s.updated, sizeof(s.updated), 0);
    atlas_auto_state_save(&s);
}

int atlas_apply_profile_id(const char *id, int by_auto)
{
    AudioProfile p;

    if (!id || !atlas_safe_name(id)) {
        fprintf(stderr, "Nombre de perfil no válido.\n");
        return -1;
    }
    if (profile_load(id, &p) != 0) {
        fprintf(stderr, "No pude cargar perfil: %s\n", id);
        return -1;
    }
    if (profile_apply(&p) != 0) {
        fprintf(stderr, "No pude aplicar perfil: %s\n", id);
        return -1;
    }
    if (!by_auto)
        atlas_auto_mark_manual(id);
    return 0;
}

/* ============================================================== reglas */

static int ci_contains(const char *hay, const char *needle)
{
    return hay && needle && *needle && strcasestr(hay, needle) != NULL;
}

int atlas_auto_rule_matches(const char *conds, const AtlasAutoStream *st)
{
    char copy[512], *tok, *save = NULL;
    int any = 0;

    snprintf(copy, sizeof(copy), "%s", conds);
    for (tok = strtok_r(copy, " \t", &save); tok; tok = strtok_r(NULL, " \t", &save)) {
        char *op;
        const char *val;
        if (*tok == '#')
            break;
        any = 1;
        if ((op = strstr(tok, "~"))) {
            *op = '\0';
            val = op + 1;
            /* "_" en el valor equivale a espacio (las reglas se separan por espacios) */
            char v[256];
            size_t i;
            snprintf(v, sizeof(v), "%s", val);
            for (i = 0; v[i]; i++)
                if (v[i] == '_')
                    v[i] = ' ';
            if (!strcmp(tok, "app") && !ci_contains(st->app, v)) return 0;
            if (!strcmp(tok, "media") && !ci_contains(st->media, v)) return 0;
            /* title~ mira la ventana (Sway) y lo que el proceso reproduce por MPRIS */
            if (!strcmp(tok, "title") && !ci_contains(st->title, v) && !ci_contains(st->mpris, v)) return 0;
            if (!strcmp(tok, "url") && !ci_contains(st->url, v)) return 0;
            if (!strcmp(tok, "binary") && !ci_contains(st->binary, v)) return 0;
            if (strcmp(tok, "app") && strcmp(tok, "media") && strcmp(tok, "title") &&
                strcmp(tok, "binary") && strcmp(tok, "url"))
                return 0;
        } else if ((op = strchr(tok, '='))) {
            *op = '\0';
            val = op + 1;
            if (!strcmp(tok, "binary")) {
                if (fnmatch(val, st->binary, FNM_CASEFOLD) != 0)
                    return 0;
            } else if (!strcmp(tok, "role")) {
                if (strcasecmp(val, st->role) != 0)
                    return 0;
            } else if (!strcmp(tok, "steam")) {
                if (!st->steam_appid[0])
                    return 0;
                if (strcmp(val, "*") && strcmp(val, st->steam_appid))
                    return 0;
            } else if (!strcmp(tok, "app")) {
                if (fnmatch(val, st->app, FNM_CASEFOLD) != 0)
                    return 0;
            } else if (!strcmp(tok, "genre")) {
                if (!st->genre[0] || strcasecmp(val, st->genre) != 0)
                    return 0;
            } else {
                return 0;   /* condición desconocida: nunca coincide */
            }
        } else if (!strcmp(tok, "any")) {
            /* comodín */
        } else {
            return 0;
        }
    }
    return any;
}

typedef struct {
    char profile[64];
    char conds[384];
    char line[460];
} Rule;

static int load_rules(Rule *rules, int max)
{
    char paths[2][PATH_MAX];
    int n = 0, i;

    snprintf(paths[0], sizeof(paths[0]), "%s/auto.rules", atlas_config_dir());
    snprintf(paths[1], sizeof(paths[1]), "%s/rules/auto.rules", atlas_data_dir());
    for (i = 0; i < 2; i++) {
        FILE *f = fopen(paths[i], "r");
        char line[512];
        if (!f)
            continue;
        while (fgets(line, sizeof(line), f) && n < max) {
            char *p = line, *sp;
            line[strcspn(line, "\r\n")] = '\0';
            while (isspace((unsigned char)*p))
                p++;
            if (!*p || *p == '#')
                continue;
            sp = p + strcspn(p, " \t");
            if (!*sp)
                continue;
            *sp++ = '\0';
            if (!atlas_safe_name(p))
                continue;
            snprintf(rules[n].profile, sizeof(rules[n].profile), "%s", p);
            snprintf(rules[n].conds, sizeof(rules[n].conds), "%s", sp);
            snprintf(rules[n].line, sizeof(rules[n].line), "%s %s", p, sp);
            n++;
        }
        fclose(f);
    }
    return n;
}

/* ======================================================= descubrimiento */

static void steam_appid(int pid, char *out, size_t n)
{
    char path[64], *env;
    size_t len = 0;
    FILE *f;

    out[0] = '\0';
    if (pid <= 0)
        return;
    snprintf(path, sizeof(path), "/proc/%d/environ", pid);
    f = fopen(path, "rb");
    if (!f)
        return;
    env = malloc(1 << 17);
    if (env) {
        size_t i = 0;
        len = fread(env, 1, (1 << 17) - 1, f);
        env[len] = '\0';
        while (i < len) {
            const char *v = env + i;
            if (!strncmp(v, "SteamAppId=", 11) && v[11] && strcmp(v + 11, "0")) {
                snprintf(out, n, "%s", v + 11);
                break;
            }
            if (!strncmp(v, "SteamGameId=", 12) && v[12] && strcmp(v + 12, "0") && !out[0])
                snprintf(out, n, "%s", v + 12);
            i += strlen(v) + 1;
        }
        free(env);
    }
    fclose(f);
}

/* pid -> títulos de ventana en Sway */
typedef struct {
    int pid;
    char title[256];
} WinTitle;

static void walk_tree(JsonObject *o, WinTitle *w, int *n, int max)
{
    const char *keys[] = { "nodes", "floating_nodes" };
    int k;

    if (json_object_has_member(o, "pid") && json_object_has_member(o, "name") && *n < max) {
        JsonNode *nn = json_object_get_member(o, "name");
        if (JSON_NODE_TYPE(nn) == JSON_NODE_VALUE && json_node_get_value_type(nn) == G_TYPE_STRING) {
            w[*n].pid = (int)json_object_get_int_member(o, "pid");
            snprintf(w[*n].title, sizeof(w[*n].title), "%s", json_node_get_string(nn));
            (*n)++;
        }
    }
    for (k = 0; k < 2; k++) {
        JsonArray *a;
        guint i;
        if (!json_object_has_member(o, keys[k]))
            continue;
        a = json_object_get_array_member(o, keys[k]);
        for (i = 0; a && i < json_array_get_length(a); i++)
            walk_tree(json_array_get_object_element(a, i), w, n, max);
    }
}

static int sway_titles(WinTitle *w, int max)
{
    char *out = NULL, sock[PATH_MAX] = "";
    const char *env = getenv("SWAYSOCK");
    JsonParser *p;
    int n = 0, r;

    if (!atlas_have_cmd("swaymsg"))
        return 0;
    if (env && *env) {
        snprintf(sock, sizeof(sock), "%s", env);
    } else {
        /* servicio systemd sin SWAYSOCK: buscar el socket del usuario */
        glob_t g;
        char pat[128];
        snprintf(pat, sizeof(pat), "/run/user/%d/sway-ipc.*.sock", (int)getuid());
        if (glob(pat, 0, NULL, &g) == 0 && g.gl_pathc > 0)
            snprintf(sock, sizeof(sock), "%s", g.gl_pathv[g.gl_pathc - 1]);
        globfree(&g);
    }
    if (!sock[0])
        return 0;
    r = atlas_runl(&out, 2000, "swaymsg", "-s", sock, "-t", "get_tree", "-r", NULL);
    if (r != 0 || !out) {
        free(out);
        return 0;
    }
    p = json_parser_new();
    if (json_parser_load_from_data(p, out, -1, NULL) && JSON_NODE_HOLDS_OBJECT(json_parser_get_root(p)))
        walk_tree(json_node_get_object(json_parser_get_root(p)), w, &n, max);
    g_object_unref(p);
    free(out);
    return n;
}

static const char *pstr(JsonObject *props, const char *k)
{
    if (!props || !json_object_has_member(props, k))
        return "";
    return json_object_get_string_member(props, k);
}

/* --- MPRIS: qué reproduce cada proceso (también pestañas en segundo plano) --- */

typedef struct {
    char bus[160];
    int pid;
    int playing;
    char title[256];
    char url[256];
} Player;

static GDBusConnection *session_bus(void)
{
    static GDBusConnection *c;
    if (!c)
        c = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL);
    return c;
}

static GVariant *call(GDBusConnection *c, const char *dest, const char *path, const char *iface,
                      const char *method, GVariant *args, const char *type)
{
    return g_dbus_connection_call_sync(c, dest, path, iface, method, args,
                                       type ? G_VARIANT_TYPE(type) : NULL,
                                       G_DBUS_CALL_FLAGS_NO_AUTO_START, 800, NULL, NULL);
}

static void meta_str(GVariant *meta, const char *key, char *out, size_t n)
{
    GVariant *v = g_variant_lookup_value(meta, key, NULL);
    if (!v)
        return;
    if (g_variant_is_of_type(v, G_VARIANT_TYPE_STRING))
        snprintf(out, n, "%s", g_variant_get_string(v, NULL));
    g_variant_unref(v);
}

static int mpris_players(Player *pl, int max)
{
    GDBusConnection *c = session_bus();
    GVariant *r, *names;
    GVariantIter it;
    const char *name;
    int n = 0;

    if (!c)
        return 0;
    r = call(c, "org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "ListNames",
             NULL, "(as)");
    if (!r)
        return 0;
    names = g_variant_get_child_value(r, 0);
    g_variant_iter_init(&it, names);
    while (g_variant_iter_next(&it, "&s", &name) && n < max) {
        Player *p;
        GVariant *v, *inner;
        if (strncmp(name, "org.mpris.MediaPlayer2.", 23))
            continue;
        p = &pl[n];
        memset(p, 0, sizeof(*p));
        snprintf(p->bus, sizeof(p->bus), "%s", name + 23);

        v = call(c, "org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus",
                 "GetConnectionUnixProcessID", g_variant_new("(s)", name), "(u)");
        if (v) {
            guint32 pid;
            g_variant_get(v, "(u)", &pid);
            p->pid = (int)pid;
            g_variant_unref(v);
        }
        v = call(c, name, "/org/mpris/MediaPlayer2", "org.freedesktop.DBus.Properties", "Get",
                 g_variant_new("(ss)", "org.mpris.MediaPlayer2.Player", "PlaybackStatus"), "(v)");
        if (v) {
            g_variant_get(v, "(v)", &inner);
            p->playing = g_variant_is_of_type(inner, G_VARIANT_TYPE_STRING) &&
                         !strcmp(g_variant_get_string(inner, NULL), "Playing");
            g_variant_unref(inner);
            g_variant_unref(v);
        }
        v = call(c, name, "/org/mpris/MediaPlayer2", "org.freedesktop.DBus.Properties", "Get",
                 g_variant_new("(ss)", "org.mpris.MediaPlayer2.Player", "Metadata"), "(v)");
        if (v) {
            GVariant *kp;
            g_variant_get(v, "(v)", &inner);
            if (g_variant_is_of_type(inner, G_VARIANT_TYPE_VARDICT)) {
                meta_str(inner, "xesam:title", p->title, sizeof(p->title));
                meta_str(inner, "xesam:url", p->url, sizeof(p->url));
                /* plasma-browser-integration indica el pid real del navegador */
                kp = g_variant_lookup_value(inner, "kde:pid", G_VARIANT_TYPE_INT32);
                if (kp) {
                    p->pid = g_variant_get_int32(kp);
                    g_variant_unref(kp);
                }
            }
            g_variant_unref(inner);
            g_variant_unref(v);
        }
        n++;
    }
    g_variant_unref(names);
    g_variant_unref(r);
    return n;
}

/* ¿el reproductor MPRIS pertenece a este stream? pid exacto, o mismo programa
 * (org.mpris.MediaPlayer2.brave.instanceN <-> binary=brave) */
static int player_matches(const Player *p, const AtlasAutoStream *s)
{
    char prog[64];
    if (p->pid > 0 && p->pid == s->pid)
        return 1;
    snprintf(prog, sizeof(prog), "%.*s", (int)strcspn(p->bus, "."), p->bus);
    if (!prog[0])
        return 0;
    if (s->binary[0])
        return strcasestr(s->binary, prog) != NULL;
    /* algunos clientes no publican el ejecutable: usar el nombre de la aplicación */
    return s->app[0] && strcasestr(s->app, prog) != NULL;
}

int atlas_auto_streams(AtlasAutoStream *out, int max)
{
    char *json = NULL;
    JsonParser *p;
    JsonArray *arr;
    WinTitle wins[128];
    Player players[16];
    int nwins, nplayers, n = 0;
    guint i;

    if (atlas_runl(&json, 4000, "pactl", "-f", "json", "list", "sink-inputs", NULL) != 0) {
        free(json);
        return -1;
    }
    p = json_parser_new();
    if (!json_parser_load_from_data(p, json, -1, NULL) || !JSON_NODE_HOLDS_ARRAY(json_parser_get_root(p))) {
        g_object_unref(p);
        free(json);
        return -1;
    }
    arr = json_node_get_array(json_parser_get_root(p));
    /* nada sonando: no preguntar a sway ni a los reproductores */
    if (json_array_get_length(arr) == 0) {
        g_object_unref(p);
        free(json);
        return 0;
    }
    nwins = sway_titles(wins, 128);
    nplayers = mpris_players(players, 16);
    for (i = 0; i < json_array_get_length(arr) && n < max; i++) {
        JsonObject *o = json_array_get_object_element(arr, i);
        JsonObject *props = json_object_has_member(o, "properties")
                                ? json_object_get_object_member(o, "properties") : NULL;
        AtlasAutoStream *s = &out[n];
        int k;

        memset(s, 0, sizeof(*s));
        snprintf(s->media, sizeof(s->media), "%s", pstr(props, "media.name"));
        if (strstr(s->media, "Atlas silent probe") || strstr(s->media, "Atlas test tone"))
            continue;
        snprintf(s->binary, sizeof(s->binary), "%s", pstr(props, "application.process.binary"));
        /* streams nativos de PipeWire (mpv...): el binario está en el cliente,
         * no en el stream; application.id suele ser ese mismo nombre */
        if (!s->binary[0])
            snprintf(s->binary, sizeof(s->binary), "%s", pstr(props, "application.id"));
        snprintf(s->app, sizeof(s->app), "%s", pstr(props, "application.name"));
        snprintf(s->role, sizeof(s->role), "%s", pstr(props, "media.role"));
        s->pid = atoi(pstr(props, "application.process.id"));
        s->corked = json_object_has_member(o, "corked") && json_object_get_boolean_member(o, "corked");
        steam_appid(s->pid, s->steam_appid, sizeof(s->steam_appid));
        if (s->steam_appid[0])
            atlas_steam_genre(s->steam_appid, s->genre, sizeof(s->genre));
        for (k = 0; k < nwins; k++)
            if (wins[k].pid == s->pid && s->pid > 0) {
                size_t l = strlen(s->title);
                snprintf(s->title + l, sizeof(s->title) - l, "%s%s", l ? " | " : "", wins[k].title);
            }
        /* solo lo que está sonando: una pestaña pausada no decide el perfil */
        for (k = 0; k < nplayers; k++)
            if (players[k].playing && player_matches(&players[k], s)) {
                size_t l = strlen(s->mpris);
                snprintf(s->mpris + l, sizeof(s->mpris) - l, "%s%s", l ? " | " : "", players[k].title);
                if (!s->url[0])
                    snprintf(s->url, sizeof(s->url), "%s", players[k].url);
            }
        n++;
    }
    g_object_unref(p);
    free(json);
    return n;
}

int atlas_auto_evaluate(AtlasAutoStream *st, int n, char *winner, size_t wn, char *reason, size_t rn)
{
    static Rule rules[256];
    int nr = load_rules(rules, 256), i, j, best_prio = -1;

    winner[0] = '\0';
    reason[0] = '\0';
    for (i = 0; i < n; i++) {
        AudioProfile p;
        for (j = 0; j < nr; j++)
            if (atlas_auto_rule_matches(rules[j].conds, &st[i])) {
                snprintf(st[i].profile, sizeof(st[i].profile), "%s", rules[j].profile);
                snprintf(st[i].rule, sizeof(st[i].rule), "%s", rules[j].line);
                break;
            }
        /* un stream en pausa muestra su regla, pero no decide el perfil */
        if (st[i].corked)
            continue;
        if (!st[i].profile[0] || profile_load(st[i].profile, &p) != 0 || !p.enabled)
            continue;
        if (p.priority > best_prio) {
            best_prio = p.priority;
            snprintf(winner, wn, "%s", st[i].profile);
            if (st[i].steam_appid[0])
                snprintf(reason, rn, "%s (Steam %s) → %s",
                         st[i].binary[0] ? st[i].binary : st[i].app, st[i].steam_appid, st[i].profile);
            else
                snprintf(reason, rn, "%s → %s",
                         st[i].binary[0] ? st[i].binary : st[i].app, st[i].profile);
        }
    }
    return winner[0] ? 0 : 1;
}

/* Añade una regla al archivo del usuario (al principio = mayor prioridad). */
int atlas_auto_assign(const char *profile, const char *conds)
{
    char path[PATH_MAX], *old, *buf;
    size_t need;
    int r;
    AudioProfile p;

    if (!atlas_safe_name(profile) || profile_load(profile, &p) != 0) {
        fprintf(stderr, "Perfil desconocido: %s\n", profile);
        return -1;
    }
    if (!conds || !*conds || strpbrk(conds, "\n\r")) {
        fprintf(stderr, "Condición no válida.\n");
        return -1;
    }
    atlas_mkdirs(atlas_config_dir());
    snprintf(path, sizeof(path), "%s/auto.rules", atlas_config_dir());
    old = atlas_read_file(path, 1 << 20);
    need = (old ? strlen(old) : 0) + strlen(profile) + strlen(conds) + 256;
    buf = malloc(need);
    if (!buf) {
        free(old);
        return -1;
    }
    if (old && strncmp(old, "# Reglas del usuario", 20) == 0) {
        char *nl = strchr(old, '\n');
        snprintf(buf, need, "%.*s\n%-12s %s\n%s", (int)(nl ? nl - old : 0), old, profile, conds,
                 nl ? nl + 1 : "");
    } else {
        snprintf(buf, need, "# Reglas del usuario (se evalúan antes que las de fábrica)\n"
                            "%-12s %s\n%s", profile, conds, old ? old : "");
    }
    r = atlas_write_file_atomic(path, buf);
    free(buf);
    free(old);
    return r;
}

/* ================================================================ tick */

#define AUTO_SETTLE_MS 3000   /* el ganador debe mantenerse este tiempo */

int atlas_auto_tick(void)
{
    static char candidate[64];
    static long long since;
    AtlasAutoStream st[ATLAS_AUTO_MAX_STREAMS];
    AtlasAutoState s;
    char winner[64], reason[256];
    int n, released = 0;

    atlas_auto_state_load(&s);
    if (!s.enabled)
        return 0;   /* modo manual */
    n = atlas_auto_streams(st, ATLAS_AUTO_MAX_STREAMS);
    if (n <= 0 || atlas_auto_evaluate(st, n, winner, sizeof(winner), reason, sizeof(reason)) != 0)
        return 0;   /* nada sonando: se mantiene el perfil actual */
    if (strcmp(winner, candidate)) {
        snprintf(candidate, sizeof(candidate), "%s", winner);
        since = atlas_now_ms();
        return AUTO_SETTLE_MS + 200;
    }
    if (atlas_now_ms() - since < AUTO_SETTLE_MS)   /* que el cambio se mantenga antes de actuar */
        return (int)(AUTO_SETTLE_MS - (atlas_now_ms() - since)) + 200;
    if (s.manual_lock) {
        if (!atlas_auto_should_release(&s, winner))
            return 0;
        printf("auto: cambió lo que suena (%s), vuelve el modo automático\n", winner);
        fflush(stdout);
        s.manual_lock = 0;
        s.lock_winner[0] = '\0';
        released = 1;
    }
    if (!strcmp(winner, s.current)) {
        if (released) {
            snprintf(s.reason, sizeof(s.reason), "%s", reason);
            atlas_timestamp(s.updated, sizeof(s.updated), 0);
            atlas_auto_state_save(&s);
        }
        return 0;
    }

    printf("auto: %s\n", reason);
    fflush(stdout);
    if (atlas_apply_profile_id(winner, 1) == 0) {
        snprintf(s.current, sizeof(s.current), "%s", winner);
        snprintf(s.reason, sizeof(s.reason), "%s", reason);
        atlas_timestamp(s.updated, sizeof(s.updated), 0);
        atlas_auto_state_save(&s);
    } else if (released) {
        atlas_auto_state_save(&s);
    }
    return 0;
}

/* ============================================================= comando */

int atlas_cmd_auto(int argc, char **argv)
{
    const char *sub = argc > 0 ? argv[0] : "status";
    AtlasAutoState s;
    AtlasAutoStream st[ATLAS_AUTO_MAX_STREAMS];
    char winner[64], reason[256];
    int n, i;

    atlas_auto_state_load(&s);

    if (!strcmp(sub, "on") || !strcmp(sub, "resume")) {
        s.enabled = 1;
        s.manual_lock = 0;
        s.lock_winner[0] = '\0';
        snprintf(s.reason, sizeof(s.reason), "automático activado");
        atlas_auto_state_save(&s);
        puts("Modo automático: Atlas elige el perfil según lo que suena.");
        return 0;
    }
    if (!strcmp(sub, "off")) {
        s.enabled = 0;
        atlas_auto_state_save(&s);
        puts("Modo manual: los perfiles solo cambian cuando los eliges tú.");
        return 0;
    }
    if (!strcmp(sub, "assign")) {
        char conds[384] = "";
        if (argc < 3) {
            fprintf(stderr, "Uso: audio-profile auto assign <perfil> <condición...>\n"
                            "  p. ej.  audio-profile auto assign racing steam=244210\n"
                            "          audio-profile auto assign fps binary=cs2\n");
            return 2;
        }
        for (i = 2; i < argc; i++) {
            size_t l = strlen(conds);
            snprintf(conds + l, sizeof(conds) - l, "%s%s", l ? " " : "", argv[i]);
        }
        if (atlas_auto_assign(argv[1], conds) != 0)
            return 1;
        printf("Regla añadida: %s %s\n", argv[1], conds);
        return 0;
    }
    if (!strcmp(sub, "rules")) {
        static Rule rules[256];
        int nr = load_rules(rules, 256);
        if (atlas_json_mode) {
            JsonBuilder *b = json_builder_new();
            json_builder_begin_array(b);
            for (i = 0; i < nr; i++) {
                json_builder_begin_object(b);
                atlas_json_kv_str(b, "profile", rules[i].profile);
                atlas_json_kv_str(b, "conditions", rules[i].conds);
                json_builder_end_object(b);
            }
            json_builder_end_array(b);
            atlas_json_print(b);
        } else {
            for (i = 0; i < nr; i++)
                printf("%-12s %s\n", rules[i].profile, rules[i].conds);
        }
        return 0;
    }

    /* status / once */
    n = atlas_auto_streams(st, ATLAS_AUTO_MAX_STREAMS);
    if (n < 0)
        n = 0;
    atlas_auto_evaluate(st, n, winner, sizeof(winner), reason, sizeof(reason));

    if (!strcmp(sub, "once")) {
        if (!winner[0]) {
            puts("Nada sonando: se mantiene el perfil actual.");
            return 0;
        }
        printf("%s\n", reason);
        if (atlas_apply_profile_id(winner, 1) != 0)
            return 1;
        snprintf(s.current, sizeof(s.current), "%s", winner);
        snprintf(s.reason, sizeof(s.reason), "%s", reason);
        s.manual_lock = 0;
        atlas_timestamp(s.updated, sizeof(s.updated), 0);
        atlas_auto_state_save(&s);
        return 0;
    }

    if (atlas_json_mode) {
        JsonBuilder *b = json_builder_new();
        json_builder_begin_object(b);
        atlas_json_kv_bool(b, "enabled", s.enabled);
        atlas_json_kv_bool(b, "manual_lock", s.manual_lock);
        atlas_json_kv_str(b, "mode", atlas_auto_mode(&s));
        atlas_json_kv_str(b, "lock_winner", s.lock_winner);
        atlas_json_kv_str(b, "current", s.current);
        atlas_json_kv_str(b, "reason", s.reason);
        atlas_json_kv_str(b, "updated", s.updated);
        atlas_json_kv_str(b, "candidate", winner);
        json_builder_set_member_name(b, "streams");
        json_builder_begin_array(b);
        for (i = 0; i < n; i++) {
            json_builder_begin_object(b);
            atlas_json_kv_str(b, "binary", st[i].binary);
            atlas_json_kv_str(b, "app", st[i].app);
            atlas_json_kv_str(b, "media", st[i].media);
            atlas_json_kv_str(b, "title", st[i].title);
            atlas_json_kv_str(b, "steam_appid", st[i].steam_appid);
            atlas_json_kv_str(b, "genre", st[i].genre);
            atlas_json_kv_str(b, "mpris", st[i].mpris);
            atlas_json_kv_str(b, "url", st[i].url);
            atlas_json_kv_int(b, "pid", st[i].pid);
            atlas_json_kv_bool(b, "corked", st[i].corked);
            atlas_json_kv_str(b, "profile", st[i].profile);
            atlas_json_kv_str(b, "rule", st[i].rule);
            json_builder_end_object(b);
        }
        json_builder_end_array(b);
        json_builder_end_object(b);
        atlas_json_print(b);
        return 0;
    }

    printf("Modo                : %s\n",
           !s.enabled ? "manual (los perfiles solo cambian cuando los eliges; audio-profile auto on)"
           : s.manual_lock ? "automático en pausa: elegiste un perfil a mano; vuelve solo cuando cambie lo que suena"
                           : "automático");
    printf("Perfil actual       : %s%s%s\n", s.current[0] ? s.current : "-",
           s.reason[0] ? " · " : "", s.reason);
    printf("Ganador ahora       : %s\n", winner[0] ? reason : "nada sonando");
    puts("\nStreams:");
    for (i = 0; i < n; i++) {
        printf("  %-16s %-24s %s%s%s%s%s → %s\n", st[i].binary[0] ? st[i].binary : "?",
               st[i].app, st[i].corked ? "[pausa] " : "",
               st[i].steam_appid[0] ? "steam=" : "", st[i].steam_appid,
               st[i].genre[0] ? " género=" : "", st[i].genre,
               st[i].profile[0] ? st[i].profile : "(sin regla)");
        if (st[i].mpris[0])
            printf("      ▶ %s%s%s\n", st[i].mpris, st[i].url[0] ? "  " : "", st[i].url);
    }
    if (!n)
        puts("  (ninguno)");
    return 0;
}
