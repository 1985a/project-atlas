#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "atlas.h"
#include "atlas_audio.h"
#include "atlas_health.h"
#include "atlas_probe.h"
#include "atlas_recover.h"

static const char *level_names[] = { "evidence", "card-cycle", "pulse", "pipewire" };
static const char *level_desc[] = {
    "Capturar evidencia",
    "Reabrir la tarjeta (perfil off → anterior)",
    "Reiniciar pipewire-pulse",
    "Reiniciar PipeWire, pipewire-pulse y WirePlumber",
};

/* ============================================================ eventos */

static void emit(const char *kind, int level, const char *msg)
{
    if (atlas_json_mode) {
        JsonBuilder *b = json_builder_new();
        JsonGenerator *g = json_generator_new();
        JsonNode *root;
        gchar *s;
        json_builder_begin_object(b);
        atlas_json_kv_str(b, "event", kind);
        atlas_json_kv_int(b, "level", level);
        atlas_json_kv_str(b, "message", msg);
        json_builder_end_object(b);
        root = json_builder_get_root(b);
        json_generator_set_root(g, root);
        s = json_generator_to_data(g, NULL);
        puts(s);
        fflush(stdout);
        g_free(s);
        json_node_unref(root);
        g_object_unref(g);
        g_object_unref(b);
        return;
    }
    if (!strcmp(kind, "level"))
        printf("\n== Nivel %d: %s ==\n", level, msg);
    else if (!strcmp(kind, "ok"))
        printf("  ✓ %s\n", msg);
    else if (!strcmp(kind, "fail"))
        printf("  ✗ %s\n", msg);
    else if (!strcmp(kind, "warn"))
        printf("  ⚠ %s\n", msg);
    else
        printf("  · %s\n", msg);
    fflush(stdout);
}

/* ============================================================ informe */

static void section(FILE *f, const char *title, const char *const argv[])
{
    char *out = NULL;
    int r;

    fprintf(f, "\n----- %s -----\n", title);
    r = atlas_run(argv, &out, 8000, ATLAS_RUN_MERGE_STDERR);
    if (out)
        fputs(out, f);
    if (r != 0)
        fprintf(f, "[código de salida %d]\n", r);
    free(out);
}

#define SECTION(f, title, ...) do { \
    const char *av_[] = { __VA_ARGS__, NULL }; \
    section(f, title, av_); } while (0)

static void dump_proc_asound(FILE *f)
{
    int card, dev, sub;
    char path[256], *s;

    fprintf(f, "\n----- /proc/asound -----\n");
    s = atlas_read_file("/proc/asound/cards", 8192);
    if (s) {
        fputs(s, f);
        free(s);
    }
    for (card = 0; card < 16; card++)
        for (dev = 0; dev < 16; dev++)
            for (sub = 0; sub < 8; sub++) {
                snprintf(path, sizeof(path), "/proc/asound/card%d/pcm%dp/sub%d/status", card, dev, sub);
                s = atlas_read_file(path, 4096);
                if (!s)
                    break;
                fprintf(f, "[%s]\n%s", path, s);
                free(s);
            }
}

static void dump_kernel(FILE *f)
{
    char *out = NULL, *line, *save = NULL;
    const char *keys[] = { "snd", "hda", "ca0132", "AMD-Vi", "IO_PAGE_FAULT", "PM: ", "audio",
                           "xhci", NULL };

    fprintf(f, "\n----- kernel (arranque actual, filtrado) -----\n");
    if (atlas_runl(&out, 8000, "journalctl", "-k", "-b", "--no-pager", "-n", "600",
                   "-o", "short-iso", NULL) != 0 || !out) {
        fprintf(f, "(journalctl -k no disponible)\n");
        free(out);
        return;
    }
    for (line = strtok_r(out, "\n", &save); line; line = strtok_r(NULL, "\n", &save)) {
        int i;
        for (i = 0; keys[i]; i++)
            if (strstr(line, keys[i])) {
                fprintf(f, "%s\n", line);
                break;
            }
    }
    free(out);
}

int atlas_report_capture(const char *path, const char *title, int append)
{
    FILE *f = fopen(path, append ? "a" : "w");
    char ts[64];

    if (!f)
        return -1;
    atlas_timestamp(ts, sizeof(ts), 0);
    fprintf(f, "\n=================================================================\n");
    fprintf(f, "Project Atlas %s · %s · %s\n", ATLAS_VERSION, title, ts);
    fprintf(f, "=================================================================\n");
    fflush(f);

    SECTION(f, "uname", "uname", "-r");
    SECTION(f, "systemd --user", "systemctl", "--user", "status", "pipewire.service",
            "pipewire-pulse.service", "wireplumber.service", "xdg-desktop-portal-wlr.service",
            "--no-pager", "-n", "12");
    SECTION(f, "pactl info", "pactl", "info");
    SECTION(f, "pactl cards", "pactl", "list", "short", "cards");
    SECTION(f, "pactl sinks", "pactl", "list", "short", "sinks");
    SECTION(f, "pactl sources", "pactl", "list", "short", "sources");
    SECTION(f, "pactl sink-inputs", "pactl", "list", "short", "sink-inputs");
    SECTION(f, "wpctl status", "wpctl", "status");
    SECTION(f, "pw-metadata settings", "pw-metadata", "-n", "settings", "0");
    SECTION(f, "pw-metadata default", "pw-metadata", "-n", "default", "0");
    fflush(f);
    dump_proc_asound(f);
    dump_kernel(f);
    SECTION(f, "journal --user (audio)", "journalctl", "--user", "-b", "--no-pager", "-n", "80",
            "-u", "pipewire", "-u", "pipewire-pulse", "-u", "wireplumber",
            "-u", "xdg-desktop-portal-wlr", "-u", "audio-profile-watch");
    fclose(f);
    return 0;
}

/* Añade una nota con formato al informe de recuperación. */
static void report_note(const char *path, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
static void report_note(const char *path, const char *fmt, ...)
{
    FILE *f = fopen(path, "a");
    va_list ap;

    if (!f)
        return;
    va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    fclose(f);
}

/* ============================================================ portales */

int atlas_repair_portals(char *log, size_t n)
{
    char list[512];
    char *tok, *save = NULL;
    int repaired = 0;

    if (log && n)
        log[0] = '\0';
    snprintf(list, sizeof(list), "%s", atlas_conf_get("recover.portal_units"));
    for (tok = strtok_r(list, " ,", &save); tok; tok = strtok_r(NULL, " ,", &save)) {
        AtlasUnit u;
        if (atlas_unit_query(tok, &u) != 0 || !u.loaded || !strcmp(u.active, "active"))
            continue;
        atlas_runl(NULL, 4000, "systemctl", "--user", "reset-failed", tok, NULL);
        atlas_runl(NULL, 8000, "systemctl", "--user", "restart", tok, NULL);
        atlas_msleep(1200);
        atlas_unit_query(tok, &u);
        if (log && n) {
            size_t l = strlen(log);
            snprintf(log + l, n - l, "%s%s -> %s", l ? "; " : "", tok, u.active);
        }
        if (!strcmp(u.active, "active"))
            repaired++;
    }
    return repaired;
}

/* ======================================================== recuperación */

static int healthy(const AtlasHealth *h, int *verified)
{
    *verified = h->hardware == ATLAS_OK;
    if (h->services != ATLAS_OK || h->connection == ATLAS_FAIL)
        return 0;
    if (h->hardware == ATLAS_OK)
        return 1;
    if (h->hardware == ATLAS_FAIL)
        return 0;
    /* sink no ALSA o sonda no disponible: no hay evidencia de fallo */
    return h->probe.result == ATLAS_PROBE_UNSUPPORTED;
}

static int ask_heard(void)
{
    char line[32];
    if (!isatty(STDIN_FILENO) || atlas_json_mode)
        return 1;
    atlas_test_tone(NULL, 1.0, 440, 0.12);
    printf("  ¿Se oyó el tono? [s/N]: ");
    fflush(stdout);
    if (!fgets(line, sizeof(line), stdin))
        return 0;
    return line[0] == 's' || line[0] == 'S' || line[0] == 'y' || line[0] == 'Y';
}

typedef struct {
    int level;
    int ok;
    long long ms;
    char detail[512];
} Step;

static void history_write(const AtlasRecoverOpts *o, const AtlasHealth *initial, const Step *steps,
                          int nsteps, int final_ok, int verified, const char *report)
{
    JsonBuilder *b = json_builder_new();
    JsonGenerator *g = json_generator_new();
    JsonNode *root;
    char ts[64], path[1024];
    gchar *s;
    int i;

    atlas_timestamp(ts, sizeof(ts), 0);
    json_builder_begin_object(b);
    atlas_json_kv_str(b, "time", ts);
    atlas_json_kv_str(b, "trigger", o->trigger ? o->trigger : "manual");
    atlas_json_kv_str(b, "initial", atlas_level_name(initial->overall));
    atlas_json_kv_str(b, "initial_probe", atlas_probe_result_name(initial->probe.result));
    atlas_json_kv_str(b, "initial_detail", initial->hardware_detail);
    json_builder_set_member_name(b, "steps");
    json_builder_begin_array(b);
    for (i = 0; i < nsteps; i++) {
        json_builder_begin_object(b);
        atlas_json_kv_int(b, "level", steps[i].level);
        atlas_json_kv_str(b, "action", level_names[steps[i].level]);
        atlas_json_kv_bool(b, "ok", steps[i].ok);
        atlas_json_kv_int(b, "ms", steps[i].ms);
        atlas_json_kv_str(b, "detail", steps[i].detail);
        json_builder_end_object(b);
    }
    json_builder_end_array(b);
    atlas_json_kv_bool(b, "final_ok", final_ok);
    atlas_json_kv_bool(b, "verified", verified);
    atlas_json_kv_bool(b, "dry_run", o->dry_run);
    atlas_json_kv_str(b, "report", report);
    json_builder_end_object(b);

    root = json_builder_get_root(b);
    json_generator_set_root(g, root);
    s = json_generator_to_data(g, NULL);
    snprintf(path, sizeof(path), "%s/history.jsonl", atlas_state_dir());
    atlas_append_line(path, s);
    g_free(s);
    json_node_unref(root);
    g_object_unref(g);
    g_object_unref(b);
}

static int start_level(const AtlasHealth *h)
{
    int pw = 1, pulse = 1, wp = 1, i;
    for (i = 0; i < h->nunits; i++) {
        int up = !strcmp(h->units[i].active, "active");
        if (strstr(h->units[i].unit, "pipewire-pulse"))
            pulse = up;
        else if (strstr(h->units[i].unit, "wireplumber"))
            wp = up;
        else if (strstr(h->units[i].unit, "pipewire"))
            pw = up;
    }
    if (!pw || !wp)
        return 3;
    if (!pulse || !h->audio.server_ok)
        return 2;
    return 1;
}

static int do_card_cycle(const AtlasHealth *h, char *detail, size_t n)
{
    AtlasPcmFlowInfo flows[16];
    const AtlasNode *sink = atlas_audio_default_sink(&h->audio);
    char done[ATLAS_MAX_CARDS][ATLAS_NAME];
    int ndone = 0, i, nflows, rc = 0;

    detail[0] = '\0';

    /* tarjeta del sink por defecto + cualquier tarjeta con PCM atascado */
    nflows = atlas_pcm_scan_running(flows, 16, 300);
    for (i = -1; i < nflows; i++) {
        const AtlasCard *c = NULL;
        int k, dup = 0;
        if (i < 0)
            c = sink ? atlas_audio_card_by_alsa(&h->audio, sink->alsa_card) : NULL;
        else if (flows[i].flow == ATLAS_FLOW_STALLED || flows[i].flow == ATLAS_FLOW_SLOW)
            c = atlas_audio_card_by_alsa(&h->audio, flows[i].card);
        if (!c || !strcmp(c->active_profile, "off") || !c->active_profile[0])
            continue;
        for (k = 0; k < ndone; k++)
            if (!strcmp(done[k], c->name))
                dup = 1;
        if (dup || ndone >= ATLAS_MAX_CARDS)
            continue;
        snprintf(done[ndone++], ATLAS_NAME, "%s", c->name);
        if (atlas_audio_cycle_card(c->name, c->active_profile) != 0)
            rc = -1;
        snprintf(detail + strlen(detail), n - strlen(detail), "%s%s (%s)",
                 strlen(detail) ? ", " : "", c->description, c->active_profile);
    }
    if (ndone == 0) {
        snprintf(detail, n, "no hay tarjeta ALSA activa que reabrir");
        return -1;
    }
    atlas_msleep(1500);
    return rc;
}

int atlas_recover(const AtlasRecoverOpts *o, char *report_path, size_t rpn)
{
    AtlasHealth *initial = malloc(sizeof(AtlasHealth));
    AtlasHealth *h = malloc(sizeof(AtlasHealth));
    AtlasAudio *snap = malloc(sizeof(AtlasAudio));
    AtlasClock clock;
    Step steps[8];
    int nsteps = 0, level, max_level, ok = 0, verified = 0, from;
    char ts[64], msg[768], rdir[1024];

    if (!initial || !h || !snap) {
        free(initial);
        free(h);
        free(snap);
        return 2;
    }
    max_level = o->max_level > 0 ? o->max_level : atlas_conf_int("recover.max_level");
    if (max_level < 1 || max_level > 3)
        max_level = 3;

    /* ---- nivel 0: evidencia ---- */
    snprintf(rdir, sizeof(rdir), "%s/reports", atlas_state_dir());
    atlas_mkdirs(rdir);
    atlas_timestamp(ts, sizeof(ts), 1);
    snprintf(report_path, rpn, "%s/recover-%s.txt", rdir, ts);
    for (from = 2; atlas_file_exists(report_path) && from < 100; from++)
        snprintf(report_path, rpn, "%s/recover-%s-%d.txt", rdir, ts, from);
    emit("level", 0, level_desc[0]);
    atlas_report_capture(report_path, "ANTES de cualquier acción", 0);
    snprintf(msg, sizeof(msg), "informe: %s", report_path);
    emit("info", 0, msg);

    atlas_clock_query(&clock);
    atlas_health_check(initial, 1);
    snprintf(msg, sizeof(msg), "estado inicial: servicios=%s conexión=%s hardware=%s · %s",
             atlas_level_name(initial->services), atlas_level_name(initial->connection),
             atlas_level_name(initial->hardware), initial->hardware_detail);
    emit(initial->overall == ATLAS_FAIL ? "fail" : "info", 0, msg);
    report_note(report_path, "\n>>> Sonda inicial: %s\n", initial->hardware_detail);

    /* instantánea a restaurar: la indicada, o la actual si el servidor respondía */
    if (o->snapshot && atlas_audio_load_snapshot(snap, o->snapshot) == 0)
        emit("info", 0, "usando instantánea previa a la suspensión");
    else
        *snap = initial->audio;

    if (healthy(initial, &verified) && !o->force) {
        int heard = 1;
        if (o->confirm)
            heard = ask_heard();
        if (heard) {
            emit("ok", 0, verified ? "audio sano (hardware verificado): no se hace nada"
                                   : "servicios y conexión sanos; hardware no verificable: no se hace nada");
            ok = 1;
            goto done;
        }
        emit("warn", 0, "la sonda dice OK pero no se oyó: se intentará la recuperación");
    }

    from = o->force ? 1 : start_level(initial);
    if (o->start_level > from)
        from = o->start_level;
    if (from > max_level)
        from = max_level;

    for (level = from; level <= max_level; level++) {
        Step *st = &steps[nsteps++];
        long long t0 = atlas_now_ms();
        AtlasHealth *before = (level == from) ? initial : h;

        memset(st, 0, sizeof(*st));
        st->level = level;
        emit("level", level, level_desc[level]);

        if (o->dry_run) {
            snprintf(st->detail, sizeof(st->detail), "simulación: no se ejecuta");
            emit("info", level, st->detail);
            continue;
        }

        if (level == 1) {
            if (do_card_cycle(before, st->detail, sizeof(st->detail)) != 0) {
                emit("warn", level, st->detail);
            } else {
                snprintf(msg, sizeof(msg), "reabierta: %s", st->detail);
                emit("info", level, msg);
            }
        } else if (level == 2) {
            atlas_runl(NULL, 15000, "systemctl", "--user", "restart",
                       "pipewire-pulse.socket", "pipewire-pulse.service", NULL);
            atlas_audio_wait_ready(snap->default_sink, 8000);
            snprintf(st->detail, sizeof(st->detail), "pipewire-pulse reiniciado");
        } else {
            atlas_runl(NULL, 20000, "systemctl", "--user", "restart",
                       "pipewire.socket", "pipewire.service",
                       "pipewire-pulse.socket", "pipewire-pulse.service",
                       "wireplumber.service", NULL);
            atlas_audio_wait_ready(snap->default_sink, 10000);
            /* el reinicio borra la metadata "settings": reaplicar rate/quantum forzados */
            if (clock.ok && (clock.force_rate || clock.force_quantum)) {
                char r[16], q[16];
                snprintf(r, sizeof(r), "%d", clock.force_rate);
                snprintf(q, sizeof(q), "%d", clock.force_quantum);
                atlas_runl(NULL, 4000, "pw-metadata", "-n", "settings", "0", "clock.force-rate", r, NULL);
                atlas_runl(NULL, 4000, "pw-metadata", "-n", "settings", "0", "clock.force-quantum", q, NULL);
                emit("info", level, "rate/quantum del perfil reaplicados");
            }
            snprintf(st->detail, sizeof(st->detail), "pila de audio reiniciada");
        }

        if (level >= 2 && snap->server_ok) {
            char rlog[1024];
            int n = atlas_audio_restore(snap, atlas_conf_bool("recover.restore_volumes"),
                                        rlog, sizeof(rlog));
            if (n > 0) {
                snprintf(msg, sizeof(msg), "restaurado: %s", rlog);
                for (char *p = msg; *p; p++)
                    if (*p == '\n')
                        *p = ' ';
                emit("info", level, msg);
            }
        }

        {
            char plog[512];
            if (atlas_repair_portals(plog, sizeof(plog)) > 0 || plog[0]) {
                snprintf(msg, sizeof(msg), "portales: %s", plog);
                emit("info", level, msg);
            }
        }

        atlas_health_check(h, 1);
        st->ms = atlas_now_ms() - t0;
        st->ok = healthy(h, &verified);
        snprintf(msg, sizeof(msg), "verificación: %s", h->hardware_detail);
        report_note(report_path, "\n>>> Nivel %s: %s\n", level_names[level], h->hardware_detail);

        if (st->ok && o->confirm && !ask_heard()) {
            st->ok = 0;
            snprintf(msg, sizeof(msg), "la sonda dice OK pero no se oyó; se sube de nivel");
        }
        emit(st->ok ? "ok" : "fail", level, msg);
        if (st->ok) {
            ok = 1;
            break;
        }
    }

    if (o->dry_run) {
        emit("info", 0, "simulación terminada: no se modificó nada");
        ok = healthy(initial, &verified);
    } else if (!ok) {
        emit("fail", max_level, "no se pudo recuperar automáticamente; revisa el informe");
        if (initial->probe.result == ATLAS_PROBE_STALLED)
            emit("info", max_level,
                 "el hardware sigue sin consumir muestras: puede ser un fallo del driver "
                 "tras S3 (mira IO_PAGE_FAULT en el informe). Último recurso: reiniciar el equipo");
    }

done:
    if (!o->dry_run)
        atlas_report_capture(report_path, ok ? "DESPUÉS (recuperado)" : "DESPUÉS (sin recuperar)", 1);
    history_write(o, initial, steps, nsteps, ok, verified, report_path);

    if (atlas_json_mode) {
        snprintf(msg, sizeof(msg), "%s", ok ? "recovered" : "failed");
        emit("result", ok ? 0 : 1, msg);
    } else {
        printf("\nResultado: %s\n", ok ? (verified ? "audio funcionando (verificado en hardware)"
                                                   : "servicios sanos (hardware no verificable)")
                                       : "SIN RECUPERAR");
        printf("Informe:   %s\n", report_path);
    }
    free(initial);
    free(h);
    free(snap);
    return ok ? 0 : 1;
}

int atlas_cmd_recover(int argc, char **argv)
{
    AtlasRecoverOpts o;
    char report[1024];
    int i;

    memset(&o, 0, sizeof(o));
    o.trigger = "manual";
    for (i = 0; i < argc; i++) {
        if (!strcmp(argv[i], "--dry-run"))
            o.dry_run = 1;
        else if (!strcmp(argv[i], "--force"))
            o.force = 1;
        else if (!strcmp(argv[i], "--confirm"))
            o.confirm = 1;
        else if (!strcmp(argv[i], "--max-level") && i + 1 < argc)
            o.max_level = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--level") && i + 1 < argc)
            o.start_level = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--trigger") && i + 1 < argc)
            o.trigger = argv[++i];
        else {
            fprintf(stderr, "Opción desconocida: %s\n", argv[i]);
            fprintf(stderr, "Uso: audio-profile recover [--dry-run] [--force] [--confirm] "
                            "[--level 1-3] [--max-level 1-3]\n");
            return 2;
        }
    }
    if (!atlas_json_mode) {
        puts("Project Atlas · Recuperación escalonada");
        puts("=======================================");
    }
    return atlas_recover(&o, report, sizeof(report));
}

int atlas_cmd_report(void)
{
    char rdir[1024], path[1200], ts[64];

    snprintf(rdir, sizeof(rdir), "%s/reports", atlas_state_dir());
    atlas_mkdirs(rdir);
    atlas_timestamp(ts, sizeof(ts), 1);
    snprintf(path, sizeof(path), "%s/report-%s.txt", rdir, ts);
    if (atlas_report_capture(path, "Informe manual", 0) != 0) {
        fprintf(stderr, "No pude escribir %s\n", path);
        return 1;
    }
    if (atlas_json_mode) {
        JsonBuilder *b = json_builder_new();
        json_builder_begin_object(b);
        atlas_json_kv_str(b, "report", path);
        json_builder_end_object(b);
        atlas_json_print(b);
    } else {
        printf("Informe creado: %s\n", path);
    }
    return 0;
}

int atlas_cmd_history(int argc, char **argv)
{
    char path[1024];
    char *data, *line, *save = NULL;
    int limit = 20, total = 0, i, start;
    char *lines[4096];

    for (i = 0; i < argc; i++)
        if (!strcmp(argv[i], "-n") && i + 1 < argc)
            limit = atoi(argv[++i]);

    snprintf(path, sizeof(path), "%s/history.jsonl", atlas_state_dir());
    data = atlas_read_file(path, 8 << 20);
    for (line = data ? strtok_r(data, "\n", &save) : NULL; line && total < 4096;
         line = strtok_r(NULL, "\n", &save))
        lines[total++] = line;
    start = total > limit ? total - limit : 0;

    if (atlas_json_mode) {
        printf("[");
        for (i = start; i < total; i++)
            printf("%s%s", i > start ? ",\n" : "\n", lines[i]);
        printf("\n]\n");
        free(data);
        return 0;
    }

    if (total == 0) {
        puts("Sin historial todavía.");
        free(data);
        return 0;
    }
    puts("Historial de recuperaciones (más reciente al final)");
    puts("----------------------------------------------------");
    for (i = start; i < total; i++) {
        JsonParser *p = json_parser_new();
        if (json_parser_load_from_data(p, lines[i], -1, NULL)) {
            JsonObject *o = json_node_get_object(json_parser_get_root(p));
            JsonArray *st = json_object_get_array_member(o, "steps");
            char acts[256] = "";
            guint k;
            for (k = 0; st && k < json_array_get_length(st); k++) {
                JsonObject *so = json_array_get_object_element(st, k);
                snprintf(acts + strlen(acts), sizeof(acts) - strlen(acts), "%s%s%s",
                         k ? " → " : "", json_object_get_string_member(so, "action"),
                         json_object_get_boolean_member(so, "ok") ? "✓" : "✗");
            }
            int dry = json_object_has_member(o, "dry_run") && json_object_get_boolean_member(o, "dry_run");
            printf("%s  %-7s inicial=%-5s sonda=%-11s %s  %s\n",
                   json_object_get_string_member(o, "time"),
                   json_object_get_string_member(o, "trigger"),
                   json_object_get_string_member(o, "initial"),
                   json_object_get_string_member(o, "initial_probe"),
                   dry ? "simulación (sin cambios)" : (acts[0] ? acts : "sin acciones"),
                   json_object_get_boolean_member(o, "final_ok") ? "OK" : "FALLO");
        }
        g_object_unref(p);
    }
    free(data);
    return 0;
}
