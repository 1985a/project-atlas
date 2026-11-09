#ifndef ATLAS_PROBE_H
#define ATLAS_PROBE_H

/*
 * Sonda funcional de reproducción (BUG-001).
 *
 * "Servicio activo", "pactl responde" y "sink RUNNING" ya dieron falsos
 * positivos. Esta sonda mira el hardware: lee hw_ptr de
 * /proc/asound/cardN/pcmDp/subS/status dos veces y comprueba que el DMA de
 * la tarjeta realmente consume muestras. Si no hay reproducción, envía
 * SILENCIO al sink durante ~1 s para forzar la apertura del dispositivo
 * (inaudible, no invasivo).
 *
 * Límite honesto: hw_ptr avanzando demuestra que el hardware reproduce,
 * no que se oiga (cables, amplificador, DSP en mute). Para eso existe
 * `audio-profile test-tone`.
 */

#include <json-glib/json-glib.h>

#include "atlas_audio.h"

typedef enum {
    ATLAS_PCM_CLOSED = 0,
    ATLAS_PCM_OPEN,          /* SETUP / PREPARED */
    ATLAS_PCM_RUNNING,
    ATLAS_PCM_XRUN,
    ATLAS_PCM_DRAINING,
    ATLAS_PCM_PAUSED,
    ATLAS_PCM_SUSPENDED,
    ATLAS_PCM_DISCONNECTED,
    ATLAS_PCM_UNKNOWN
} AtlasPcmState;

typedef struct {
    int valid;
    AtlasPcmState state;
    unsigned long hw_ptr;
    unsigned long appl_ptr;
    int owner_pid;
} AtlasPcmStatus;

typedef enum {
    ATLAS_FLOW_ADVANCING = 0,
    ATLAS_FLOW_STALLED,       /* RUNNING pero hw_ptr no se mueve */
    ATLAS_FLOW_SLOW,          /* avanza muy por debajo de la frecuencia */
    ATLAS_FLOW_NOT_RUNNING,
    ATLAS_FLOW_UNKNOWN
} AtlasFlow;

typedef enum {
    ATLAS_PROBE_OK = 0,          /* hardware consume muestras */
    ATLAS_PROBE_STALLED,         /* BUG-001: dispositivo atascado */
    ATLAS_PROBE_NOT_OPENED,      /* el stream de prueba nunca abrió el PCM */
    ATLAS_PROBE_START_FAILED,    /* no se pudo lanzar/conectar pw-cat */
    ATLAS_PROBE_UNSUPPORTED,     /* sink no ALSA (bluetooth, virtual...) */
    ATLAS_PROBE_IDLE,            /* sin reproducción y sonda activa desactivada */
    ATLAS_PROBE_NO_SINK
} AtlasProbeResult;

typedef struct {
    AtlasProbeResult result;
    int active;                 /* se usó flujo de silencio */
    int card;
    int device;
    int sub;
    unsigned long frames;       /* avance medido */
    int window_ms;
    int rate;                   /* rate del PCM (hw_params) */
    double ratio;               /* avance medido / esperado */
    AtlasPcmStatus before;
    AtlasPcmStatus after;
    char sink[ATLAS_NAME];
    char detail[512];
} AtlasProbe;

/* --- funciones puras (pruebas) --- */
int atlas_pcm_parse(const char *text, AtlasPcmStatus *st);
int atlas_pcm_parse_rate(const char *hw_params);
AtlasFlow atlas_pcm_classify(const AtlasPcmStatus *a, const AtlasPcmStatus *b,
                             int rate, int window_ms, double *ratio);
const char *atlas_pcm_state_name(AtlasPcmState s);
const char *atlas_probe_result_name(AtlasProbeResult r);
const char *atlas_flow_name(AtlasFlow f);

/* --- lectura de /proc (respeta ATLAS_PROC_ROOT) --- */
int atlas_pcm_read(int card, int device, int sub, AtlasPcmStatus *st);
/* Primer subdispositivo RUNNING de card/device (o el sub0). */
int atlas_pcm_read_any(int card, int device, int *sub_out, AtlasPcmStatus *st);
int atlas_pcm_rate(int card, int device, int sub);

/* Lista todos los PCM de reproducción RUNNING con su flujo. */
typedef struct {
    int card, device, sub;
    AtlasFlow flow;
    unsigned long frames;
} AtlasPcmFlowInfo;
int atlas_pcm_scan_running(AtlasPcmFlowInfo *out, int max, int window_ms);

/* --- sonda completa --- */
int atlas_probe_sink(const AtlasNode *sink, int allow_active, AtlasProbe *p);
void atlas_probe_json(JsonBuilder *b, const AtlasProbe *p);

/* Tono audible suave para confirmación humana. */
int atlas_test_tone(const char *sink, double seconds, double freq, double gain);

#endif
