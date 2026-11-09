#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "atlas.h"
#include "atlas_auto.h"
#include "atlas_health.h"
#include "atlas_hwdb.h"

/* Doctor: comprobaciones con nivel, detalle y comando de arreglo. */

typedef struct {
    const char *section;
    AtlasLevel level;
    char title[160];
    char detail[640];
    char fix[256];
} Check;

#define MAX_CHECKS 64
static Check checks[MAX_CHECKS];
static int nchecks;

static Check *add(const char *section, AtlasLevel lv, const char *title)
{
    Check *c;
    if (nchecks >= MAX_CHECKS)
        return &checks[MAX_CHECKS - 1];
    c = &checks[nchecks++];
    memset(c, 0, sizeof(*c));
    c->section = section;
    c->level = lv;
    snprintf(c->title, sizeof(c->title), "%s", title);
    return c;
}

static int count_lines(const char *text, const char *needle)
{
    int n = 0;
    const char *p = text;
    while (p && (p = strstr(p, needle))) {
        n++;
        p += strlen(needle);
    }
    return n;
}

static void check_kernel(void)
{
    char *out = NULL;
    int faults, suspends, dsp;
    Check *c;

    if (atlas_runl(&out, 8000, "journalctl", "-k", "-b", "--no-pager", "-o", "cat", NULL) != 0 || !out) {
        c = add("kernel", ATLAS_UNKNOWN, "Registro del kernel");
        snprintf(c->detail, sizeof(c->detail), "journalctl -k no disponible (¿grupo systemd-journal?)");
        free(out);
        return;
    }
    suspends = count_lines(out, "PM: suspend exit");
    dsp = count_lines(out, "ca0132 DSP downloaded and running");
    faults = 0;
    {
        char *line, *save = NULL;
        for (line = strtok_r(out, "\n", &save); line; line = strtok_r(NULL, "\n", &save))
            if (strstr(line, "snd_hda_intel") && strstr(line, "IO_PAGE_FAULT"))
                faults++;
    }
    c = add("kernel", ATLAS_OK, "Suspensiones en este arranque");
    snprintf(c->detail, sizeof(c->detail), "%d reanudación(es); DSP CA0132 recargado %d vez/veces",
             suspends, dsp);
    if (faults) {
        c = add("kernel", ATLAS_WARN, "IO_PAGE_FAULT de la tarjeta de sonido");
        snprintf(c->detail, sizeof(c->detail),
                 "%d fallo(s) de página IOMMU de snd_hda_intel, normalmente justo al reanudar. "
                 "Es la huella de BUG-001: el DMA de la tarjeta queda apuntando a memoria no mapeada. "
                 "Atlas lo mitiga cerrando el PCM antes de suspender (watch).", faults);
        snprintf(c->fix, sizeof(c->fix),
                 "opcional: probar el parámetro de kernel iommu=pt o quitar pcie_aspm=force");
    }
    free(out);
}

static void check_hwlock(void)
{
    char path[1024], vol[1024];
    const char *x = getenv("XDG_CONFIG_HOME"), *h = getenv("HOME");
    Check *c;

    snprintf(vol, sizeof(vol), "%s/hardware/ca0132/dsp/volumes.conf", atlas_config_dir());
    if (!atlas_file_exists(vol))
        return;   /* sin base de volumen importada no aplica */
    if (x && *x)
        snprintf(path, sizeof(path), "%s/wireplumber/wireplumber.conf.d/60-atlas-hw-mixer-lock.conf", x);
    else
        snprintf(path, sizeof(path), "%s/.config/wireplumber/wireplumber.conf.d/60-atlas-hw-mixer-lock.conf",
                 h ? h : "");
    if (atlas_file_exists(path)) {
        c = add("audio", ATLAS_OK, "Base de volumen protegida");
        snprintf(c->detail, sizeof(c->detail),
                 "PipeWire usa volumen por software; el mezclador de hardware conserva tu base");
    } else {
        c = add("audio", ATLAS_WARN, "Base de volumen sin proteger");
        snprintf(c->detail, sizeof(c->detail),
                 "PipeWire reescribe Master/PCM/Front al reconstruir la tarjeta (suspensión, "
                 "recuperación, auriculares)");
        snprintf(c->fix, sizeof(c->fix), "audio-profile base hw-lock on");
    }
}

static void check_watch(void)
{
    AtlasUnit u;
    Check *c;
    atlas_unit_query("audio-profile-watch.service", &u);
    if (u.loaded && !strcmp(u.active, "active")) {
        c = add("bug-001", ATLAS_OK, "Daemon de suspensión");
        snprintf(c->detail, sizeof(c->detail), "audio-profile-watch.service activo");
    } else {
        c = add("bug-001", ATLAS_WARN, "Daemon de suspensión");
        snprintf(c->detail, sizeof(c->detail), "audio-profile-watch.service %s: el audio no se "
                 "protege al suspender", u.loaded ? u.active : "no instalado");
        snprintf(c->fix, sizeof(c->fix), "systemctl --user enable --now audio-profile-watch.service");
    }
}

static void check_history(void)
{
    char path[1024], *data, *last = NULL, *line, *save = NULL;
    int total = 0, resume = 0, resume_ok = 0, resume_noaction = 0;
    Check *c;

    snprintf(path, sizeof(path), "%s/history.jsonl", atlas_state_dir());
    data = atlas_read_file(path, 8 << 20);
    if (!data) {
        c = add("bug-001", ATLAS_UNKNOWN, "Evidencia de ciclos de suspensión");
        snprintf(c->detail, sizeof(c->detail), "sin historial todavía; se registrará cada reanudación");
        return;
    }
    for (line = strtok_r(data, "\n", &save); line; line = strtok_r(NULL, "\n", &save)) {
        total++;
        last = line;
        if (strstr(line, "\"trigger\" : \"resume\"") || strstr(line, "\"trigger\":\"resume\"")) {
            resume++;
            if (strstr(line, "\"final_ok\" : true") || strstr(line, "\"final_ok\":true"))
                resume_ok++;
            if (strstr(line, "\"steps\" : [ ]") || strstr(line, "\"steps\" : []") ||
                strstr(line, "\"steps\":[]"))
                resume_noaction++;
        }
    }
    c = add("bug-001", resume && resume_ok < resume ? ATLAS_WARN : (resume ? ATLAS_OK : ATLAS_UNKNOWN),
            "Evidencia de ciclos de suspensión");
    snprintf(c->detail, sizeof(c->detail),
             "%d reanudación(es) registradas: %d con audio sano al final, %d sin necesitar acción. "
             "Criterio de cierre de BUG-001: varios ciclos seguidos sanos sin acción",
             resume, resume_ok, resume_noaction);
    if (resume == 0)
        snprintf(c->fix, sizeof(c->fix), "suspende y reanuda el equipo; luego: audio-profile history");
    (void)last;
    (void)total;
    free(data);
}

static void check_clock(void)
{
    AtlasClock k;
    Check *c;
    char prof[128];

    if (atlas_clock_query(&k) != 0) {
        c = add("pipewire", ATLAS_WARN, "Reloj de PipeWire");
        snprintf(c->detail, sizeof(c->detail), "pw-metadata no responde");
        return;
    }
    atlas_current_profile(prof, sizeof(prof));
    c = add("pipewire", ATLAS_OK, "Reloj de PipeWire");
    {
        char fr[32] = "automático", fq[32] = "automático";
        if (k.force_rate)
            snprintf(fr, sizeof(fr), "forzado a %d", k.force_rate);
        if (k.force_quantum)
            snprintf(fq, sizeof(fq), "forzado a %d", k.force_quantum);
        snprintf(c->detail, sizeof(c->detail),
                 "rate %s (base %d), quantum %s (base %d), permitidos %s · perfil %s",
                 fr, k.rate, fq, k.quantum, k.allowed_rates[0] ? k.allowed_rates : "?", prof);
    }
    if (k.force_rate && k.allowed_rates[0]) {
        char r[16];
        snprintf(r, sizeof(r), "%d", k.force_rate);
        if (!strstr(k.allowed_rates, r)) {
            c->level = ATLAS_WARN;
            snprintf(c->fix, sizeof(c->fix), "audio-profile reset");
        }
    }
    if (k.force_quantum && (k.force_quantum < k.min_quantum || k.force_quantum > k.max_quantum)) {
        c->level = ATLAS_WARN;
        snprintf(c->fix, sizeof(c->fix), "quantum forzado fuera de [%d, %d]: audio-profile reset",
                 k.min_quantum, k.max_quantum);
    }
}

static void check_hardware_db(void)
{
    AtlasHwDb *db = malloc(sizeof(*db));
    AtlasHwDevice devs[32];
    int n, i;

    if (!db)
        return;
    atlas_hwdb_load(db);
    n = atlas_hw_discover(devs, 32);
    for (i = 0; i < db->nerrors; i++) {
        Check *c = add("hardware", ATLAS_WARN, "Hardware DB");
        snprintf(c->detail, sizeof(c->detail), "%s", db->errors[i]);
        snprintf(c->fix, sizeof(c->fix), "audio-profile hardware-validate");
    }
    for (i = 0; i < n; i++) {
        const AtlasHwEntry *e, *e2;
        AtlasMatchResult r = atlas_hwdb_match(db, &devs[i], &e, &e2);
        Check *c;
        char title[160];
        snprintf(title, sizeof(title), "Tarjeta %d · %s", devs[i].alsa_card,
                 devs[i].codec[0] ? devs[i].codec : devs[i].longname);
        switch (r) {
        case ATLAS_MATCH_UNIQUE: {
            char kb[1024], *s;
            c = add("hardware", ATLAS_OK, title);
            snprintf(c->detail, sizeof(c->detail), "registrada como '%s' (%s)", e->id, devs[i].stable_key);
            snprintf(kb, sizeof(kb), "%s/known_bugs.json", e->dir);
            if ((s = atlas_read_file(kb, 1 << 16))) {
                int nb = count_lines(s, "\"id\"") + count_lines(s, "\"notes\"");
                if (nb)
                    snprintf(c->detail + strlen(c->detail), sizeof(c->detail) - strlen(c->detail),
                             " · %d aviso(s) conocidos: audio-profile hardware-info %s", nb, e->id);
                free(s);
            }
            break;
        }
        case ATLAS_MATCH_PARTIAL:
            c = add("hardware", ATLAS_WARN, title);
            snprintf(c->detail, sizeof(c->detail), "parecida a '%s' pero no idéntica (%s)", e->id,
                     devs[i].stable_key);
            snprintf(c->fix, sizeof(c->fix), "audio-profile hardware-register %d --link %s   o   --new",
                     devs[i].alsa_card, e->id);
            break;
        case ATLAS_MATCH_AMBIGUOUS:
            c = add("hardware", ATLAS_FAIL, title);
            snprintf(c->detail, sizeof(c->detail), "coincide con '%s' y '%s'", e->id, e2 ? e2->id : "?");
            snprintf(c->fix, sizeof(c->fix), "audio-profile hardware-validate");
            break;
        default:
            c = add("hardware", ATLAS_UNKNOWN, title);
            snprintf(c->detail, sizeof(c->detail), "no registrada (%s)", devs[i].stable_key);
            snprintf(c->fix, sizeof(c->fix), "audio-profile hardware-register %d", devs[i].alsa_card);
            break;
        }
        if (devs[i].missing[0]) {
            c = add("hardware", ATLAS_WARN, title);
            snprintf(c->detail, sizeof(c->detail), "no se pudieron leer: %s", devs[i].missing);
            snprintf(c->fix, sizeof(c->fix), "udevadm info -a /sys/class/sound/card%d | head -40",
                     devs[i].alsa_card);
        }
    }
    free(db);
}

static void check_failed_units(void)
{
    char *out = NULL;
    Check *c;
    if (atlas_runl(&out, 4000, "systemctl", "--user", "--failed", "--plain", "--no-legend", NULL) != 0) {
        free(out);
        return;
    }
    if (out && out[0]) {
        char *p;
        for (p = out; *p; p++)
            if (*p == '\n')
                *p = ';';
        c = add("servicios", ATLAS_WARN, "Unidades de usuario fallidas");
        snprintf(c->detail, sizeof(c->detail), "%s", out);
        snprintf(c->fix, sizeof(c->fix), "systemctl --user --failed");
    }
    free(out);
}

int atlas_cmd_doctor(void)
{
    AtlasHealth *h = malloc(sizeof(*h));
    AtlasAutoState as;
    Check *c;
    int i, fails = 0, warns = 0;
    const char *last = "";

    if (!h)
        return 2;
    nchecks = 0;
    atlas_health_check(h, 1);

    for (i = 0; i < h->nunits; i++) {
        c = add("servicios", strcmp(h->units[i].active, "active") ? ATLAS_FAIL : ATLAS_OK, h->units[i].unit);
        snprintf(c->detail, sizeof(c->detail), "%s (%s)", h->units[i].active, h->units[i].sub);
        if (c->level == ATLAS_FAIL)
            snprintf(c->fix, sizeof(c->fix), "audio-profile recover");
    }
    for (i = 0; i < h->nportals; i++) {
        int okp = !strcmp(h->portals[i].active, "active");
        c = add("servicios", okp ? ATLAS_OK : ATLAS_WARN, h->portals[i].unit);
        snprintf(c->detail, sizeof(c->detail), "%s%s%s — se cae cuando PipeWire se detiene; "
                 "afecta a compartir pantalla, no al sonido", h->portals[i].active,
                 h->portals[i].result[0] ? " · " : "", h->portals[i].result);
        if (!okp)
            snprintf(c->fix, sizeof(c->fix), "systemctl --user reset-failed %s && systemctl --user restart %s",
                     h->portals[i].unit, h->portals[i].unit);
    }
    check_failed_units();

    c = add("audio", h->connection, "Conexión (pipewire-pulse y sink por defecto)");
    snprintf(c->detail, sizeof(c->detail), "%s", h->connection_detail);
    if (h->connection == ATLAS_FAIL)
        snprintf(c->fix, sizeof(c->fix), "audio-profile recover");

    c = add("audio", h->hardware, "Reproducción en hardware (sonda hw_ptr)");
    snprintf(c->detail, sizeof(c->detail), "%s", h->hardware_detail);
    if (h->hardware == ATLAS_FAIL)
        snprintf(c->fix, sizeof(c->fix), "audio-profile recover");

    c = add("audio", ATLAS_UNKNOWN, "Audibilidad");
    snprintf(c->detail, sizeof(c->detail), "no medible automáticamente (cables, amplificador, volumen físico)");
    snprintf(c->fix, sizeof(c->fix), "audio-profile test-tone");

    check_hwlock();
    check_clock();
    check_hardware_db();
    check_kernel();
    check_watch();
    check_history();

    atlas_auto_state_load(&as);
    c = add("perfiles", ATLAS_OK, "Perfiles automáticos");
    snprintf(c->detail, sizeof(c->detail), "%s%s · actual %s", as.enabled ? "activados" : "desactivados",
             as.manual_lock ? " (en pausa por selección manual)" : "", as.current[0] ? as.current : "-");
    if (as.manual_lock)
        snprintf(c->fix, sizeof(c->fix), "audio-profile auto on");

    for (i = 0; i < nchecks; i++) {
        if (checks[i].level == ATLAS_FAIL)
            fails++;
        else if (checks[i].level == ATLAS_WARN)
            warns++;
    }

    if (atlas_json_mode) {
        JsonBuilder *b = json_builder_new();
        json_builder_begin_object(b);
        atlas_json_kv_int(b, "fails", fails);
        atlas_json_kv_int(b, "warnings", warns);
        json_builder_set_member_name(b, "checks");
        json_builder_begin_array(b);
        for (i = 0; i < nchecks; i++) {
            json_builder_begin_object(b);
            atlas_json_kv_str(b, "section", checks[i].section);
            atlas_json_kv_str(b, "level", atlas_level_name(checks[i].level));
            atlas_json_kv_str(b, "title", checks[i].title);
            atlas_json_kv_str(b, "detail", checks[i].detail);
            atlas_json_kv_str(b, "fix", checks[i].fix);
            json_builder_end_object(b);
        }
        json_builder_end_array(b);
        json_builder_set_member_name(b, "health");
        atlas_health_json(b, h);
        json_builder_end_object(b);
        atlas_json_print(b);
        free(h);
        return fails ? 1 : 0;
    }

    puts("Project Atlas · Doctor");
    puts("======================");
    for (i = 0; i < nchecks; i++) {
        if (strcmp(last, checks[i].section)) {
            printf("\n[%s]\n", checks[i].section);
            last = checks[i].section;
        }
        printf("  %s %s\n", atlas_level_icon(checks[i].level), checks[i].title);
        if (checks[i].detail[0])
            printf("      %s\n", checks[i].detail);
        if (checks[i].fix[0])
            printf("      → %s\n", checks[i].fix);
    }
    printf("\nResumen: %d fallo(s), %d advertencia(s)\n", fails, warns);
    free(h);
    return fails ? 1 : 0;
}
