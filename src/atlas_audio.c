#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "atlas.h"
#include "atlas_audio.h"

#define PACTL_TIMEOUT 4000

static void cpy(char *dst, size_t n, const char *src)
{
    snprintf(dst, n, "%s", src ? src : "");
}

static const char *obj_str(JsonObject *o, const char *k)
{
    JsonNode *n;
    if (!o || !json_object_has_member(o, k))
        return "";
    n = json_object_get_member(o, k);
    if (!n || JSON_NODE_TYPE(n) != JSON_NODE_VALUE)
        return "";
    if (json_node_get_value_type(n) != G_TYPE_STRING)
        return "";
    return json_node_get_string(n);
}

static long long obj_int(JsonObject *o, const char *k, long long def)
{
    JsonNode *n;
    if (!o || !json_object_has_member(o, k))
        return def;
    n = json_object_get_member(o, k);
    if (!n || JSON_NODE_TYPE(n) != JSON_NODE_VALUE)
        return def;
    if (json_node_get_value_type(n) == G_TYPE_STRING)
        return atoll(json_node_get_string(n));
    if (json_node_get_value_type(n) == G_TYPE_BOOLEAN)
        return json_node_get_boolean(n);
    return json_node_get_int(n);
}

static int obj_bool(JsonObject *o, const char *k)
{
    JsonNode *n;
    if (!o || !json_object_has_member(o, k))
        return 0;
    n = json_object_get_member(o, k);
    if (!n || JSON_NODE_TYPE(n) != JSON_NODE_VALUE)
        return 0;
    if (json_node_get_value_type(n) == G_TYPE_BOOLEAN)
        return json_node_get_boolean(n);
    if (json_node_get_value_type(n) == G_TYPE_STRING)
        return !strcmp(json_node_get_string(n), "yes") || !strcmp(json_node_get_string(n), "true");
    return json_node_get_int(n) != 0;
}

static JsonObject *obj_obj(JsonObject *o, const char *k)
{
    JsonNode *n;
    if (!o || !json_object_has_member(o, k))
        return NULL;
    n = json_object_get_member(o, k);
    return (n && JSON_NODE_HOLDS_OBJECT(n)) ? json_node_get_object(n) : NULL;
}

static JsonParser *parse(const char *json, JsonNode **root)
{
    JsonParser *p = json_parser_new();
    if (!json || !json_parser_load_from_data(p, json, -1, NULL)) {
        g_object_unref(p);
        return NULL;
    }
    *root = json_parser_get_root(p);
    return p;
}

/* ============================================================ parsers */

int atlas_audio_parse_info(AtlasAudio *a, const char *json)
{
    JsonNode *root;
    JsonParser *p = parse(json, &root);
    JsonObject *o;

    if (!p)
        return -1;
    if (!JSON_NODE_HOLDS_OBJECT(root)) {
        g_object_unref(p);
        return -1;
    }
    o = json_node_get_object(root);
    cpy(a->server_name, sizeof(a->server_name), obj_str(o, "server_name"));
    cpy(a->default_sink, sizeof(a->default_sink), obj_str(o, "default_sink_name"));
    cpy(a->default_source, sizeof(a->default_source), obj_str(o, "default_source_name"));
    cpy(a->sample_spec, sizeof(a->sample_spec), obj_str(o, "default_sample_specification"));
    g_object_unref(p);
    return a->server_name[0] ? 0 : -1;
}

int atlas_audio_parse_cards(AtlasAudio *a, const char *json)
{
    JsonNode *root;
    JsonParser *p = parse(json, &root);
    JsonArray *arr;
    guint i;

    a->ncards = 0;
    if (!p)
        return -1;
    if (!JSON_NODE_HOLDS_ARRAY(root)) {
        g_object_unref(p);
        return -1;
    }
    arr = json_node_get_array(root);

    for (i = 0; i < json_array_get_length(arr) && a->ncards < ATLAS_MAX_CARDS; i++) {
        JsonObject *o = json_array_get_object_element(arr, i);
        JsonObject *props = obj_obj(o, "properties");
        JsonObject *profs = obj_obj(o, "profiles");
        AtlasCard *c = &a->cards[a->ncards];
        const char *n;

        memset(c, 0, sizeof(*c));
        cpy(c->name, sizeof(c->name), obj_str(o, "name"));
        cpy(c->active_profile, sizeof(c->active_profile), obj_str(o, "active_profile"));
        cpy(c->description, sizeof(c->description), obj_str(props, "device.description"));
        if (!c->description[0])
            cpy(c->description, sizeof(c->description), obj_str(props, "device.product.name"));
        cpy(c->bus, sizeof(c->bus), obj_str(props, "device.bus"));
        cpy(c->vendor_id, sizeof(c->vendor_id), obj_str(props, "device.vendor.id"));
        cpy(c->product_id, sizeof(c->product_id), obj_str(props, "device.product.id"));
        cpy(c->serial, sizeof(c->serial), obj_str(props, "device.serial"));
        cpy(c->bus_path, sizeof(c->bus_path), obj_str(props, "device.bus_path"));
        n = obj_str(props, "alsa.card");
        if (!*n)
            n = obj_str(props, "api.alsa.card");
        c->alsa_card = *n ? atoi(n) : -1;

        if (profs) {
            GList *members = json_object_get_members(profs), *l;
            for (l = members; l && c->nprofiles < ATLAS_MAX_PROFILES; l = l->next) {
                JsonObject *po = obj_obj(profs, l->data);
                AtlasCardProfile *cp = &c->profiles[c->nprofiles++];
                cpy(cp->name, sizeof(cp->name), l->data);
                cpy(cp->description, sizeof(cp->description), obj_str(po, "description"));
                cp->sinks = (int)obj_int(po, "sinks", 0);
                cp->sources = (int)obj_int(po, "sources", 0);
                cp->available = po ? obj_bool(po, "available") : 1;
            }
            g_list_free(members);
        }
        a->ncards++;
    }
    g_object_unref(p);
    return 0;
}

int atlas_audio_parse_nodes(AtlasNode *out, int max, int *count, const char *json)
{
    JsonNode *root;
    JsonParser *p = parse(json, &root);
    JsonArray *arr;
    guint i;

    *count = 0;
    if (!p)
        return -1;
    if (!JSON_NODE_HOLDS_ARRAY(root)) {
        g_object_unref(p);
        return -1;
    }
    arr = json_node_get_array(root);

    for (i = 0; i < json_array_get_length(arr) && *count < max; i++) {
        JsonObject *o = json_array_get_object_element(arr, i);
        JsonObject *props = obj_obj(o, "properties");
        JsonObject *vol = obj_obj(o, "volume");
        AtlasNode *nd = &out[*count];
        const char *s;

        memset(nd, 0, sizeof(*nd));
        cpy(nd->name, sizeof(nd->name), obj_str(o, "name"));
        cpy(nd->description, sizeof(nd->description), obj_str(o, "description"));
        cpy(nd->state, sizeof(nd->state), obj_str(o, "state"));
        cpy(nd->api, sizeof(nd->api), obj_str(props, "device.api"));
        nd->mute = obj_bool(o, "mute");
        nd->is_monitor = !strcmp(obj_str(props, "device.class"), "monitor") ||
                         (obj_str(o, "monitor_of_sink")[0] != '\0');

        s = obj_str(props, "alsa.card");
        if (!*s)
            s = obj_str(props, "api.alsa.pcm.card");
        nd->alsa_card = *s ? atoi(s) : -1;
        s = obj_str(props, "alsa.device");
        nd->alsa_device = *s ? atoi(s) : (nd->alsa_card >= 0 ? 0 : -1);

        if (nd->alsa_card >= 0) {
            /* alsa_output.pci-0000_27_00.0.analog-surround-51 -> alsa_card.pci-0000_27_00.0 */
            const char *dot;
            s = nd->name;
            if (!strncmp(s, "alsa_output.", 12) || !strncmp(s, "alsa_input.", 11)) {
                const char *rest = strchr(s, '.') + 1;
                dot = strrchr(rest, '.');
                if (dot)
                    snprintf(nd->card_name, sizeof(nd->card_name), "alsa_card.%.*s",
                             (int)(dot - rest), rest);
            }
        }

        if (vol) {
            /* el orden importa: pactl set-sink-volume asigna valores en el
             * orden del mapa de canales; solo el iterador "ordered" conserva
             * el orden del JSON (get_members/iter_init usan la tabla hash) */
            JsonObjectIter it;
            const gchar *ch;
            JsonNode *chn;
            json_object_iter_init_ordered(&it, vol);
            while (json_object_iter_next_ordered(&it, &ch, &chn) && nd->volume.channels < ATLAS_MAX_CH)
                if (JSON_NODE_HOLDS_OBJECT(chn))
                    nd->volume.value[nd->volume.channels++] =
                        (unsigned)obj_int(json_node_get_object(chn), "value", 0);
        }
        (*count)++;
    }
    g_object_unref(p);
    return 0;
}

int atlas_audio_parse_inputs(AtlasAudio *a, const char *json)
{
    JsonNode *root;
    JsonParser *p = parse(json, &root);
    JsonArray *arr;
    guint i;

    a->playback_streams = a->playback_streams_active = 0;
    if (!p)
        return -1;
    if (!JSON_NODE_HOLDS_ARRAY(root)) {
        g_object_unref(p);
        return -1;
    }
    arr = json_node_get_array(root);
    for (i = 0; i < json_array_get_length(arr); i++) {
        JsonObject *o = json_array_get_object_element(arr, i);
        a->playback_streams++;
        if (!obj_bool(o, "corked"))
            a->playback_streams_active++;
    }
    g_object_unref(p);
    return 0;
}

/* ============================================================== query */

int atlas_audio_query(AtlasAudio *a)
{
    char *out = NULL;
    int ok;

    memset(a, 0, sizeof(*a));

    if (atlas_runl(&out, PACTL_TIMEOUT, "pactl", "-f", "json", "info", NULL) != 0 ||
        atlas_audio_parse_info(a, out) != 0) {
        free(out);
        a->server_ok = 0;
        return -1;
    }
    free(out);
    a->server_ok = 1;

    ok = atlas_runl(&out, PACTL_TIMEOUT, "pactl", "-f", "json", "list", "cards", NULL) == 0;
    if (ok)
        atlas_audio_parse_cards(a, out);
    free(out);

    ok = atlas_runl(&out, PACTL_TIMEOUT, "pactl", "-f", "json", "list", "sinks", NULL) == 0;
    if (ok)
        atlas_audio_parse_nodes(a->sinks, ATLAS_MAX_NODES, &a->nsinks, out);
    free(out);

    ok = atlas_runl(&out, PACTL_TIMEOUT, "pactl", "-f", "json", "list", "sources", NULL) == 0;
    if (ok)
        atlas_audio_parse_nodes(a->sources, ATLAS_MAX_NODES, &a->nsources, out);
    free(out);

    ok = atlas_runl(&out, PACTL_TIMEOUT, "pactl", "-f", "json", "list", "sink-inputs", NULL) == 0;
    if (ok)
        atlas_audio_parse_inputs(a, out);
    free(out);

    ok = atlas_runl(&out, PACTL_TIMEOUT, "pactl", "-f", "json", "list", "source-outputs", NULL) == 0;
    if (ok) {
        AtlasAudio tmp;
        atlas_audio_parse_inputs(&tmp, out);
        a->capture_streams = tmp.playback_streams;
    }
    free(out);

    return 0;
}

/* ============================================================ lookups */

static const AtlasNode *find_node(const AtlasNode *arr, int n, const char *name)
{
    int i;
    if (!name || !*name)
        return NULL;
    for (i = 0; i < n; i++)
        if (!strcmp(arr[i].name, name))
            return &arr[i];
    return NULL;
}

const AtlasNode *atlas_audio_find_sink(const AtlasAudio *a, const char *name)
{
    return find_node(a->sinks, a->nsinks, name);
}

const AtlasNode *atlas_audio_find_source(const AtlasAudio *a, const char *name)
{
    return find_node(a->sources, a->nsources, name);
}

const AtlasNode *atlas_audio_default_sink(const AtlasAudio *a)
{
    return atlas_audio_find_sink(a, a->default_sink);
}

const AtlasNode *atlas_audio_default_source(const AtlasAudio *a)
{
    return atlas_audio_find_source(a, a->default_source);
}

const AtlasCard *atlas_audio_find_card(const AtlasAudio *a, const char *name)
{
    int i;
    for (i = 0; name && i < a->ncards; i++)
        if (!strcmp(a->cards[i].name, name))
            return &a->cards[i];
    return NULL;
}

const AtlasCard *atlas_audio_card_by_alsa(const AtlasAudio *a, int alsa_card)
{
    int i;
    for (i = 0; alsa_card >= 0 && i < a->ncards; i++)
        if (a->cards[i].alsa_card == alsa_card)
            return &a->cards[i];
    return NULL;
}

double atlas_volume_percent(const AtlasVolume *v)
{
    double sum = 0;
    int i;
    if (!v || v->channels == 0)
        return -1;
    for (i = 0; i < v->channels; i++)
        sum += v->value[i];
    return sum / v->channels * 100.0 / 65536.0;
}

/* ============================================================ acciones */

int atlas_audio_set_card_profile(const char *card, const char *profile)
{
    return atlas_runl(NULL, PACTL_TIMEOUT, "pactl", "set-card-profile", card, profile, NULL);
}

int atlas_audio_cycle_card(const char *card, const char *profile)
{
    int r;
    if (!card || !*card || !profile || !*profile || !strcmp(profile, "off"))
        return -1;
    r = atlas_audio_set_card_profile(card, "off");
    if (r != 0)
        return r;
    atlas_msleep(800);
    return atlas_audio_set_card_profile(card, profile);
}

int atlas_audio_set_default_sink(const char *name)
{
    return atlas_runl(NULL, PACTL_TIMEOUT, "pactl", "set-default-sink", name, NULL);
}

int atlas_audio_set_default_source(const char *name)
{
    return atlas_runl(NULL, PACTL_TIMEOUT, "pactl", "set-default-source", name, NULL);
}

int atlas_audio_set_volume(int is_sink, const char *name, const AtlasVolume *v)
{
    char vals[ATLAS_MAX_CH][16];
    const char *argv[ATLAS_MAX_CH + 5];
    int i, n = 0;

    if (!v || v->channels <= 0)
        return -1;
    argv[n++] = "pactl";
    argv[n++] = is_sink ? "set-sink-volume" : "set-source-volume";
    argv[n++] = name;
    for (i = 0; i < v->channels; i++) {
        snprintf(vals[i], sizeof(vals[i]), "%u", v->value[i]);
        argv[n++] = vals[i];
    }
    argv[n] = NULL;
    return atlas_run(argv, NULL, PACTL_TIMEOUT, 0);
}

int atlas_audio_set_volume_percent(int is_sink, const char *name, int percent)
{
    char v[16];
    if (percent < 0 || percent > 150)
        return -1;
    snprintf(v, sizeof(v), "%d%%", percent);
    return atlas_runl(NULL, PACTL_TIMEOUT, "pactl",
                      is_sink ? "set-sink-volume" : "set-source-volume", name, v, NULL);
}

int atlas_audio_set_mute(int is_sink, const char *name, int mute)
{
    return atlas_runl(NULL, PACTL_TIMEOUT, "pactl",
                      is_sink ? "set-sink-mute" : "set-source-mute", name, mute ? "1" : "0", NULL);
}

int atlas_audio_wait_ready(const char *sink_name, int timeout_ms)
{
    long long deadline = atlas_now_ms() + timeout_ms;
    AtlasAudio *a = malloc(sizeof(*a));
    int ready = 0;

    if (!a)
        return -1;
    while (atlas_now_ms() < deadline) {
        if (atlas_audio_query(a) == 0 &&
            (!sink_name || !*sink_name || atlas_audio_find_sink(a, sink_name))) {
            ready = 1;
            break;
        }
        atlas_msleep(300);
    }
    free(a);
    return ready ? 0 : -1;
}

/* Añade texto con formato al final de `log` sin pasarse de `n`. */
static void logf_append(char *log, size_t n, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
static void logf_append(char *log, size_t n, const char *fmt, ...)
{
    va_list ap;
    size_t l;

    if (!log || n == 0)
        return;
    l = strlen(log);
    if (l + 1 >= n)
        return;
    va_start(ap, fmt);
    vsnprintf(log + l, n - l, fmt, ap);
    va_end(ap);
}

static int volume_differs(const AtlasVolume *a, const AtlasVolume *b)
{
    int i;
    if (a->channels != b->channels)
        return 1;
    for (i = 0; i < a->channels; i++) {
        long d = (long)a->value[i] - (long)b->value[i];
        if (d > 64 || d < -64)
            return 1;
    }
    return 0;
}

int atlas_audio_restore(const AtlasAudio *snap, int restore_volumes, char *log, size_t logn)
{
    AtlasAudio *cur = malloc(sizeof(*cur));
    int i, actions = 0, changed_profiles = 0;

    if (log && logn)
        log[0] = '\0';
    if (!cur)
        return -1;
    if (atlas_audio_query(cur) != 0) {
        free(cur);
        logf_append(log, logn, "%s", "pipewire-pulse no responde; nada que restaurar\n");
        return -1;
    }

    /* 1. Perfiles de tarjeta */
    for (i = 0; i < snap->ncards; i++) {
        const AtlasCard *sc = &snap->cards[i];
        const AtlasCard *cc = atlas_audio_find_card(cur, sc->name);
        if (!cc || !sc->active_profile[0] || !strcmp(sc->active_profile, cc->active_profile))
            continue;
        if (atlas_audio_set_card_profile(sc->name, sc->active_profile) == 0) {
            logf_append(log, logn, "perfil %s", sc->name);
            logf_append(log, logn, " -> %s\n", sc->active_profile);
            actions++;
            changed_profiles = 1;
        }
    }
    if (changed_profiles) {
        atlas_msleep(1000);
        atlas_audio_query(cur);
    }

    /* 2. Defaults */
    if (snap->default_sink[0] && strcmp(snap->default_sink, cur->default_sink) &&
        atlas_audio_find_sink(cur, snap->default_sink) &&
        atlas_audio_set_default_sink(snap->default_sink) == 0) {
        logf_append(log, logn, "sink por defecto -> %s\n", snap->default_sink);
        actions++;
    }
    if (snap->default_source[0] && strcmp(snap->default_source, cur->default_source) &&
        atlas_audio_find_source(cur, snap->default_source) &&
        atlas_audio_set_default_source(snap->default_source) == 0) {
        logf_append(log, logn, "source por defecto -> %s\n", snap->default_source);
        actions++;
    }

    /* 3. Volumen y mute (solo nodos que existen y difieren) */
    if (restore_volumes) {
        int pass;
        for (pass = 0; pass < 2; pass++) {
            const AtlasNode *arr = pass == 0 ? snap->sinks : snap->sources;
            int n = pass == 0 ? snap->nsinks : snap->nsources;
            for (i = 0; i < n; i++) {
                const AtlasNode *sn = &arr[i];
                const AtlasNode *cn = pass == 0 ? atlas_audio_find_sink(cur, sn->name)
                                                : atlas_audio_find_source(cur, sn->name);
                if (!cn || sn->is_monitor)
                    continue;
                if (sn->volume.channels > 0 && volume_differs(&sn->volume, &cn->volume) &&
                    atlas_audio_set_volume(pass == 0, sn->name, &sn->volume) == 0) {
                    logf_append(log, logn, "volumen %s\n", sn->name);
                    actions++;
                }
                if (sn->mute != cn->mute && atlas_audio_set_mute(pass == 0, sn->name, sn->mute) == 0) {
                    logf_append(log, logn, "mute %s\n", sn->name);
                    actions++;
                }
            }
        }
    }

    free(cur);
    return actions;
}

/* =============================================================== JSON */

static void node_json(JsonBuilder *b, const AtlasNode *n, const char *def)
{
    int i;
    json_builder_begin_object(b);
    atlas_json_kv_str(b, "name", n->name);
    atlas_json_kv_str(b, "description", n->description);
    atlas_json_kv_str(b, "state", n->state);
    atlas_json_kv_str(b, "api", n->api);
    atlas_json_kv_str(b, "card", n->card_name);
    atlas_json_kv_int(b, "alsa_card", n->alsa_card);
    atlas_json_kv_int(b, "alsa_device", n->alsa_device);
    atlas_json_kv_bool(b, "mute", n->mute);
    atlas_json_kv_bool(b, "monitor", n->is_monitor);
    atlas_json_kv_bool(b, "default", def && !strcmp(def, n->name));
    atlas_json_kv_double(b, "volume_percent", atlas_volume_percent(&n->volume));
    json_builder_set_member_name(b, "volume_raw");
    json_builder_begin_array(b);
    for (i = 0; i < n->volume.channels; i++)
        json_builder_add_int_value(b, n->volume.value[i]);
    json_builder_end_array(b);
    json_builder_end_object(b);
}

void atlas_audio_json(JsonBuilder *b, const AtlasAudio *a)
{
    int i, j;

    json_builder_begin_object(b);
    atlas_json_kv_bool(b, "server_ok", a->server_ok);
    atlas_json_kv_str(b, "server_name", a->server_name);
    atlas_json_kv_str(b, "default_sink", a->default_sink);
    atlas_json_kv_str(b, "default_source", a->default_source);
    atlas_json_kv_str(b, "sample_spec", a->sample_spec);
    atlas_json_kv_int(b, "playback_streams", a->playback_streams);
    atlas_json_kv_int(b, "playback_streams_active", a->playback_streams_active);
    atlas_json_kv_int(b, "capture_streams", a->capture_streams);

    json_builder_set_member_name(b, "cards");
    json_builder_begin_array(b);
    for (i = 0; i < a->ncards; i++) {
        const AtlasCard *c = &a->cards[i];
        json_builder_begin_object(b);
        atlas_json_kv_str(b, "name", c->name);
        atlas_json_kv_str(b, "description", c->description);
        atlas_json_kv_str(b, "active_profile", c->active_profile);
        atlas_json_kv_str(b, "bus", c->bus);
        atlas_json_kv_str(b, "vendor_id", c->vendor_id);
        atlas_json_kv_str(b, "product_id", c->product_id);
        atlas_json_kv_int(b, "alsa_card", c->alsa_card);
        json_builder_set_member_name(b, "profiles");
        json_builder_begin_array(b);
        for (j = 0; j < c->nprofiles; j++) {
            json_builder_begin_object(b);
            atlas_json_kv_str(b, "name", c->profiles[j].name);
            atlas_json_kv_str(b, "description", c->profiles[j].description);
            atlas_json_kv_int(b, "sinks", c->profiles[j].sinks);
            atlas_json_kv_int(b, "sources", c->profiles[j].sources);
            atlas_json_kv_bool(b, "available", c->profiles[j].available);
            json_builder_end_object(b);
        }
        json_builder_end_array(b);
        json_builder_end_object(b);
    }
    json_builder_end_array(b);

    json_builder_set_member_name(b, "sinks");
    json_builder_begin_array(b);
    for (i = 0; i < a->nsinks; i++)
        node_json(b, &a->sinks[i], a->default_sink);
    json_builder_end_array(b);

    json_builder_set_member_name(b, "sources");
    json_builder_begin_array(b);
    for (i = 0; i < a->nsources; i++)
        if (!a->sources[i].is_monitor)
            node_json(b, &a->sources[i], a->default_source);
    json_builder_end_array(b);

    json_builder_end_object(b);
}

/* ======================================================== instantánea */

int atlas_audio_save_snapshot(const AtlasAudio *a, const char *path)
{
    JsonBuilder *b = json_builder_new();
    JsonGenerator *g = json_generator_new();
    JsonNode *root;
    gchar *s;
    int r;

    atlas_audio_json(b, a);
    root = json_builder_get_root(b);
    json_generator_set_root(g, root);
    json_generator_set_pretty(g, TRUE);
    s = json_generator_to_data(g, NULL);
    r = atlas_write_file_atomic(path, s);
    g_free(s);
    json_node_unref(root);
    g_object_unref(g);
    g_object_unref(b);
    return r;
}

static void load_nodes(JsonObject *o, const char *k, AtlasNode *arr, int *n)
{
    JsonNode *an;
    JsonArray *ja;
    guint i;

    *n = 0;
    if (!o || !json_object_has_member(o, k))
        return;
    an = json_object_get_member(o, k);
    if (!JSON_NODE_HOLDS_ARRAY(an))
        return;
    ja = json_node_get_array(an);
    for (i = 0; i < json_array_get_length(ja) && *n < ATLAS_MAX_NODES; i++) {
        JsonObject *no = json_array_get_object_element(ja, i);
        AtlasNode *nd = &arr[(*n)++];
        JsonNode *vr;
        memset(nd, 0, sizeof(*nd));
        cpy(nd->name, sizeof(nd->name), obj_str(no, "name"));
        cpy(nd->description, sizeof(nd->description), obj_str(no, "description"));
        cpy(nd->card_name, sizeof(nd->card_name), obj_str(no, "card"));
        nd->alsa_card = (int)obj_int(no, "alsa_card", -1);
        nd->alsa_device = (int)obj_int(no, "alsa_device", -1);
        nd->mute = obj_bool(no, "mute");
        nd->is_monitor = obj_bool(no, "monitor");
        if (json_object_has_member(no, "volume_raw") &&
            JSON_NODE_HOLDS_ARRAY(vr = json_object_get_member(no, "volume_raw"))) {
            JsonArray *va = json_node_get_array(vr);
            guint j;
            for (j = 0; j < json_array_get_length(va) && j < ATLAS_MAX_CH; j++)
                nd->volume.value[nd->volume.channels++] = (unsigned)json_array_get_int_element(va, j);
        }
    }
}

int atlas_audio_load_snapshot(AtlasAudio *a, const char *path)
{
    char *data = atlas_read_file(path, 1 << 20);
    JsonNode *root;
    JsonParser *p;
    JsonObject *o;

    memset(a, 0, sizeof(*a));
    if (!data)
        return -1;
    p = parse(data, &root);
    free(data);
    if (!p)
        return -1;
    if (!JSON_NODE_HOLDS_OBJECT(root)) {
        g_object_unref(p);
        return -1;
    }
    o = json_node_get_object(root);
    a->server_ok = obj_bool(o, "server_ok");
    cpy(a->default_sink, sizeof(a->default_sink), obj_str(o, "default_sink"));
    cpy(a->default_source, sizeof(a->default_source), obj_str(o, "default_source"));

    if (json_object_has_member(o, "cards")) {
        JsonArray *ca = json_object_get_array_member(o, "cards");
        guint i;
        for (i = 0; ca && i < json_array_get_length(ca) && a->ncards < ATLAS_MAX_CARDS; i++) {
            JsonObject *co = json_array_get_object_element(ca, i);
            AtlasCard *c = &a->cards[a->ncards++];
            cpy(c->name, sizeof(c->name), obj_str(co, "name"));
            cpy(c->description, sizeof(c->description), obj_str(co, "description"));
            cpy(c->active_profile, sizeof(c->active_profile), obj_str(co, "active_profile"));
            c->alsa_card = (int)obj_int(co, "alsa_card", -1);
        }
    }
    load_nodes(o, "sinks", a->sinks, &a->nsinks);
    load_nodes(o, "sources", a->sources, &a->nsources);
    g_object_unref(p);
    return 0;
}
