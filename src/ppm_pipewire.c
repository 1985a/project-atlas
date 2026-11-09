#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ppm_pipewire.h"

static int parse_pwtop_line(const char *line, PpmAudioNode *n) {
    char state;

    memset(n, 0, sizeof(*n));

    int matched = sscanf(
        line,
        " %c %d %d %d %lfus %lfus %lf %lf %d %63s %d %d %255[^\n]",
        &state,
        &n->id,
        &n->quantum,
        &n->rate,
        &n->wait_us,
        &n->busy_us,
        &n->wq,
        &n->bq,
        &n->err,
        n->format,
        &n->channels,
        &n->format_rate,
        n->name
    );

    //return matched >= 13 && state == 'R';
    return matched >= 10 && state == 'R' && n->quantum > 0 && n->rate > 0;
}

int ppm_get_main_audio_node(PpmAudioNode *node) {
    FILE *fp;
    char line[1024];
    PpmAudioNode n;
    int found = 0;

    fp = popen("timeout 1s pw-top -b 2>/dev/null", "r");
    if (!fp)
        return 0;

    while (fgets(line, sizeof(line), fp)) {
        if (!parse_pwtop_line(line, &n))
            continue;

     //   if (strstr(n.name, "alsa_output") || strstr(n.name, "CA0132")) {
        if (strstr(line, "alsa_output") || strstr(line, "CA0132") || strstr(line, "analog-surround")) {
            *node = n;
            found = 1;
        }
    }

    pclose(fp);
    return found;
}

double ppm_latency_ms(const PpmAudioNode *node) {
    if (!node || node->rate <= 0)
        return 0.0;

    return ((double)node->quantum / (double)node->rate) * 1000.0;
}

double ppm_budget_us(const PpmAudioNode *node) {
    return ppm_latency_ms(node) * 1000.0;
}

double ppm_dsp_usage_percent(const PpmAudioNode *node) {
    double budget = ppm_budget_us(node);

    if (!node || budget <= 0.0)
        return 0.0;

    return (node->busy_us / budget) * 100.0;
}
