#define _GNU_SOURCE
#include <dirent.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <alsa/asoundlib.h>
#include <gio/gio.h>
#include <glib-unix.h>
#include <gio/gunixfdlist.h>

#include "atlas.h"
#include "atlas_audio.h"
#include "atlas_auto.h"
#include "atlas_base.h"
#include "atlas_dsp.h"
#include "atlas_output.h"
#include "atlas_health.h"
#include "atlas_hwdb.h"
#include "atlas_recover.h"
#include "atlas_steam.h"
#include "atlas_watch.h"

static GDBusConnection *sysbus;
static int inhibit_fd = -1;
static int sleeping;
static GMainLoop *loop;

/* Una línea del registro del daemon (journalctl), con fecha. */
static void logm(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void logm(const char *fmt, ...)
{
    char ts[64];
    va_list ap;

    atlas_timestamp(ts, sizeof(ts), 0);
    printf("[%s] ", ts);
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    putchar('\n');
    fflush(stdout);
}

static void notify(const char *title, const char *body)
{
    if (!atlas_conf_bool("watch.notify") || !atlas_have_cmd("notify-send"))
        return;
    atlas_runl(NULL, 3000, "notify-send", "-a", "Atlas Audio", "-i", "audio-card", title, body, NULL);
}

/* ============================================================ inhibidor */

static void take_inhibitor(void)
{
    GUnixFDList *fds = NULL;
    GError *err = NULL;
    GVariant *r;

    if (inhibit_fd >= 0 || !sysbus || !atlas_conf_bool("watch.quiesce_before_sleep"))
        return;
    r = g_dbus_connection_call_with_unix_fd_list_sync(
        sysbus, "org.freedesktop.login1", "/org/freedesktop/login1",
        "org.freedesktop.login1.Manager", "Inhibit",
        g_variant_new("(ssss)", "sleep", "audio-profile",
                      "Cerrar el audio antes de suspender (BUG-001)", "delay"),
        G_VARIANT_TYPE("(h)"), G_DBUS_CALL_FLAGS_NONE, 5000, NULL, &fds, NULL, &err);
    if (!r) {
        logm("no se pudo tomar el inhibidor de logind: %s", err ? err->message : "?");
        g_clear_error(&err);
        return;
    }
    {
        gint32 idx;
        g_variant_get(r, "(h)", &idx);
        inhibit_fd = g_unix_fd_list_get(fds, idx, &err);
        g_clear_error(&err);
    }
    g_variant_unref(r);
    g_object_unref(fds);
}

static void release_inhibitor(void)
{
    if (inhibit_fd >= 0) {
        close(inhibit_fd);
        inhibit_fd = -1;
    }
}

/* ======================================================= antes / después */

static void path_in_state(char *buf, size_t n, const char *name)
{
    snprintf(buf, n, "%s/%s", atlas_state_dir(), name);
}

static int card_selected(const AtlasCard *c, const AtlasHwDb *db)
{
    const char *mode = atlas_conf_get("watch.quiesce_cards");
    const AtlasHwEntry *e;

    if (c->alsa_card < 0 || !c->active_profile[0] || !strcmp(c->active_profile, "off"))
        return 0;
    if (!strcmp(mode, "all"))
        return 1;
    e = atlas_hwdb_for_card(db, c->alsa_card);
    if (!strcmp(mode, "db"))
        return e && atlas_hw_has_quirk(e, "close-pcm-before-sleep");
    /* lista: ids de la DB o nombres de tarjeta */
    {
        char list[512], *t, *save = NULL;
        snprintf(list, sizeof(list), "%s", mode);
        for (t = strtok_r(list, ", ", &save); t; t = strtok_r(NULL, ", ", &save))
            if (!strcmp(t, c->name) || (e && !strcmp(t, e->id)))
                return 1;
    }
    return 0;
}

void atlas_watch_before_sleep(void)
{
    AtlasAudio *a = malloc(sizeof(*a));
    AtlasHwDb *db = malloc(sizeof(*db));
    AtlasPcmFlowInfo flows[16];
    JsonBuilder *b;
    JsonGenerator *g;
    JsonNode *root;
    gchar *txt;
    char snap[PATH_MAX], st[PATH_MAX], ts[64];
    int i, nflows, quiesce = atlas_conf_bool("watch.quiesce_before_sleep");

    if (!a || !db) {
        free(a);
        free(db);
        return;
    }
    path_in_state(snap, sizeof(snap), "sleep-snapshot.json");
    path_in_state(st, sizeof(st), "sleep-state.json");
    atlas_mkdirs(atlas_state_dir());

    if (atlas_audio_query(a) != 0) {
        logm("antes de suspender: pipewire-pulse no responde; no se cierra nada");
        free(a);
        free(db);
        return;
    }
    atlas_audio_save_snapshot(a, snap);
    atlas_hwdb_load(db);
    nflows = atlas_pcm_scan_running(flows, 16, 0);

    atlas_timestamp(ts, sizeof(ts), 0);
    b = json_builder_new();
    json_builder_begin_object(b);
    atlas_json_kv_str(b, "time", ts);
    atlas_json_kv_int(b, "running_pcms", nflows);
    atlas_json_kv_int(b, "playback_streams", a->playback_streams);
    atlas_json_kv_bool(b, "restored", 0);
    json_builder_set_member_name(b, "quiesced");
    json_builder_begin_array(b);
    for (i = 0; quiesce && i < a->ncards; i++) {
        const AtlasCard *c = &a->cards[i];
        if (!card_selected(c, db))
            continue;
        if (atlas_audio_set_card_profile(c->name, "off") == 0) {
            json_builder_begin_object(b);
            atlas_json_kv_str(b, "card", c->name);
            atlas_json_kv_str(b, "profile", c->active_profile);
            json_builder_end_object(b);
            logm("antes de suspender: %s cerrada (perfil %s)", c->description, c->active_profile);
        }
    }
    json_builder_end_array(b);
    json_builder_end_object(b);

    root = json_builder_get_root(b);
    g = json_generator_new();
    json_generator_set_pretty(g, TRUE);
    json_generator_set_root(g, root);
    txt = json_generator_to_data(g, NULL);
    atlas_write_file_atomic(st, txt);
    g_free(txt);
    json_node_unref(root);
    g_object_unref(g);
    g_object_unref(b);
    free(a);
    free(db);
}

/* Restaura las tarjetas cerradas antes de dormir. Devuelve cuántas. */
static int restore_quiesced(void)
{
    char st[PATH_MAX], *data;
    JsonParser *p;
    int n = 0;

    path_in_state(st, sizeof(st), "sleep-state.json");
    data = atlas_read_file(st, 1 << 16);
    if (!data)
        return 0;
    p = json_parser_new();
    if (json_parser_load_from_data(p, data, -1, NULL) && JSON_NODE_HOLDS_OBJECT(json_parser_get_root(p))) {
        JsonObject *o = json_node_get_object(json_parser_get_root(p));
        JsonArray *q = json_object_has_member(o, "quiesced") ? json_object_get_array_member(o, "quiesced") : NULL;
        int restored = json_object_has_member(o, "restored") && json_object_get_boolean_member(o, "restored");
        guint i;
        if (!restored && q) {
            /* pipewire-pulse puede tardar tras el resume */
            atlas_audio_wait_ready(NULL, 6000);
            for (i = 0; i < json_array_get_length(q); i++) {
                JsonObject *c = json_array_get_object_element(q, i);
                const char *card = json_object_get_string_member(c, "card");
                const char *prof = json_object_get_string_member(c, "profile");
                int tries;
                for (tries = 0; tries < 3; tries++) {
                    if (atlas_audio_set_card_profile(card, prof) == 0) {
                        logm("después de reanudar: %s → %s", card, prof);
                        n++;
                        break;
                    }
                    atlas_msleep(1000);
                }
            }
            json_object_set_boolean_member(o, "restored", TRUE);
            {
                JsonGenerator *g = json_generator_new();
                gchar *txt;
                json_generator_set_root(g, json_parser_get_root(p));
                json_generator_set_pretty(g, TRUE);
                txt = json_generator_to_data(g, NULL);
                atlas_write_file_atomic(st, txt);
                g_free(txt);
                g_object_unref(g);
            }
        }
    }
    g_object_unref(p);
    free(data);
    return n;
}

static void prune_reports(int keep)
{
    char dir[PATH_MAX];
    struct dirent **list;
    int n, i;

    snprintf(dir, sizeof(dir), "%s/reports", atlas_state_dir());
    n = scandir(dir, &list, NULL, alphasort);
    if (n < 0)
        return;
    for (i = 0; i < n; i++) {
        if (list[i]->d_name[0] != '.' && i < n - keep - 2) {
            char f[PATH_MAX + 260];
            snprintf(f, sizeof(f), "%s/%s", dir, list[i]->d_name);
            unlink(f);
        }
        free(list[i]);
    }
    free(list);
}

void atlas_watch_after_resume(void)
{
    char snap[PATH_MAX], report[1024], rlog[1024], cur[128];
    AtlasAudio *s = malloc(sizeof(*s));
    int settle = atlas_conf_int("watch.settle_ms"), restored, rc = 0;

    if (settle < 0 || settle > 30000)
        settle = 2500;
    atlas_msleep(settle);
    restored = restore_quiesced();
    if (restored)
        atlas_msleep(1500);

    path_in_state(snap, sizeof(snap), "sleep-snapshot.json");
    if (s && atlas_audio_load_snapshot(s, snap) == 0) {
        int n = atlas_audio_restore(s, atlas_conf_bool("recover.restore_volumes"), rlog, sizeof(rlog));
        if (n > 0)
            logm("restaurado tras reanudar: %s", rlog);
    }
    free(s);

    if (atlas_conf_bool("watch.auto_recover")) {
        AtlasRecoverOpts o;
        int saved = atlas_json_mode;
        memset(&o, 0, sizeof(o));
        o.trigger = "resume";
        o.snapshot = snap;
        atlas_json_mode = 0;
        rc = atlas_recover(&o, report, sizeof(report));
        atlas_json_mode = saved;
        if (rc != 0)
            notify("Audio sin recuperar tras suspender",
                   "Atlas no pudo restaurar el sonido. Abre Atlas Audio → Recuperación.");
        prune_reports(60);
    }

    atlas_current_profile(cur, sizeof(cur));
    if (rc == 0 && atlas_conf_bool("watch.reapply_profile") && strcmp(cur, "ninguno")) {
        logm("reaplicando perfil %s", cur);
        atlas_apply_profile_id(cur, 1);
    }
}

/* ============================================================ D-Bus */

static void ctl_open(void);
static void schedule_enforce(guint ms);

static void on_prepare_for_sleep(GDBusConnection *c, const gchar *sender, const gchar *path,
                                 const gchar *iface, const gchar *signal, GVariant *params,
                                 gpointer data)
{
    gboolean going;
    (void)c; (void)sender; (void)path; (void)iface; (void)signal; (void)data;

    g_variant_get(params, "(b)", &going);
    if (going) {
        sleeping = 1;
        logm("logind: preparando suspensión");
        if (atlas_conf_bool("watch.quiesce_before_sleep"))
            atlas_watch_before_sleep();
        release_inhibitor();
    } else {
        logm("logind: sistema reanudado");
        atlas_watch_after_resume();
        sleeping = 0;
        take_inhibitor();
        ctl_open();   /* el driver puede haber recreado los controles */
        schedule_enforce(3000);
    }
}

/* ============================================= eventos de la tarjeta (ALSA) */

/*
 * En vez de preguntar a amixer cada pocos segundos, el daemon se suscribe a
 * los eventos del mezclador de la tarjeta: duerme hasta que cambia un control
 * (alguien mueve un volumen, el jack de auriculares...) y solo entonces mira.
 * Si la suscripción falla, se vuelve al sondeo periódico.
 */
static snd_ctl_t *ctl;
static guint ctl_src, enforce_src;

static void run_enforce(void)
{
    char what[1024];
    if (sleeping || !atlas_conf_bool("base.enforce"))
        return;
    if (atlas_base_enforce(what, sizeof(what)) > 0)
        logm("base: restaurado %s", what);
}

static gboolean enforce_cb(gpointer data)
{
    (void)data;
    enforce_src = 0;
    run_enforce();
    return G_SOURCE_REMOVE;
}

/* agrupa ráfagas de eventos (aplicar un perfil cambia 50 controles) */
static void schedule_enforce(guint ms)
{
    if (enforce_src)
        g_source_remove(enforce_src);
    enforce_src = g_timeout_add(ms, enforce_cb, NULL);
}

static void ctl_close(void)
{
    if (ctl_src)
        g_source_remove(ctl_src);
    ctl_src = 0;
    if (ctl)
        snd_ctl_close(ctl);
    ctl = NULL;
}

static gboolean on_ctl(gint fd, GIOCondition cond, gpointer data)
{
    snd_ctl_event_t *ev;
    int any = 0, jack = 0;
    (void)fd;
    (void)data;

    if (cond & (G_IO_ERR | G_IO_HUP | G_IO_NVAL)) {
        logm("eventos ALSA: la tarjeta se cerró; vuelvo al sondeo");
        ctl_src = 0;
        snd_ctl_close(ctl);
        ctl = NULL;
        return G_SOURCE_REMOVE;
    }
    snd_ctl_event_alloca(&ev);
    while (snd_ctl_read(ctl, ev) > 0) {
        if (snd_ctl_event_get_type(ev) != SND_CTL_EVENT_ELEM)
            continue;
        any = 1;
        if (snd_ctl_event_elem_get_interface(ev) == SND_CTL_ELEM_IFACE_CARD)
            jack = 1;
    }
    if (sleeping)
        return G_SOURCE_CONTINUE;
    if (jack)
        atlas_output_tick();
    if (any)
        schedule_enforce(2000);
    return G_SOURCE_CONTINUE;
}

static void ctl_open(void)
{
    char hw_id[64], hw_dir[PATH_MAX], dev[32];
    struct pollfd pfd;
    int card;

    ctl_close();
    if (atlas_dsp_target(&card, hw_id, sizeof(hw_id), hw_dir, sizeof(hw_dir)) != 0 || card < 0)
        return;
    snprintf(dev, sizeof(dev), "hw:%d", card);
    if (snd_ctl_open(&ctl, dev, SND_CTL_NONBLOCK) < 0) {
        ctl = NULL;
        return;
    }
    if (snd_ctl_subscribe_events(ctl, 1) < 0 || snd_ctl_poll_descriptors(ctl, &pfd, 1) != 1) {
        snd_ctl_close(ctl);
        ctl = NULL;
        return;
    }
    ctl_src = g_unix_fd_add(pfd.fd, G_IO_IN | G_IO_ERR | G_IO_HUP, on_ctl, NULL);
    logm("eventos ALSA: escuchando la tarjeta %s (sin sondeo)", dev);
}

/* ================================== qué suena: eventos de pipewire-pulse */

/*
 * `pactl subscribe` avisa al instante cuando una aplicación empieza o deja de
 * sonar (eventos de sink-input), así que la detección automática no necesita
 * preguntar cada 2 s. El sondeo queda cada AUTO_POLL_SLOW_S para lo que no
 * genera evento (cambiar de pestaña en el navegador cambia el título, no el
 * stream). Si pactl muere (p. ej. al reiniciar pipewire-pulse) se relanza.
 */
#define AUTO_POLL_FAST_S   2    /* sin eventos de pactl */
#define AUTO_POLL_SLOW_S   6    /* con eventos de pactl */
#define AUTO_EVENT_DELAY_MS 300 /* agrupar ráfagas de eventos */
#define PACTL_RESTART_S    5

static GPid pactl_pid;
static guint pactl_out_src, auto_src;
static GIOChannel *pactl_out;

static void pulse_events_start(void);

static gboolean auto_cb(gpointer data)
{
    int recheck_ms;
    (void)data;
    auto_src = 0;
    if (sleeping)
        return G_SOURCE_REMOVE;
    recheck_ms = atlas_auto_tick();
    if (recheck_ms > 0)   /* un cambio esperando confirmación */
        auto_src = g_timeout_add((guint)recheck_ms, auto_cb, NULL);
    return G_SOURCE_REMOVE;
}

static void schedule_auto(guint ms)
{
    if (auto_src)
        g_source_remove(auto_src);
    auto_src = g_timeout_add(ms, auto_cb, NULL);
}

static gboolean on_pactl_line(GIOChannel *ch, GIOCondition cond, gpointer data)
{
    gchar *line = NULL;
    int relevant = 0;
    (void)data;

    if (cond & G_IO_IN) {
        while (g_io_channel_read_line(ch, &line, NULL, NULL, NULL) == G_IO_STATUS_NORMAL && line) {
            if (strstr(line, "sink-input"))
                relevant = 1;
            g_free(line);
            line = NULL;
        }
    }
    if (relevant && !sleeping)
        schedule_auto(AUTO_EVENT_DELAY_MS);
    if (cond & (G_IO_HUP | G_IO_ERR)) {
        pactl_out_src = 0;
        return G_SOURCE_REMOVE;   /* el proceso terminó: on_pactl_exit lo relanza */
    }
    return G_SOURCE_CONTINUE;
}

static gboolean restart_pulse_events(gpointer data)
{
    (void)data;
    pulse_events_start();
    return G_SOURCE_REMOVE;
}

static void on_pactl_exit(GPid pid, gint status, gpointer data)
{
    (void)status;
    (void)data;
    g_spawn_close_pid(pid);
    pactl_pid = 0;
    if (pactl_out_src)
        g_source_remove(pactl_out_src);
    pactl_out_src = 0;
    if (pactl_out)
        g_io_channel_unref(pactl_out);
    pactl_out = NULL;
    g_timeout_add_seconds(PACTL_RESTART_S, restart_pulse_events, NULL);
}

static void pulse_events_start(void)
{
    char program[] = "pactl", subcommand[] = "subscribe";
    gchar *argv[] = { program, subcommand, NULL };   /* GLib pide char*, no const */
    gint out_fd;

    if (pactl_pid || !atlas_have_cmd("pactl"))
        return;
    if (!g_spawn_async_with_pipes(NULL, argv, NULL, G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD |
                                  G_SPAWN_STDERR_TO_DEV_NULL, NULL, NULL, &pactl_pid, NULL, &out_fd, NULL,
                                  NULL)) {
        pactl_pid = 0;
        return;
    }
    pactl_out = g_io_channel_unix_new(out_fd);
    g_io_channel_set_close_on_unref(pactl_out, TRUE);
    g_io_channel_set_flags(pactl_out, G_IO_FLAG_NONBLOCK, NULL);
    pactl_out_src = g_io_add_watch(pactl_out, G_IO_IN | G_IO_HUP | G_IO_ERR, on_pactl_line, NULL);
    g_child_watch_add(pactl_pid, on_pactl_exit, NULL);
}

/* Temporizador de respaldo: lo que no avisa con eventos. */
static gboolean auto_tick(gpointer data)
{
    static int elapsed;
    int base_every, auto_every = pactl_pid ? AUTO_POLL_SLOW_S : AUTO_POLL_FAST_S;
    (void)data;
    if (sleeping)
        return G_SOURCE_CONTINUE;
    elapsed += 2;
    /* con eventos ALSA el jack y la base solo se revisan como red de
     * seguridad (30 s / 5 min); sin ellos, como antes (2 s / 10 s) */
    if (!ctl || elapsed % 30 == 0)
        atlas_output_tick();
    if (elapsed % auto_every == 0 && !auto_src)
        auto_cb(NULL);
    base_every = ctl ? 300 : (atlas_conf_int("base.enforce_seconds") > 1 ? atlas_conf_int("base.enforce_seconds") : 10);
    if (elapsed % base_every < 2)
        run_enforce();
    if (!ctl && elapsed % 60 == 0)
        ctl_open();   /* reintentar la suscripción */
    return G_SOURCE_CONTINUE;
}

int atlas_cmd_watch(int argc, char **argv)
{
    GError *err = NULL;
    int i;

    for (i = 0; i < argc; i++) {
        if (!strcmp(argv[i], "--simulate-resume")) {
            /* prueba manual del ciclo completo sin suspender */
            logm("simulación: antes de suspender");
            atlas_watch_before_sleep();
            atlas_msleep(1500);
            logm("simulación: después de reanudar");
            atlas_watch_after_resume();
            return 0;
        }
    }

    setvbuf(stdout, NULL, _IOLBF, 0);
    {
        /* el bucle nunca espera a la red: las consultas de Steam las hace
         * otro proceso de esta misma CLI */
        char self[PATH_MAX];
        ssize_t l = readlink("/proc/self/exe", self, sizeof(self) - 1);
        if (l > 0) {
            self[l] = '\0';
            atlas_steam_set_background_lookups(self);
        }
    }
    sysbus = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &err);
    if (!sysbus) {
        fprintf(stderr, "No pude conectar con el bus del sistema: %s\n", err ? err->message : "?");
        return 1;
    }
    g_dbus_connection_signal_subscribe(sysbus, "org.freedesktop.login1",
                                       "org.freedesktop.login1.Manager", "PrepareForSleep",
                                       "/org/freedesktop/login1", NULL, G_DBUS_SIGNAL_FLAGS_NONE,
                                       on_prepare_for_sleep, NULL, NULL);
    take_inhibitor();
    logm("Atlas watch %s: vigilando suspensión%s", ATLAS_VERSION,
         inhibit_fd >= 0 ? " (inhibidor delay activo: se cierra el PCM antes de S3)" : "");

    /* si el daemon murió entre suspensión y reanudación */
    if (restore_quiesced())
        logm("restauradas tarjetas que quedaron cerradas de una suspensión anterior");

    /* al iniciar sesión: base de volumen + perfil actual (sustituye al
     * `alsactl --file ~/.config/asound.state restore` manual) */
    if (atlas_conf_bool("watch.apply_on_start")) {
        char cur[128];
        atlas_audio_wait_ready(NULL, 10000);
        atlas_current_profile(cur, sizeof(cur));
        if (strcmp(cur, "ninguno") && atlas_apply_profile_id(cur, 1) == 0)
            logm("inicio: perfil %s aplicado sobre tu base de volumen", cur);
        else if (atlas_base_apply() == 0)
            logm("inicio: base de volumen aplicada");
    }

    ctl_open();
    pulse_events_start();
    g_timeout_add_seconds(2, auto_tick, NULL);
    loop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(loop);
    return 0;
}
