/*
 * Pruebas de la sonda funcional (BUG-001) y del lector de reloj.
 * Los textos de /proc son capturas reales de la Sound Blaster Z.
 */
#include "test_fixture.h"
#include "test_common.h"

#include "atlas.h"
#include "atlas_health.h"
#include "atlas_probe.h"

/* Estado capturado el 2026-11-03 tras reanudar: RUNNING pero DMA parado. */
static const char *STALLED =
    "state: RUNNING\n"
    "owner_pid   : 2561780\n"
    "trigger_time: 183051.695983075\n"
    "tstamp      : 183412.628807870\n"
    "delay       : 1906\n"
    "avail       : 31744\n"
    "avail_max   : 0\n"
    "-----\n"
    "hw_ptr      : 0\n"
    "appl_ptr    : 1024\n";

static const char *HW_PARAMS =
    "access: MMAP_INTERLEAVED\nformat: S16_LE\nsubformat: STD\nchannels: 6\n"
    "rate: 48000 (48000/1)\nperiod_size: 1024\nbuffer_size: 32768\n";

static AtlasPcmStatus st(AtlasPcmState s, unsigned long hw)
{
    AtlasPcmStatus x;
    memset(&x, 0, sizeof(x));
    x.valid = 1;
    x.state = s;
    x.hw_ptr = hw;
    return x;
}

static void test_parse(void)
{
    AtlasPcmStatus s;

    TEST_ASSERT_EQ(atlas_pcm_parse(STALLED, &s), 0);
    TEST_ASSERT_EQ_INT(ATLAS_PCM_RUNNING, s.state);
    TEST_ASSERT_EQ_INT(0, s.hw_ptr);
    TEST_ASSERT_EQ_INT(1024, s.appl_ptr);
    TEST_ASSERT_EQ_INT(2561780, s.owner_pid);

    TEST_ASSERT_EQ(atlas_pcm_parse("closed\n", &s), 0);
    TEST_ASSERT_EQ_INT(ATLAS_PCM_CLOSED, s.state);

    TEST_ASSERT_EQ(atlas_pcm_parse("state: PREPARED\nowner_pid : 1\n", &s), 0);
    TEST_ASSERT_EQ_INT(ATLAS_PCM_OPEN, s.state);

    TEST_ASSERT_EQ(atlas_pcm_parse("state: XRUN\n", &s), 0);
    TEST_ASSERT_EQ_INT(ATLAS_PCM_XRUN, s.state);

    TEST_ASSERT_EQ(atlas_pcm_parse("basura", &s), -1);
    TEST_ASSERT_EQ(atlas_pcm_parse(NULL, &s), -1);

    TEST_ASSERT_EQ_INT(48000, atlas_pcm_parse_rate(HW_PARAMS));
    TEST_ASSERT_EQ_INT(0, atlas_pcm_parse_rate("closed\n"));
}

static void test_classify(void)
{
    AtlasPcmStatus a, b;
    double ratio;

    a = st(ATLAS_PCM_RUNNING, 1000);
    b = st(ATLAS_PCM_RUNNING, 1000 + 19200);
    TEST_ASSERT_EQ_INT(ATLAS_FLOW_ADVANCING, atlas_pcm_classify(&a, &b, 48000, 400, &ratio));
    TEST_ASSERT(ratio > 0.99 && ratio < 1.01);

    /* BUG-001: hw_ptr no se mueve */
    a = st(ATLAS_PCM_RUNNING, 0);
    b = st(ATLAS_PCM_RUNNING, 0);
    TEST_ASSERT_EQ_INT(ATLAS_FLOW_STALLED, atlas_pcm_classify(&a, &b, 48000, 400, NULL));

    /* avanza muy despacio (reloj roto) */
    a = st(ATLAS_PCM_RUNNING, 0);
    b = st(ATLAS_PCM_RUNNING, 1000);
    TEST_ASSERT_EQ_INT(ATLAS_FLOW_SLOW, atlas_pcm_classify(&a, &b, 48000, 400, NULL));

    /* sin rate conocido no se juzga la velocidad */
    TEST_ASSERT_EQ_INT(ATLAS_FLOW_ADVANCING, atlas_pcm_classify(&a, &b, 0, 400, NULL));

    a = st(ATLAS_PCM_CLOSED, 0);
    b = st(ATLAS_PCM_CLOSED, 0);
    TEST_ASSERT_EQ_INT(ATLAS_FLOW_NOT_RUNNING, atlas_pcm_classify(&a, &b, 48000, 400, NULL));

    /* vuelta del contador: se considera movimiento */
    a = st(ATLAS_PCM_RUNNING, 5000);
    b = st(ATLAS_PCM_RUNNING, 10);
    TEST_ASSERT_EQ_INT(ATLAS_FLOW_ADVANCING, atlas_pcm_classify(&a, &b, 48000, 400, NULL));

    a.valid = 0;
    TEST_ASSERT_EQ_INT(ATLAS_FLOW_UNKNOWN, atlas_pcm_classify(&a, &b, 48000, 400, NULL));
}

static AtlasNode alsa_sink(int card)
{
    AtlasNode n;
    memset(&n, 0, sizeof(n));
    snprintf(n.name, sizeof(n.name), "alsa_output.pci-0000_27_00.0.analog-surround-51");
    snprintf(n.description, sizeof(n.description), "CA0132 Analog Surround 5.1");
    snprintf(n.api, sizeof(n.api), "alsa");
    n.alsa_card = card;
    n.alsa_device = 0;
    return n;
}

/* Regresión BUG-001: con el PCM congelado la sonda DEBE fallar aunque el
 * estado diga RUNNING (antes status/doctor daban falso positivo). */
static void test_probe_detects_bug001(void)
{
    AtlasProbe p;
    AtlasNode sink = alsa_sink(0);

    fx_setup();
    fx_write("asound/card0/pcm0p/sub0/status", STALLED);
    fx_write("asound/card0/pcm0p/sub0/hw_params", HW_PARAMS);
    setenv("ATLAS_PROC_ROOT", fx_root, 1);

    TEST_ASSERT_EQ(atlas_probe_sink(&sink, 0, &p), 0);
    TEST_ASSERT_EQ_INT(ATLAS_PROBE_STALLED, p.result);
    TEST_ASSERT_EQ_INT(0, p.active);
    TEST_ASSERT(strstr(p.detail, "hw_ptr no se mueve") != NULL);

    /* scan global: también lo marca como atascado */
    {
        AtlasPcmFlowInfo f[4];
        int n = atlas_pcm_scan_running(f, 4, 20);
        TEST_ASSERT_EQ_INT(1, n);
        TEST_ASSERT_EQ_INT(ATLAS_FLOW_STALLED, f[0].flow);
        TEST_ASSERT_EQ_INT(0, f[0].card);
    }

    unsetenv("ATLAS_PROC_ROOT");
    fx_teardown();
}

static void test_probe_idle_and_unsupported(void)
{
    AtlasProbe p;
    AtlasNode sink = alsa_sink(0);
    AtlasNode bt;

    fx_setup();
    fx_write("asound/card0/pcm0p/sub0/status", "closed\n");
    setenv("ATLAS_PROC_ROOT", fx_root, 1);

    /* sin reproducción y sin sonda activa: no se declara sano ni roto */
    atlas_probe_sink(&sink, 0, &p);
    TEST_ASSERT_EQ_INT(ATLAS_PROBE_IDLE, p.result);

    /* tarjeta desaparecida */
    sink.alsa_card = 5;
    atlas_probe_sink(&sink, 0, &p);
    TEST_ASSERT_EQ_INT(ATLAS_PROBE_NOT_OPENED, p.result);

    memset(&bt, 0, sizeof(bt));
    snprintf(bt.name, sizeof(bt.name), "bluez_output.AA_BB");
    snprintf(bt.api, sizeof(bt.api), "bluez5");
    bt.alsa_card = -1;
    atlas_probe_sink(&bt, 1, &p);
    TEST_ASSERT_EQ_INT(ATLAS_PROBE_UNSUPPORTED, p.result);

    TEST_ASSERT_EQ(atlas_probe_sink(NULL, 1, &p), -1);
    TEST_ASSERT_EQ_INT(ATLAS_PROBE_NO_SINK, p.result);

    unsetenv("ATLAS_PROC_ROOT");
    fx_teardown();
}

/* Salida real de pw-metadata: el formato value:'0' type:'' es el que el
 * status v0.1 mostraba mal (la "anomalía" de clock.force-rate). */
static void test_clock_parse(void)
{
    AtlasClock c;
    const char *out =
        "Found \"settings\" metadata 30\n"
        "update: id:0 key:'log.level' value:'2' type:''\n"
        "update: id:0 key:'clock.rate' value:'48000' type:''\n"
        "update: id:0 key:'clock.allowed-rates' value:'[ 48000, 44100 ]' type:''\n"
        "update: id:0 key:'clock.quantum' value:'64' type:''\n"
        "update: id:0 key:'clock.min-quantum' value:'32' type:''\n"
        "update: id:0 key:'clock.max-quantum' value:'1024' type:''\n"
        "update: id:0 key:'clock.force-quantum' value:'0' type:''\n"
        "update: id:0 key:'clock.force-rate' value:'0' type:''\n";

    TEST_ASSERT_EQ(atlas_clock_parse(out, &c), 0);
    TEST_ASSERT_EQ_INT(48000, c.rate);
    TEST_ASSERT_EQ_INT(64, c.quantum);
    TEST_ASSERT_EQ_INT(32, c.min_quantum);
    TEST_ASSERT_EQ_INT(1024, c.max_quantum);
    TEST_ASSERT_EQ_INT(0, c.force_rate);
    TEST_ASSERT_EQ_INT(0, c.force_quantum);
    TEST_ASSERT_EQ_STR("[ 48000, 44100 ]", c.allowed_rates);

    TEST_ASSERT_EQ(atlas_clock_parse("", &c), -1);
}

int main(void)
{
    test_parse();
    test_classify();
    test_probe_detects_bug001();
    test_probe_idle_and_unsupported();
    test_clock_parse();
    return test_summary();
}
