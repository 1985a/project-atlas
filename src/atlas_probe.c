#define _GNU_SOURCE
#include <fcntl.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "atlas.h"
#include "atlas_probe.h"

/* ================================================== funciones puras */

const char *atlas_pcm_state_name(AtlasPcmState s)
{
    switch (s) {
    case ATLAS_PCM_CLOSED: return "closed";
    case ATLAS_PCM_OPEN: return "open";
    case ATLAS_PCM_RUNNING: return "running";
    case ATLAS_PCM_XRUN: return "xrun";
    case ATLAS_PCM_DRAINING: return "draining";
    case ATLAS_PCM_PAUSED: return "paused";
    case ATLAS_PCM_SUSPENDED: return "suspended";
    case ATLAS_PCM_DISCONNECTED: return "disconnected";
    default: return "unknown";
    }
}

const char *atlas_flow_name(AtlasFlow f)
{
    switch (f) {
    case ATLAS_FLOW_ADVANCING: return "advancing";
    case ATLAS_FLOW_STALLED: return "stalled";
    case ATLAS_FLOW_SLOW: return "slow";
    case ATLAS_FLOW_NOT_RUNNING: return "not-running";
    default: return "unknown";
    }
}

const char *atlas_probe_result_name(AtlasProbeResult r)
{
    switch (r) {
    case ATLAS_PROBE_OK: return "ok";
    case ATLAS_PROBE_STALLED: return "stalled";
    case ATLAS_PROBE_NOT_OPENED: return "not-opened";
    case ATLAS_PROBE_START_FAILED: return "start-failed";
    case ATLAS_PROBE_UNSUPPORTED: return "unsupported";
    case ATLAS_PROBE_IDLE: return "idle";
    case ATLAS_PROBE_NO_SINK: return "no-sink";
    }
    return "unknown";
}

static AtlasPcmState state_from(const char *s)
{
    if (!strncmp(s, "RUNNING", 7)) return ATLAS_PCM_RUNNING;
    if (!strncmp(s, "PREPARED", 8) || !strncmp(s, "SETUP", 5) || !strncmp(s, "OPEN", 4))
        return ATLAS_PCM_OPEN;
    if (!strncmp(s, "XRUN", 4)) return ATLAS_PCM_XRUN;
    if (!strncmp(s, "DRAINING", 8)) return ATLAS_PCM_DRAINING;
    if (!strncmp(s, "PAUSED", 6)) return ATLAS_PCM_PAUSED;
    if (!strncmp(s, "SUSPENDED", 9)) return ATLAS_PCM_SUSPENDED;
    if (!strncmp(s, "DISCONNECTED", 12)) return ATLAS_PCM_DISCONNECTED;
    return ATLAS_PCM_UNKNOWN;
}

int atlas_pcm_parse(const char *text, AtlasPcmStatus *st)
{
    const char *p;

    memset(st, 0, sizeof(*st));
    st->state = ATLAS_PCM_UNKNOWN;
    if (!text)
        return -1;

    while (*text == ' ' || *text == '\n' || *text == '\t')
        text++;

    if (!strncmp(text, "closed", 6)) {
        st->valid = 1;
        st->state = ATLAS_PCM_CLOSED;
        return 0;
    }

    p = strstr(text, "state:");
    if (!p)
        return -1;
    p += 6;
    while (*p == ' ')
        p++;
    st->state = state_from(p);

    if ((p = strstr(text, "owner_pid")) && (p = strchr(p, ':')))
        st->owner_pid = atoi(p + 1);
    if ((p = strstr(text, "hw_ptr")) && (p = strchr(p, ':')))
        st->hw_ptr = strtoul(p + 1, NULL, 10);
    if ((p = strstr(text, "appl_ptr")) && (p = strchr(p, ':')))
        st->appl_ptr = strtoul(p + 1, NULL, 10);

    st->valid = 1;
    return 0;
}

int atlas_pcm_parse_rate(const char *hw)
{
    const char *p;
    if (!hw || !(p = strstr(hw, "rate:")))
        return 0;
    return atoi(p + 5);
}

AtlasFlow atlas_pcm_classify(const AtlasPcmStatus *a, const AtlasPcmStatus *b,
                             int rate, int window_ms, double *ratio)
{
    unsigned long d;

    if (ratio)
        *ratio = 0;
    if (!a || !b || !a->valid || !b->valid)
        return ATLAS_FLOW_UNKNOWN;
    if (a->state != ATLAS_PCM_RUNNING || b->state != ATLAS_PCM_RUNNING)
        return ATLAS_FLOW_NOT_RUNNING;
    if (b->hw_ptr == a->hw_ptr)
        return ATLAS_FLOW_STALLED;

    /* hw_ptr crece de forma monótona hasta el límite (boundary) y da la vuelta */
    d = b->hw_ptr > a->hw_ptr ? b->hw_ptr - a->hw_ptr : 0;
    if (d == 0)
        return ATLAS_FLOW_ADVANCING;    /* vuelta del contador: se movió */

    if (rate > 0 && window_ms > 0) {
        double expected = (double)rate * window_ms / 1000.0;
        double r = d / expected;
        if (ratio)
            *ratio = r;
        if (r < 0.25)
            return ATLAS_FLOW_SLOW;
    }
    return ATLAS_FLOW_ADVANCING;
}

/* ================================================== lectura de /proc */

static const char *proc_root(void)
{
    const char *r = getenv("ATLAS_PROC_ROOT");
    return (r && *r) ? r : "/proc";
}

int atlas_pcm_read(int card, int device, int sub, AtlasPcmStatus *st)
{
    char path[512];
    char *text;
    int r;

    snprintf(path, sizeof(path), "%s/asound/card%d/pcm%dp/sub%d/status",
             proc_root(), card, device, sub);
    text = atlas_read_file(path, 4096);
    if (!text) {
        memset(st, 0, sizeof(*st));
        st->state = ATLAS_PCM_UNKNOWN;
        return -1;
    }
    r = atlas_pcm_parse(text, st);
    free(text);
    return r;
}

int atlas_pcm_read_any(int card, int device, int *sub_out, AtlasPcmStatus *st)
{
    int sub, first_ok = -1;
    AtlasPcmStatus tmp;

    for (sub = 0; sub < 8; sub++) {
        if (atlas_pcm_read(card, device, sub, &tmp) != 0)
            break;
        if (first_ok < 0) {
            first_ok = sub;
            *st = tmp;
        }
        if (tmp.state == ATLAS_PCM_RUNNING) {
            *st = tmp;
            if (sub_out)
                *sub_out = sub;
            return 0;
        }
    }
    if (first_ok < 0)
        return -1;
    if (sub_out)
        *sub_out = first_ok;
    return 0;
}

int atlas_pcm_rate(int card, int device, int sub)
{
    char path[512];
    char *text;
    int r;

    snprintf(path, sizeof(path), "%s/asound/card%d/pcm%dp/sub%d/hw_params",
             proc_root(), card, device, sub);
    text = atlas_read_file(path, 4096);
    r = atlas_pcm_parse_rate(text);
    free(text);
    return r;
}

int atlas_pcm_scan_running(AtlasPcmFlowInfo *out, int max, int window_ms)
{
    AtlasPcmStatus first[64];
    int cards[64], devs[64], subs[64], rates[64];
    int n = 0, i, card, dev, sub;

    for (card = 0; card < 16; card++)
        for (dev = 0; dev < 16; dev++)
            for (sub = 0; sub < 8 && n < 64; sub++) {
                AtlasPcmStatus st;
                if (atlas_pcm_read(card, dev, sub, &st) != 0)
                    break;
                if (st.state != ATLAS_PCM_RUNNING)
                    continue;
                first[n] = st;
                cards[n] = card;
                devs[n] = dev;
                subs[n] = sub;
                rates[n] = atlas_pcm_rate(card, dev, sub);
                n++;
            }

    if (n == 0)
        return 0;
    atlas_msleep(window_ms);

    for (i = 0; i < n && i < max; i++) {
        AtlasPcmStatus st;
        atlas_pcm_read(cards[i], devs[i], subs[i], &st);
        out[i].card = cards[i];
        out[i].device = devs[i];
        out[i].sub = subs[i];
        out[i].flow = atlas_pcm_classify(&first[i], &st, rates[i], window_ms, NULL);
        out[i].frames = st.hw_ptr >= first[i].hw_ptr ? st.hw_ptr - first[i].hw_ptr : 0;
    }
    return i;
}

/* ===================================================== sonda completa */

static void measure(AtlasProbe *p)
{
    AtlasFlow f;

    p->rate = atlas_pcm_rate(p->card, p->device, p->sub);
    atlas_msleep(p->window_ms);
    atlas_pcm_read(p->card, p->device, p->sub, &p->after);
    f = atlas_pcm_classify(&p->before, &p->after, p->rate, p->window_ms, &p->ratio);
    p->frames = p->after.hw_ptr >= p->before.hw_ptr ? p->after.hw_ptr - p->before.hw_ptr : 0;

    switch (f) {
    case ATLAS_FLOW_ADVANCING:
        p->result = ATLAS_PROBE_OK;
        snprintf(p->detail, sizeof(p->detail),
                 "hw:%d,%d sub%d avanza %lu frames en %d ms (%d Hz)%s",
                 p->card, p->device, p->sub, p->frames, p->window_ms, p->rate,
                 p->active ? " con flujo de silencio" : " con la reproducción actual");
        break;
    case ATLAS_FLOW_SLOW:
        p->result = ATLAS_PROBE_STALLED;
        snprintf(p->detail, sizeof(p->detail),
                 "hw:%d,%d avanza demasiado lento: %lu frames en %d ms (%.0f%% de lo esperado)",
                 p->card, p->device, p->frames, p->window_ms, p->ratio * 100);
        break;
    case ATLAS_FLOW_STALLED:
        p->result = ATLAS_PROBE_STALLED;
        snprintf(p->detail, sizeof(p->detail),
                 "hw:%d,%d sub%d está RUNNING pero hw_ptr no se mueve (%lu); appl_ptr=%lu. "
                 "El hardware no consume audio (BUG-001)",
                 p->card, p->device, p->sub, p->after.hw_ptr, p->after.appl_ptr);
        break;
    default:
        p->result = ATLAS_PROBE_NOT_OPENED;
        snprintf(p->detail, sizeof(p->detail),
                 "hw:%d,%d dejó de estar RUNNING durante la medición (%s)",
                 p->card, p->device, atlas_pcm_state_name(p->after.state));
        break;
    }
}

int atlas_probe_sink(const AtlasNode *sink, int allow_active, AtlasProbe *p)
{
    int pid = -1, zero_fd;
    long long deadline;
    const char *argv[24];
    int n = 0;

    memset(p, 0, sizeof(*p));
    p->window_ms = atlas_conf_int("probe.window_ms");
    if (p->window_ms < 100 || p->window_ms > 3000)
        p->window_ms = 400;

    if (!sink) {
        p->result = ATLAS_PROBE_NO_SINK;
        snprintf(p->detail, sizeof(p->detail), "no hay sink por defecto");
        return -1;
    }
    snprintf(p->sink, sizeof(p->sink), "%s", sink->name);
    p->card = sink->alsa_card;
    p->device = sink->alsa_device < 0 ? 0 : sink->alsa_device;

    if (sink->alsa_card < 0) {
        p->result = ATLAS_PROBE_UNSUPPORTED;
        snprintf(p->detail, sizeof(p->detail),
                 "el sink '%s' no es ALSA (api=%s); no se puede medir el hardware",
                 sink->description, sink->api[0] ? sink->api : "?");
        return 0;
    }

    if (atlas_pcm_read_any(p->card, p->device, &p->sub, &p->before) != 0) {
        p->result = ATLAS_PROBE_NOT_OPENED;
        snprintf(p->detail, sizeof(p->detail),
                 "no existe /proc/asound/card%d/pcm%dp (¿tarjeta desaparecida?)", p->card, p->device);
        return 0;
    }

    /* Caso 1: ya hay reproducción, medición pasiva */
    if (p->before.state == ATLAS_PCM_RUNNING) {
        measure(p);
        return 0;
    }

    if (!allow_active) {
        p->result = ATLAS_PROBE_IDLE;
        snprintf(p->detail, sizeof(p->detail),
                 "hw:%d,%d está %s; no hay reproducción que medir (usa --probe para enviar silencio)",
                 p->card, p->device, atlas_pcm_state_name(p->before.state));
        return 0;
    }

    /* Caso 2: flujo de silencio hacia el sink */
    p->active = 1;
    if (!atlas_have_cmd("pw-cat")) {
        p->result = ATLAS_PROBE_START_FAILED;
        snprintf(p->detail, sizeof(p->detail), "falta pw-cat (paquete pipewire)");
        return 0;
    }

    zero_fd = open("/dev/zero", O_RDONLY | O_CLOEXEC);
    argv[n++] = "pw-cat";
    argv[n++] = "--playback";
    argv[n++] = "--target";
    argv[n++] = sink->name;
    argv[n++] = "--raw";
    argv[n++] = "--rate";
    argv[n++] = "48000";
    argv[n++] = "--channels";
    argv[n++] = "2";
    argv[n++] = "--format";
    argv[n++] = "s16";
    argv[n++] = "--media-role";
    argv[n++] = "Notification";
    argv[n++] = "-P";
    argv[n++] = "{ node.description=\"Atlas probe\" node.dont-reconnect=true media.name=\"Atlas silent probe\" }";
    argv[n++] = "-";
    argv[n] = NULL;

    if (zero_fd < 0 || atlas_spawn(argv, zero_fd, &pid) != 0) {
        if (zero_fd >= 0)
            close(zero_fd);
        p->result = ATLAS_PROBE_START_FAILED;
        snprintf(p->detail, sizeof(p->detail), "no se pudo lanzar pw-cat");
        return 0;
    }
    close(zero_fd);

    deadline = atlas_now_ms() + 3000;
    while (atlas_now_ms() < deadline) {
        atlas_msleep(100);
        if (atlas_spawn_wait(pid, 1) != 124) {
            pid = -1;
            p->result = ATLAS_PROBE_START_FAILED;
            snprintf(p->detail, sizeof(p->detail),
                     "pw-cat terminó antes de tiempo (¿PipeWire no acepta clientes?)");
            return 0;
        }
        if (atlas_pcm_read_any(p->card, p->device, &p->sub, &p->before) == 0 &&
            p->before.state == ATLAS_PCM_RUNNING)
            break;
    }

    if (p->before.state != ATLAS_PCM_RUNNING) {
        atlas_spawn_kill(pid);
        p->result = ATLAS_PROBE_NOT_OPENED;
        snprintf(p->detail, sizeof(p->detail),
                 "se envió silencio a '%s' durante 3 s pero hw:%d,%d nunca pasó a RUNNING (%s). "
                 "El grafo no está llevando audio al hardware",
                 sink->description, p->card, p->device, atlas_pcm_state_name(p->before.state));
        return 0;
    }

    /* dejar que arranque el DMA antes de medir */
    atlas_msleep(150);
    atlas_pcm_read(p->card, p->device, p->sub, &p->before);
    measure(p);
    atlas_spawn_kill(pid);
    return 0;
}

void atlas_probe_json(JsonBuilder *b, const AtlasProbe *p)
{
    json_builder_begin_object(b);
    atlas_json_kv_str(b, "result", atlas_probe_result_name(p->result));
    atlas_json_kv_bool(b, "active", p->active);
    atlas_json_kv_str(b, "sink", p->sink);
    atlas_json_kv_int(b, "card", p->card);
    atlas_json_kv_int(b, "device", p->device);
    atlas_json_kv_int(b, "sub", p->sub);
    atlas_json_kv_int(b, "frames", (long long)p->frames);
    atlas_json_kv_int(b, "window_ms", p->window_ms);
    atlas_json_kv_int(b, "rate", p->rate);
    atlas_json_kv_double(b, "ratio", p->ratio);
    atlas_json_kv_str(b, "state_before", atlas_pcm_state_name(p->before.state));
    atlas_json_kv_str(b, "state_after", atlas_pcm_state_name(p->after.state));
    atlas_json_kv_int(b, "hw_ptr_before", (long long)p->before.hw_ptr);
    atlas_json_kv_int(b, "hw_ptr_after", (long long)p->after.hw_ptr);
    atlas_json_kv_str(b, "detail", p->detail);
    json_builder_end_object(b);
}

/* ========================================================= tono audible */

int atlas_test_tone(const char *sink, double seconds, double freq, double gain)
{
    int pipefd[2], pid, rate = 48000, i, total, fade;
    const char *argv[16];
    int n = 0, status;
    short buf[2 * 1024];
    void (*old)(int);

    if (seconds <= 0 || seconds > 10)
        seconds = 1.0;
    if (gain <= 0 || gain > 0.5)
        gain = 0.12;

    if (pipe2(pipefd, O_CLOEXEC) != 0)
        return -1;

    argv[n++] = "pw-cat";
    argv[n++] = "--playback";
    if (sink && *sink) {
        argv[n++] = "--target";
        argv[n++] = sink;
    }
    argv[n++] = "--raw";
    argv[n++] = "--rate";
    argv[n++] = "48000";
    argv[n++] = "--channels";
    argv[n++] = "2";
    argv[n++] = "--format";
    argv[n++] = "s16";
    argv[n++] = "-P";
    argv[n++] = "{ node.description=\"Atlas test tone\" media.name=\"Atlas test tone\" }";
    argv[n++] = "-";
    argv[n] = NULL;

    if (atlas_spawn(argv, pipefd[0], &pid) != 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        return -1;
    }
    close(pipefd[0]);

    old = signal(SIGPIPE, SIG_IGN);
    total = (int)(seconds * rate);
    fade = rate / 20;
    for (i = 0; i < total;) {
        int k, chunk = total - i < 1024 ? total - i : 1024;
        for (k = 0; k < chunk; k++, i++) {
            double env = 1.0;
            short s;
            if (i < fade)
                env = (double)i / fade;
            else if (total - i < fade)
                env = (double)(total - i) / fade;
            s = (short)(sin(2 * M_PI * freq * i / rate) * gain * env * 32767);
            buf[2 * k] = s;
            buf[2 * k + 1] = s;
        }
        if (write(pipefd[1], buf, (size_t)chunk * 4) < 0)
            break;
    }
    /* medio segundo de silencio para que no se corte la cola */
    memset(buf, 0, sizeof(buf));
    for (i = 0; i < rate / 2; i += 1024)
        if (write(pipefd[1], buf, sizeof(buf)) < 0)
            break;
    close(pipefd[1]);
    signal(SIGPIPE, old);

    status = atlas_spawn_wait(pid, (int)(seconds * 1000) + 5000);
    if (status == 124)
        atlas_spawn_kill(pid);
    return status == 0 ? 0 : -1;
}
