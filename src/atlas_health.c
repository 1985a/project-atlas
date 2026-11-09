#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "atlas.h"
#include "atlas_health.h"

static const char *audio_units[] = {
    "pipewire.service", "pipewire-pulse.service", "wireplumber.service", NULL
};

const char *atlas_level_name(AtlasLevel l)
{
    switch (l) {
    case ATLAS_OK: return "ok";
    case ATLAS_WARN: return "warn";
    case ATLAS_FAIL: return "fail";
    default: return "unknown";
    }
}

const char *atlas_level_icon(AtlasLevel l)
{
    switch (l) {
    case ATLAS_OK: return "✓";
    case ATLAS_WARN: return "⚠";
    case ATLAS_FAIL: return "✗";
    default: return "?";
    }
}

static AtlasLevel worst(AtlasLevel a, AtlasLevel b)
{
    /* FAIL > WARN > OK; UNKNOWN no empeora */
    if (a == ATLAS_FAIL || b == ATLAS_FAIL)
        return ATLAS_FAIL;
    if (a == ATLAS_WARN || b == ATLAS_WARN)
        return ATLAS_WARN;
    if (a == ATLAS_OK || b == ATLAS_OK)
        return ATLAS_OK;
    return ATLAS_UNKNOWN;
}

/* ============================================================ systemd */

int atlas_unit_query(const char *unit, AtlasUnit *u)
{
    char *out = NULL;
    char *line, *save = NULL;
    int r;

    memset(u, 0, sizeof(*u));
    snprintf(u->unit, sizeof(u->unit), "%s", unit);
    r = atlas_runl(&out, 4000, "systemctl", "--user", "show", unit,
                   "-p", "LoadState,ActiveState,SubState,Result", NULL);
    if (r != 0 || !out) {
        free(out);
        snprintf(u->active, sizeof(u->active), "unknown");
        return -1;
    }
    for (line = strtok_r(out, "\n", &save); line; line = strtok_r(NULL, "\n", &save)) {
        if (!strncmp(line, "LoadState=", 10))
            u->loaded = !strcmp(line + 10, "loaded");
        else if (!strncmp(line, "ActiveState=", 12))
            snprintf(u->active, sizeof(u->active), "%s", line + 12);
        else if (!strncmp(line, "SubState=", 9))
            snprintf(u->sub, sizeof(u->sub), "%s", line + 9);
        else if (!strncmp(line, "Result=", 7))
            snprintf(u->result, sizeof(u->result), "%s", line + 7);
    }
    free(out);
    return 0;
}

static void query_portals(AtlasHealth *h)
{
    char list[512];
    char *tok, *save = NULL;

    snprintf(list, sizeof(list), "%s", atlas_conf_get("recover.portal_units"));
    h->nportals = 0;
    for (tok = strtok_r(list, " ,", &save); tok && h->nportals < ATLAS_MAX_UNITS;
         tok = strtok_r(NULL, " ,", &save)) {
        AtlasUnit u;
        if (atlas_unit_query(tok, &u) == 0 && u.loaded)
            h->portals[h->nportals++] = u;
    }
}

/* ============================================================== check */

int atlas_health_check(AtlasHealth *h, int allow_active_probe)
{
    const AtlasNode *sink;
    int i, down = 0;

    memset(h, 0, sizeof(*h));
    h->acoustic = ATLAS_UNKNOWN;

    /* 1. servicios */
    for (i = 0; audio_units[i]; i++) {
        AtlasUnit *u = &h->units[h->nunits++];
        atlas_unit_query(audio_units[i], u);
        if (strcmp(u->active, "active") != 0) {
            down++;
            snprintf(h->services_detail + strlen(h->services_detail),
                     sizeof(h->services_detail) - strlen(h->services_detail),
                     "%s%s=%s", down > 1 ? ", " : "", u->unit, u->active);
        }
    }
    h->services = down ? ATLAS_FAIL : ATLAS_OK;
    if (!down)
        snprintf(h->services_detail, sizeof(h->services_detail),
                 "pipewire, pipewire-pulse y wireplumber activos");
    query_portals(h);

    /* 2. conexión */
    if (atlas_audio_query(&h->audio) != 0) {
        h->connection = ATLAS_FAIL;
        snprintf(h->connection_detail, sizeof(h->connection_detail),
                 "pipewire-pulse no responde a pactl");
    } else if (!(sink = atlas_audio_default_sink(&h->audio))) {
        h->connection = ATLAS_FAIL;
        snprintf(h->connection_detail, sizeof(h->connection_detail),
                 "no hay sink por defecto válido (default='%s')", h->audio.default_sink);
    } else if (!strcmp(sink->name, "auto_null") || strstr(sink->name, "null")) {
        h->connection = ATLAS_FAIL;
        snprintf(h->connection_detail, sizeof(h->connection_detail),
                 "el sink por defecto es el sumidero nulo (%s): no hay salida real", sink->name);
    } else {
        h->connection = sink->mute ? ATLAS_WARN : ATLAS_OK;
        snprintf(h->connection_detail, sizeof(h->connection_detail),
                 "%s (%s, %.0f%%%s), %d stream(s) de reproducción",
                 sink->description, sink->state, atlas_volume_percent(&sink->volume),
                 sink->mute ? ", SILENCIADO" : "", h->audio.playback_streams);
    }

    /* 3. hardware */
    sink = h->audio.server_ok ? atlas_audio_default_sink(&h->audio) : NULL;
    if (h->connection == ATLAS_FAIL || !sink) {
        h->hardware = ATLAS_UNKNOWN;
        snprintf(h->hardware_detail, sizeof(h->hardware_detail),
                 "no evaluable sin conexión al servidor de audio");
    } else {
        h->probed = 1;
        atlas_probe_sink(sink, allow_active_probe && atlas_conf_bool("probe.active"), &h->probe);
        switch (h->probe.result) {
        case ATLAS_PROBE_OK: h->hardware = ATLAS_OK; break;
        case ATLAS_PROBE_STALLED:
        case ATLAS_PROBE_NOT_OPENED: h->hardware = ATLAS_FAIL; break;
        case ATLAS_PROBE_START_FAILED: h->hardware = ATLAS_WARN; break;
        default: h->hardware = ATLAS_UNKNOWN; break;
        }
        snprintf(h->hardware_detail, sizeof(h->hardware_detail), "%s", h->probe.detail);
    }

    h->overall = worst(worst(h->services, h->connection), h->hardware);
    for (i = 0; i < h->nportals; i++)
        if (!strcmp(h->portals[i].active, "failed") && h->overall == ATLAS_OK)
            h->overall = ATLAS_WARN;
    if (h->overall == ATLAS_UNKNOWN)
        h->overall = ATLAS_WARN;
    return h->overall == ATLAS_FAIL ? 1 : 0;
}

static void units_json(JsonBuilder *b, const char *k, const AtlasUnit *u, int n)
{
    int i;
    json_builder_set_member_name(b, k);
    json_builder_begin_array(b);
    for (i = 0; i < n; i++) {
        json_builder_begin_object(b);
        atlas_json_kv_str(b, "unit", u[i].unit);
        atlas_json_kv_str(b, "active", u[i].active);
        atlas_json_kv_str(b, "sub", u[i].sub);
        atlas_json_kv_str(b, "result", u[i].result);
        json_builder_end_object(b);
    }
    json_builder_end_array(b);
}

void atlas_health_json(JsonBuilder *b, const AtlasHealth *h)
{
    json_builder_begin_object(b);
    atlas_json_kv_str(b, "overall", atlas_level_name(h->overall));
    json_builder_set_member_name(b, "levels");
    json_builder_begin_object(b);
#define LVL(name, lv, det) \
    json_builder_set_member_name(b, name); json_builder_begin_object(b); \
    atlas_json_kv_str(b, "level", atlas_level_name(lv)); atlas_json_kv_str(b, "detail", det); \
    json_builder_end_object(b);
    LVL("services", h->services, h->services_detail);
    LVL("connection", h->connection, h->connection_detail);
    LVL("hardware", h->hardware, h->hardware_detail);
    LVL("acoustic", h->acoustic,
        "no medible automáticamente: confirma con audio-profile test-tone");
#undef LVL
    json_builder_end_object(b);
    units_json(b, "units", h->units, h->nunits);
    units_json(b, "portals", h->portals, h->nportals);
    if (h->probed) {
        json_builder_set_member_name(b, "probe");
        atlas_probe_json(b, &h->probe);
    }
    json_builder_set_member_name(b, "audio");
    atlas_audio_json(b, &h->audio);
    json_builder_end_object(b);
}

void atlas_health_print(const AtlasHealth *h)
{
    int i;

    printf("  %s Servicios   %s\n", atlas_level_icon(h->services), h->services_detail);
    for (i = 0; i < h->nportals; i++)
        if (strcmp(h->portals[i].active, "active") != 0)
            printf("  %s Portal      %s está %s (%s)\n",
                   !strcmp(h->portals[i].active, "failed") ? "⚠" : "·",
                   h->portals[i].unit, h->portals[i].active,
                   h->portals[i].result[0] ? h->portals[i].result : "-");
    printf("  %s Conexión    %s\n", atlas_level_icon(h->connection), h->connection_detail);
    printf("  %s Hardware    %s\n", atlas_level_icon(h->hardware), h->hardware_detail);
    printf("  %s Acústica    no medible automáticamente (audio-profile test-tone)\n",
           atlas_level_icon(h->acoustic));
}

/* ============================================================== clock */

static int meta_value(const char *out, const char *key)
{
    char needle[96];
    const char *p;

    snprintf(needle, sizeof(needle), "key:'%s'", key);
    p = strstr(out, needle);
    if (!p || !(p = strstr(p, "value:'")))
        return -1;
    return atoi(p + 7);
}

int atlas_clock_parse(const char *out, AtlasClock *c)
{
    const char *p;

    memset(c, 0, sizeof(*c));
    if (!out)
        return -1;
    c->rate = meta_value(out, "clock.rate");
    c->quantum = meta_value(out, "clock.quantum");
    c->min_quantum = meta_value(out, "clock.min-quantum");
    c->max_quantum = meta_value(out, "clock.max-quantum");
    c->force_rate = meta_value(out, "clock.force-rate");
    c->force_quantum = meta_value(out, "clock.force-quantum");
    if ((p = strstr(out, "key:'clock.allowed-rates'")) && (p = strstr(p, "value:'"))) {
        const char *e = strchr(p + 7, '\'');
        if (e)
            snprintf(c->allowed_rates, sizeof(c->allowed_rates), "%.*s", (int)(e - p - 7), p + 7);
    }
    if (c->force_rate < 0)
        c->force_rate = 0;
    if (c->force_quantum < 0)
        c->force_quantum = 0;
    c->ok = c->rate > 0;
    return c->ok ? 0 : -1;
}

int atlas_clock_query(AtlasClock *c)
{
    char *out = NULL;
    int r = atlas_runl(&out, 4000, "pw-metadata", "-n", "settings", "0", NULL);
    if (r != 0) {
        free(out);
        memset(c, 0, sizeof(*c));
        return -1;
    }
    r = atlas_clock_parse(out, c);
    free(out);
    return r;
}

/* ===================================================== perfil actual */

void atlas_current_profile(char *buf, size_t n)
{
    char path[1024];
    char *s;

    snprintf(buf, n, "ninguno");
    snprintf(path, sizeof(path), "%s/current-profile", atlas_state_dir());
    s = atlas_read_file(path, 256);
    if (!s) {
        const char *home = getenv("HOME");
        snprintf(path, sizeof(path), "%s/.config/audio-profiles/state/current-profile",
                 home ? home : "");
        s = atlas_read_file(path, 256);
    }
    if (s) {
        s[strcspn(s, "\r\n")] = '\0';
        if (*s)
            snprintf(buf, n, "%s", s);
        free(s);
    }
}

/* ============================================================ status */

int atlas_cmd_status(int probe)
{
    AtlasHealth *h = malloc(sizeof(*h));
    AtlasClock c;
    char prof[128];
    const AtlasNode *sink, *src;
    int ret;

    if (!h)
        return 2;
    ret = atlas_health_check(h, probe);
    atlas_clock_query(&c);
    atlas_current_profile(prof, sizeof(prof));

    if (atlas_json_mode) {
        JsonBuilder *b = json_builder_new();
        json_builder_begin_object(b);
        atlas_json_kv_str(b, "version", ATLAS_VERSION);
        atlas_json_kv_str(b, "profile", prof);
        json_builder_set_member_name(b, "clock");
        json_builder_begin_object(b);
        atlas_json_kv_bool(b, "ok", c.ok);
        atlas_json_kv_int(b, "rate", c.rate);
        atlas_json_kv_int(b, "quantum", c.quantum);
        atlas_json_kv_int(b, "force_rate", c.force_rate);
        atlas_json_kv_int(b, "force_quantum", c.force_quantum);
        atlas_json_kv_str(b, "allowed_rates", c.allowed_rates);
        atlas_json_kv_double(b, "latency_ms",
            c.ok ? 1000.0 * (c.force_quantum ? c.force_quantum : c.quantum) /
                   (c.force_rate ? c.force_rate : c.rate) : 0);
        json_builder_end_object(b);
        json_builder_set_member_name(b, "health");
        atlas_health_json(b, h);
        json_builder_end_object(b);
        atlas_json_print(b);
        free(h);
        return ret;
    }

    puts("Project Atlas · audio-profile " ATLAS_VERSION);
    puts("==========================================");
    printf("Perfil actual : %s\n", prof);
    if (c.ok) {
        int q = c.force_quantum ? c.force_quantum : c.quantum;
        int r = c.force_rate ? c.force_rate : c.rate;
        printf("Reloj         : %d Hz, quantum %d (≈%.1f ms)%s%s\n", r, q, 1000.0 * q / r,
               c.force_rate ? " · rate forzado" : "", c.force_quantum ? " · quantum forzado" : "");
    } else {
        puts("Reloj         : no disponible (pw-metadata no responde)");
    }

    sink = atlas_audio_default_sink(&h->audio);
    src = atlas_audio_default_source(&h->audio);
    if (sink)
        printf("Salida        : %s · %.0f%%%s\n", sink->description,
               atlas_volume_percent(&sink->volume), sink->mute ? " · silenciada" : "");
    if (src)
        printf("Entrada       : %s · %.0f%%%s\n", src->description,
               atlas_volume_percent(&src->volume), src->mute ? " · silenciada" : "");
    puts("");
    puts("Salud por niveles:");
    atlas_health_print(h);
    puts("");
    if (h->overall == ATLAS_FAIL)
        puts("Resultado: FALLO → ejecuta `audio-profile recover`");
    else if (h->hardware == ATLAS_UNKNOWN && h->probe.result == ATLAS_PROBE_IDLE)
        puts("Resultado: servicios y conexión bien; hardware sin verificar (usa `audio-profile status --probe`)");
    else
        printf("Resultado: %s\n", h->overall == ATLAS_OK ? "OK" : "con advertencias");

    free(h);
    return ret;
}

/* Salida corta para scripts: 0 sano, 1 fallo, 2 indeterminado */
int atlas_cmd_check(void)
{
    AtlasHealth *h = malloc(sizeof(*h));
    int r;

    if (!h)
        return 2;
    atlas_health_check(h, 1);
    r = h->overall == ATLAS_FAIL ? 1 : (h->hardware == ATLAS_OK ? 0 : 2);
    if (atlas_json_mode) {
        JsonBuilder *b = json_builder_new();
        atlas_health_json(b, h);
        atlas_json_print(b);
    } else {
        atlas_health_print(h);
    }
    free(h);
    return r;
}

int atlas_cmd_test_tone(int argc, char **argv)
{
    AtlasAudio *a = malloc(sizeof(*a));
    const char *sink = NULL;
    double secs = 1.2, freq = 440;
    int i, r;

    for (i = 0; i < argc; i++) {
        if (!strcmp(argv[i], "--sink") && i + 1 < argc)
            sink = argv[++i];
        else if (!strcmp(argv[i], "--freq") && i + 1 < argc)
            freq = atof(argv[++i]);
        else if (!strcmp(argv[i], "--seconds") && i + 1 < argc)
            secs = atof(argv[++i]);
    }
    if (!sink && a && atlas_audio_query(a) == 0 && a->default_sink[0])
        sink = a->default_sink;
    if (!atlas_json_mode)
        printf("Reproduciendo tono suave de %.0f Hz durante %.1f s en %s...\n",
               freq, secs, sink ? sink : "(sink por defecto)");
    r = atlas_test_tone(sink, secs, freq, 0.12);
    if (atlas_json_mode) {
        JsonBuilder *b = json_builder_new();
        json_builder_begin_object(b);
        atlas_json_kv_bool(b, "ok", r == 0);
        atlas_json_kv_str(b, "sink", sink);
        json_builder_end_object(b);
        atlas_json_print(b);
    } else {
        puts(r == 0 ? "Tono enviado. ¿Lo oíste? Si no, ejecuta `audio-profile recover --confirm`."
                    : "No se pudo reproducir el tono (pw-cat falló).");
    }
    free(a);
    return r == 0 ? 0 : 1;
}
