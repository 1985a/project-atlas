#define _GNU_SOURCE
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <json-glib/json-glib.h>

#include "atlas.h"
#include "atlas_audio.h"
#include "atlas_auto.h"
#include "atlas_dsp.h"
#include "atlas_health.h"
#include "atlas_output.h"

typedef struct {
    int card;
    char hw_id[64];
    char jack[96];
    char hp_profile[128];
    char spk_profile[128];
    int ok;
} HpConfig;

static const char *jstr(JsonObject *o, const char *k)
{
    if (!o || !json_object_has_member(o, k) ||
        JSON_NODE_TYPE(json_object_get_member(o, k)) != JSON_NODE_VALUE)
        return "";
    return json_object_get_string_member(o, k);
}

static int load_config(HpConfig *c)
{
    char hw_dir[PATH_MAX], path[PATH_MAX + 16], *s;
    JsonParser *p;

    memset(c, 0, sizeof(*c));
    if (atlas_dsp_target(&c->card, c->hw_id, sizeof(c->hw_id), hw_dir, sizeof(hw_dir)) != 0 || !c->hw_id[0])
        return -1;
    snprintf(path, sizeof(path), "%s/card.json", hw_dir);
    s = atlas_read_file(path, 1 << 20);
    if (!s)
        return -1;
    p = json_parser_new();
    if (json_parser_load_from_data(p, s, -1, NULL) && JSON_NODE_HOLDS_OBJECT(json_parser_get_root(p))) {
        JsonObject *o = json_node_get_object(json_parser_get_root(p));
        if (json_object_has_member(o, "headphones") &&
            JSON_NODE_HOLDS_OBJECT(json_object_get_member(o, "headphones"))) {
            JsonObject *h = json_object_get_object_member(o, "headphones");
            snprintf(c->jack, sizeof(c->jack), "%s", jstr(h, "jack"));
            snprintf(c->hp_profile, sizeof(c->hp_profile), "%s", jstr(h, "card_profile"));
            snprintf(c->spk_profile, sizeof(c->spk_profile), "%s", jstr(h, "speakers_card_profile"));
            c->ok = 1;
        }
    }
    g_object_unref(p);
    free(s);
    return c->ok ? 0 : -1;
}

/* ================================================================ estado */

static void state_path(char *buf, size_t n)
{
    snprintf(buf, n, "%s/output.json", atlas_state_dir());
}

static void state_load(char *mode, size_t mn, char *spk, size_t sn)
{
    char path[PATH_MAX], *s;
    JsonParser *p;

    snprintf(mode, mn, "speakers");
    spk[0] = '\0';
    state_path(path, sizeof(path));
    s = atlas_read_file(path, 65536);
    if (!s)
        return;
    p = json_parser_new();
    if (json_parser_load_from_data(p, s, -1, NULL) && JSON_NODE_HOLDS_OBJECT(json_parser_get_root(p))) {
        JsonObject *o = json_node_get_object(json_parser_get_root(p));
        if (*jstr(o, "mode"))
            snprintf(mode, mn, "%s", jstr(o, "mode"));
        snprintf(spk, sn, "%s", jstr(o, "speakers_card_profile"));
    }
    g_object_unref(p);
    free(s);
}

static void state_save(const char *mode, const char *spk, const char *reason)
{
    char path[PATH_MAX], ts[64];
    JsonBuilder *b = json_builder_new();
    JsonGenerator *g = json_generator_new();
    JsonNode *root;
    gchar *txt;

    atlas_timestamp(ts, sizeof(ts), 0);
    json_builder_begin_object(b);
    atlas_json_kv_str(b, "mode", mode);
    atlas_json_kv_str(b, "speakers_card_profile", spk);
    atlas_json_kv_str(b, "reason", reason);
    atlas_json_kv_str(b, "changed", ts);
    json_builder_end_object(b);
    root = json_builder_get_root(b);
    json_generator_set_root(g, root);
    json_generator_set_pretty(g, TRUE);
    txt = json_generator_to_data(g, NULL);
    atlas_mkdirs(atlas_state_dir());
    state_path(path, sizeof(path));
    atlas_write_file_atomic(path, txt);
    g_free(txt);
    json_node_unref(root);
    g_object_unref(g);
    g_object_unref(b);
}

AtlasOutputMode atlas_output_mode(void)
{
    char mode[32], spk[128];
    state_load(mode, sizeof(mode), spk, sizeof(spk));
    return !strcmp(mode, "headphones") ? ATLAS_OUT_HEADPHONES : ATLAS_OUT_SPEAKERS;
}

const char *atlas_output_mode_name(AtlasOutputMode m)
{
    return m == ATLAS_OUT_HEADPHONES ? "headphones" : "speakers";
}

/* ================================================================ cambio */

static int card_has_profile(const AtlasCard *c, const char *p)
{
    int i;
    for (i = 0; c && p && *p && i < c->nprofiles; i++)
        if (!strcmp(c->profiles[i].name, p))
            return c->profiles[i].available;
    return 0;
}

int atlas_output_set(AtlasOutputMode m, const char *reason)
{
    HpConfig cfg;
    AtlasAudio *a = malloc(sizeof(*a));
    const AtlasCard *card;
    char mode[32], spk[128], cur[128], target[128] = "";
    int i, rc = 0;

    if (!a)
        return -1;
    if (load_config(&cfg) != 0) {
        fprintf(stderr, "La tarjeta por defecto no declara salida de auriculares en la Hardware DB.\n");
        free(a);
        return -1;
    }
    if (atlas_audio_query(a) != 0) {
        fprintf(stderr, "pipewire-pulse no responde.\n");
        free(a);
        return -1;
    }
    card = atlas_audio_card_by_alsa(a, cfg.card);
    state_load(mode, sizeof(mode), spk, sizeof(spk));

    if (m == ATLAS_OUT_HEADPHONES) {
        /* recordar el perfil de altavoces actual para volver a él */
        if (card && strcmp(card->active_profile, cfg.hp_profile) && strcmp(card->active_profile, "off"))
            snprintf(spk, sizeof(spk), "%s", card->active_profile);
        if (card_has_profile(card, cfg.hp_profile))
            snprintf(target, sizeof(target), "%s", cfg.hp_profile);
    } else {
        const char *want = spk[0] ? spk : cfg.spk_profile;
        if (card_has_profile(card, want))
            snprintf(target, sizeof(target), "%s", want);
    }
    if (!spk[0])
        snprintf(spk, sizeof(spk), "%s", cfg.spk_profile);

    if (card && target[0] && strcmp(card->active_profile, target)) {
        if (atlas_audio_set_card_profile(card->name, target) != 0) {
            fprintf(stderr, "No pude cambiar el perfil de la tarjeta a %s\n", target);
            rc = -1;
        } else {
            atlas_msleep(900);
            /* el sink cambia de nombre (…analog-stereo / …analog-surround-51): hacerlo predeterminado */
            if (atlas_audio_query(a) == 0)
                for (i = 0; i < a->nsinks; i++)
                    if (!strcmp(a->sinks[i].card_name, card->name) && !a->sinks[i].is_monitor) {
                        atlas_audio_set_default_sink(a->sinks[i].name);
                        break;
                    }
        }
    }
    state_save(atlas_output_mode_name(m), spk, reason ? reason : "manual");

    /* reaplicar el perfil actual con las capas del nuevo modo (base de volumen incluida) */
    atlas_current_profile(cur, sizeof(cur));
    if (strcmp(cur, "ninguno"))
        atlas_apply_profile_id(cur, 1);
    else
        ppm_dsp_apply("volumes");

    if (!atlas_json_mode)
        printf("Salida: %s%s\n", m == ATLAS_OUT_HEADPHONES ? "auriculares" : "altavoces",
               target[0] ? "" : " (perfil de tarjeta sin cambios)");
    free(a);
    return rc;
}

/* ================================================================== jack */

static int read_jack(int card, const char *jack)
{
    char cards[16], name[160], *out = NULL, *p;
    int r = -1;

    if (card < 0 || !jack || !*jack)
        return -1;
    snprintf(cards, sizeof(cards), "%d", card);
    snprintf(name, sizeof(name), "iface=CARD,name=%s", jack);
    if (atlas_runl(&out, 2000, "amixer", "-c", cards, "cget", name, NULL) == 0 && out &&
        (p = strstr(out, ": values=")))
        r = !strncmp(p + 9, "on", 2) ? 1 : 0;
    free(out);
    return r;
}

int atlas_output_jack(void)
{
    HpConfig cfg;
    if (load_config(&cfg) != 0)
        return -1;
    return read_jack(cfg.card, cfg.jack);
}

void atlas_output_tick(void)
{
    static HpConfig cfg;
    static long long loaded;
    static int last = -2;
    int j;

    if (!atlas_conf_bool("output.follow_jack"))
        return;
    /* la configuración se refresca cada minuto (evita consultar pactl cada tick) */
    if (!loaded || atlas_now_ms() - loaded > 60000) {
        if (load_config(&cfg) != 0)
            cfg.ok = 0;
        loaded = atlas_now_ms();
    }
    if (!cfg.ok)
        return;
    j = read_jack(cfg.card, cfg.jack);
    if (j < 0)
        return;
    if (last == -2) {   /* primer valor: solo referencia, sin actuar */
        last = j;
        return;
    }
    if (j == last)
        return;
    last = j;
    printf("jack de auriculares: %s → %s\n", cfg.jack, j ? "conectado" : "desconectado");
    fflush(stdout);
    if (atlas_output_set(j ? ATLAS_OUT_HEADPHONES : ATLAS_OUT_SPEAKERS, "jack") == 0 &&
        atlas_conf_bool("watch.notify") && atlas_have_cmd("notify-send"))
        atlas_runl(NULL, 3000, "notify-send", "-a", "Atlas Audio", "-i",
                   j ? "audio-headphones" : "audio-speakers", "Salida de audio",
                   j ? "Auriculares" : "Altavoces 5.1", NULL);
}

/* =============================================================== comando */

int atlas_cmd_output(int argc, char **argv)
{
    const char *sub = argc > 0 ? argv[0] : "status";
    HpConfig cfg;
    AtlasOutputMode m = atlas_output_mode();
    int jack;

    if (!strcmp(sub, "speakers") || !strcmp(sub, "altavoces"))
        return atlas_output_set(ATLAS_OUT_SPEAKERS, "manual") == 0 ? 0 : 1;
    if (!strcmp(sub, "headphones") || !strcmp(sub, "auriculares"))
        return atlas_output_set(ATLAS_OUT_HEADPHONES, "manual") == 0 ? 0 : 1;
    if (!strcmp(sub, "toggle"))
        return atlas_output_set(m == ATLAS_OUT_HEADPHONES ? ATLAS_OUT_SPEAKERS : ATLAS_OUT_HEADPHONES,
                                "manual") == 0 ? 0 : 1;
    if (strcmp(sub, "status")) {
        fprintf(stderr, "Uso: audio-profile output [status|speakers|headphones|toggle]\n");
        return 2;
    }

    load_config(&cfg);
    jack = cfg.ok ? read_jack(cfg.card, cfg.jack) : -1;
    if (atlas_json_mode) {
        JsonBuilder *b = json_builder_new();
        json_builder_begin_object(b);
        atlas_json_kv_str(b, "mode", atlas_output_mode_name(m));
        atlas_json_kv_bool(b, "supported", cfg.ok);
        atlas_json_kv_str(b, "jack", cfg.jack);
        atlas_json_kv_int(b, "jack_state", jack);
        atlas_json_kv_bool(b, "follow_jack", atlas_conf_bool("output.follow_jack"));
        atlas_json_kv_str(b, "headphones_card_profile", cfg.hp_profile);
        json_builder_end_object(b);
        atlas_json_print(b);
        return 0;
    }
    printf("Salida      : %s\n", m == ATLAS_OUT_HEADPHONES ? "auriculares" : "altavoces");
    if (!cfg.ok) {
        puts("La tarjeta por defecto no declara auriculares en la Hardware DB.");
        return 0;
    }
    printf("Jack        : %s = %s\n", cfg.jack, jack < 0 ? "?" : (jack ? "on" : "off"));
    printf("Automático  : %s (reacciona al enchufar/desenchufar)\n",
           atlas_conf_bool("output.follow_jack") ? "sí" : "no");
    return 0;
}
