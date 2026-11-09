#define _GNU_SOURCE
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

#include <json-glib/json-glib.h>

#include "atlas.h"
#include "atlas_steam.h"

typedef struct {
    const char *cls;
    const char *tags[24];
} Class;

/* Etiquetas de la comunidad de Steam que definen cada clase. */
static const Class classes[] = {
    { "racing", { "Racing", "Driving", "Automobile Sim", "Motorbike", "Offroad", "Motocross",
                  "Combat Racing", "Racing Sim", NULL } },
    { "fps", { "FPS", "Shooter", "Tactical", "Arena Shooter", "Hero Shooter", "Looter Shooter",
               "Battle Royale", "Third-Person Shooter", "Boomer Shooter", "Extraction Shooter",
               "Military", "War", NULL } },
    { "adventure", { "RPG", "Action RPG", "Open World", "Adventure", "Story Rich", "Souls-like",
                     "JRPG", "CRPG", "Survival Horror", "Horror", "Exploration", "Fantasy",
                     "Action-Adventure", "Metroidvania", "Narration", "Choices Matter", NULL } },
    { "gaming", { "Strategy", "Simulation", "Farming Sim", "Life Sim", "Relaxing", "Puzzle",
                  "Casual", "City Builder", "Turn-Based Strategy", "RTS", "Sports", "Fighting",
                  "Platformer", "Roguelike", "Card Game", "Management", "Sandbox", "Building",
                  NULL } },
};

static int classify_tags(JsonObject *tags, char *cls, size_t n)
{
    double score[4] = { 0, 0, 0, 0 };
    size_t c;
    int k, best = -1;

    for (c = 0; c < 4; c++)
        for (k = 0; classes[c].tags[k]; k++)
            if (json_object_has_member(tags, classes[c].tags[k]))
                score[c] += (double)json_object_get_int_member(tags, classes[c].tags[k]);
    for (c = 0; c < 4; c++)
        if (score[c] > 0 && (best < 0 || score[c] > score[best]))
            best = (int)c;
    if (best < 0)
        return -1;
    snprintf(cls, n, "%s", classes[best].cls);
    return 0;
}

/* Si no hay etiquetas: usar el campo "genre" ("Action, Racing, ...") */
static int classify_genre_string(const char *g, char *cls, size_t n)
{
    if (!g || !*g)
        return -1;
    if (strcasestr(g, "Racing"))
        snprintf(cls, n, "racing");
    else if (strcasestr(g, "RPG") || strcasestr(g, "Adventure"))
        snprintf(cls, n, "adventure");
    else if (strcasestr(g, "Action"))
        snprintf(cls, n, "gaming");
    else
        snprintf(cls, n, "gaming");
    return 0;
}

int atlas_steam_classify_json(const char *json, char *cls, size_t n, char *name, size_t nn)
{
    JsonParser *p = json_parser_new();
    JsonObject *o;
    int r = -1;

    cls[0] = '\0';
    if (name && nn)
        name[0] = '\0';
    if (!json || !json_parser_load_from_data(p, json, -1, NULL) ||
        !JSON_NODE_HOLDS_OBJECT(json_parser_get_root(p))) {
        g_object_unref(p);
        return -1;
    }
    o = json_node_get_object(json_parser_get_root(p));
    if (name && nn && json_object_has_member(o, "name") &&
        JSON_NODE_TYPE(json_object_get_member(o, "name")) == JSON_NODE_VALUE)
        snprintf(name, nn, "%s", json_object_get_string_member(o, "name"));
    /* SteamSpy devuelve "tags": [] (array vacío) cuando no hay datos */
    if (json_object_has_member(o, "tags") && JSON_NODE_HOLDS_OBJECT(json_object_get_member(o, "tags")))
        r = classify_tags(json_object_get_object_member(o, "tags"), cls, n);
    if (r != 0 && json_object_has_member(o, "genre") &&
        JSON_NODE_TYPE(json_object_get_member(o, "genre")) == JSON_NODE_VALUE)
        r = classify_genre_string(json_object_get_string_member(o, "genre"), cls, n);
    g_object_unref(p);
    return r;
}

/* Etiquetas de la página de la tienda: InitAppTagModal( <id>, [ {"name":..,"count":..}, ... ] */
int atlas_steam_classify_store_html(const char *html, char *cls, size_t n)
{
    const char *p, *s, *e;
    int depth = 0, r = -1;
    JsonParser *jp;

    cls[0] = '\0';
    if (!html || !(p = strstr(html, "InitAppTagModal(")))
        return -1;
    s = strchr(p, '[');
    if (!s)
        return -1;
    for (e = s; *e; e++) {
        if (*e == '[')
            depth++;
        else if (*e == ']' && --depth == 0)
            break;
    }
    if (!*e)
        return -1;

    jp = json_parser_new();
    {
        char *arr = g_strndup(s, (gsize)(e - s + 1));
        if (json_parser_load_from_data(jp, arr, -1, NULL) && JSON_NODE_HOLDS_ARRAY(json_parser_get_root(jp))) {
            JsonArray *a = json_node_get_array(json_parser_get_root(jp));
            JsonObject *tags = json_object_new();
            guint i;
            for (i = 0; i < json_array_get_length(a); i++) {
                JsonObject *t = json_array_get_object_element(a, i);
                if (t && json_object_has_member(t, "name") && json_object_has_member(t, "count"))
                    json_object_set_int_member(tags, json_object_get_string_member(t, "name"),
                                               json_object_get_int_member(t, "count"));
            }
            r = classify_tags(tags, cls, n);
            json_object_unref(tags);
        }
        g_free(arr);
    }
    g_object_unref(jp);
    return r;
}

/* ================================================================ caché */

static void cache_dir(char *buf, size_t n)
{
    const char *x = getenv("ATLAS_CACHE_DIR");
    const char *h = getenv("HOME");
    if (x && *x)
        snprintf(buf, n, "%s/steam", x);
    else if ((x = getenv("XDG_CACHE_HOME")) && *x)
        snprintf(buf, n, "%s/audio-profile/steam", x);
    else
        snprintf(buf, n, "%s/.cache/audio-profile/steam", h ? h : "/tmp");
}

static int appid_ok(const char *a)
{
    size_t i;
    if (!a || !*a || strlen(a) > 12)
        return 0;
    for (i = 0; a[i]; i++)
        if (!isdigit((unsigned char)a[i]))
            return 0;
    return 1;
}

static int read_cache(const char *path, char *cls, size_t n, long *age)
{
    char *s = atlas_read_file(path, 65536);
    JsonParser *p;
    int r = -1;

    if (!s)
        return -1;
    p = json_parser_new();
    if (json_parser_load_from_data(p, s, -1, NULL) && JSON_NODE_HOLDS_OBJECT(json_parser_get_root(p))) {
        JsonObject *o = json_node_get_object(json_parser_get_root(p));
        long t = json_object_has_member(o, "time") ? (long)json_object_get_int_member(o, "time") : 0;
        *age = (long)time(NULL) - t;
        snprintf(cls, n, "%s", json_object_has_member(o, "class") ? json_object_get_string_member(o, "class") : "");
        r = 0;
    }
    g_object_unref(p);
    free(s);
    return r;
}

static void write_cache(const char *path, const char *appid, const char *cls, const char *name)
{
    JsonBuilder *b = json_builder_new();
    JsonGenerator *g = json_generator_new();
    JsonNode *root;
    gchar *txt;

    json_builder_begin_object(b);
    atlas_json_kv_str(b, "appid", appid);
    atlas_json_kv_str(b, "class", cls);
    atlas_json_kv_str(b, "name", name);
    atlas_json_kv_int(b, "time", (long long)time(NULL));
    json_builder_end_object(b);
    root = json_builder_get_root(b);
    json_generator_set_root(g, root);
    json_generator_set_pretty(g, TRUE);
    txt = json_generator_to_data(g, NULL);
    atlas_write_file_atomic(path, txt);
    g_free(txt);
    json_node_unref(root);
    g_object_unref(g);
    g_object_unref(b);
}

static int lookup(const char *appid, char *cls, size_t n, char *name, size_t nn, char **raw)
{
    char url[256], *out = NULL;
    int r;

    cls[0] = '\0';
    name[0] = '\0';   /* la tienda no da nombre: si solo responde ella, queda vacío */
    snprintf(url, sizeof(url), "https://steamspy.com/api.php?request=appdetails&appid=%s", appid);
    r = atlas_runl(&out, 9000, "curl", "-fsS", "-m", "8", "-A", "audio-profile/" ATLAS_VERSION, url, NULL);
    if (r == 0) {
        /* las etiquetas con votos son lo fiable; el "genre" genérico solo es último recurso */
        JsonParser *p = json_parser_new();
        int has_tags = 0;
        if (json_parser_load_from_data(p, out, -1, NULL) && JSON_NODE_HOLDS_OBJECT(json_parser_get_root(p))) {
            JsonObject *o = json_node_get_object(json_parser_get_root(p));
            has_tags = json_object_has_member(o, "tags") &&
                       JSON_NODE_HOLDS_OBJECT(json_object_get_member(o, "tags")) &&
                       json_object_get_size(json_object_get_object_member(o, "tags")) > 0;
        }
        g_object_unref(p);
        r = atlas_steam_classify_json(out, cls, n, name, nn);
        if (has_tags && r == 0)
            goto done;
    }
    /* segunda fuente: etiquetas de la página de la tienda de Steam */
    {
        char *html = NULL, store[160], fallback[32];
        snprintf(fallback, sizeof(fallback), "%s", r == 0 ? cls : "");
        snprintf(store, sizeof(store), "https://store.steampowered.com/app/%s/?l=english", appid);
        if (atlas_runl(&html, 12000, "curl", "-fsSL", "-m", "10", "-A", "audio-profile/" ATLAS_VERSION,
                       "-b", "birthtime=0; lastagecheckage=1-0-1990; wants_mature_content=1",
                       store, NULL) == 0 && atlas_steam_classify_store_html(html, cls, n) == 0)
            r = 0;
        else if (fallback[0]) {
            snprintf(cls, n, "%s", fallback);
            r = 0;
        } else {
            r = -1;
        }
        free(html);
    }
done:
    if (raw)
        *raw = out;
    else
        free(out);
    return r;
}

/*
 * El daemon no puede esperar a la red: una consulta tarda hasta ~20 s y, si
 * el equipo se suspende mientras, no llegaría a cerrar el audio a tiempo
 * (BUG-001). En modo segundo plano, si falta la caché se lanza
 * `audio-profile steam-genre <id>` como proceso aparte, que la escribe; el
 * daemon la leerá en una pasada siguiente.
 */
#define BACKGROUND_RETRY_MS (5 * 60 * 1000)
#define MAX_PENDING         16

static char background_cli[PATH_MAX];   /* "" = consultar en el momento */

void atlas_steam_set_background_lookups(const char *cli_path)
{
    snprintf(background_cli, sizeof(background_cli), "%s", cli_path ? cli_path : "");
}

static void lookup_in_background(const char *appid)
{
    static char pending[MAX_PENDING][24];
    static long long launched[MAX_PENDING];
    static int next;
    char subcommand[] = "steam-genre", id[24];
    gchar *argv[] = { background_cli, subcommand, id, NULL };   /* GLib pide char*, no const */
    int i;

    snprintf(id, sizeof(id), "%s", appid);

    for (i = 0; i < MAX_PENDING; i++)
        if (!strcmp(pending[i], appid) && atlas_now_ms() - launched[i] < BACKGROUND_RETRY_MS)
            return;   /* ya se está consultando (o se intentó hace poco) */
    /* sin G_SPAWN_DO_NOT_REAP_CHILD, GLib recoge el proceso al terminar */
    if (!g_spawn_async(NULL, argv, NULL, G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL,
                       NULL, NULL, NULL, NULL))
        return;
    snprintf(pending[next], sizeof(pending[0]), "%s", appid);
    launched[next] = atlas_now_ms();
    next = (next + 1) % MAX_PENDING;
}

int atlas_steam_genre(const char *appid, char *cls, size_t n)
{
    char dir[PATH_MAX], path[PATH_MAX + 32], name[160];
    long age = 0;

    cls[0] = '\0';
    if (!appid_ok(appid))
        return -1;
    cache_dir(dir, sizeof(dir));
    snprintf(path, sizeof(path), "%s/%s.json", dir, appid);

    if (read_cache(path, cls, n, &age) == 0) {
        /* éxito: válido 90 días; fallo (clase vacía): reintentar tras 1 día */
        if ((cls[0] && age < 90L * 86400) || (!cls[0] && age < 86400))
            return cls[0] ? 0 : -1;
    }
    if (!atlas_conf_bool("auto.steam_lookup") || !atlas_have_cmd("curl"))
        return -1;
    if (background_cli[0]) {
        cls[0] = '\0';
        lookup_in_background(appid);
        return -1;
    }
    atlas_mkdirs(dir);
    if (lookup(appid, cls, n, name, sizeof(name), NULL) != 0) {
        cls[0] = '\0';
        write_cache(path, appid, "", "");
        return -1;
    }
    write_cache(path, appid, cls, name);
    return 0;
}

int atlas_cmd_steam_genre(int argc, char **argv)
{
    char dir[PATH_MAX], path[PATH_MAX + 32], cls[32], name[160];
    char *raw = NULL;

    if (argc < 1 || !appid_ok(argv[0])) {
        fprintf(stderr, "Uso: audio-profile steam-genre <appid> [--refresh]\n");
        return 2;
    }
    cache_dir(dir, sizeof(dir));
    snprintf(path, sizeof(path), "%s/%s.json", dir, argv[0]);
    if (argc > 1 && !strcmp(argv[1], "--refresh"))
        remove(path);

    atlas_mkdirs(dir);
    if (lookup(argv[0], cls, sizeof(cls), name, sizeof(name), &raw) != 0) {
        long age = 0;
        if (read_cache(path, cls, sizeof(cls), &age) == 0 && cls[0]) {
            printf("%s → %s (de la caché; sin red)\n", argv[0], cls);
        } else {
            write_cache(path, argv[0], "", "");   /* reintento en 1 día, no en cada pasada */
            printf("%s → sin clasificar (sin red o sin etiquetas)\n", argv[0]);
        }
        free(raw);
        return 1;
    }
    write_cache(path, argv[0], cls, name);
    if (atlas_json_mode) {
        JsonBuilder *b = json_builder_new();
        json_builder_begin_object(b);
        atlas_json_kv_str(b, "appid", argv[0]);
        atlas_json_kv_str(b, "name", name);
        atlas_json_kv_str(b, "class", cls);
        json_builder_end_object(b);
        atlas_json_print(b);
    } else {
        printf("%s · %s → perfil %s\n", argv[0], name[0] ? name : "?", cls);
        printf("(para fijar otro: audio-profile auto assign <perfil> steam=%s)\n", argv[0]);
    }
    free(raw);
    return 0;
}
