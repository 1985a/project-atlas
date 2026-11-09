#ifndef PPM_PIPEWIRE_H
#define PPM_PIPEWIRE_H

typedef struct {
    int id;
    int quantum;
    int rate;
    int err;
    int channels;
    int format_rate;

    double wait_us;
    double busy_us;
    double wq;
    double bq;

    char format[64];
    char name[256];
} PpmAudioNode;

int ppm_get_main_audio_node(PpmAudioNode *node);
double ppm_latency_ms(const PpmAudioNode *node);
double ppm_budget_us(const PpmAudioNode *node);
double ppm_dsp_usage_percent(const PpmAudioNode *node);

#endif
