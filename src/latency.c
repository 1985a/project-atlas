#include "latency.h"
#include <stdio.h>
#include "ppm_pipewire.h"

void latency_show(void) {
    PpmAudioNode n;

    puts("PPM Latency Report");
    puts("==================");
    puts("");

    if (!ppm_get_main_audio_node(&n)) {
        puts("No pude detectar el nodo principal de audio.");
        return;
    }

    double latency = ppm_latency_ms(&n);
    double budget = ppm_budget_us(&n);
    double usage = ppm_dsp_usage_percent(&n);

    puts("Nodo principal");
    puts("--------------");
    printf("ID: %d\n", n.id);
    printf("Nombre: %s\n", n.name);
    printf("Formato: %s / %d canales / %d Hz\n", n.format, n.channels, n.format_rate);
    puts("");

    puts("Reloj");
    puts("-----");
    printf("Rate: %d Hz\n", n.rate);
    printf("Quantum: %d samples\n", n.quantum);
    printf("Latencia estimada: %.2f ms\n", latency);
    puts("");

    puts("DSP / Budget");
    puts("------------");
    printf("Busy: %.2f us\n", n.busy_us);
    printf("Budget: %.2f us\n", budget);
    printf("Uso DSP estimado: %.2f %%\n", usage);
    puts("");

    puts("Estado");
    puts("------");
    if (usage < 30.0)
        puts("✓ Excelente");
    else if (usage < 60.0)
        puts("✓ Bueno");
    else if (usage < 85.0)
        puts("⚠ Carga alta");
    else
        puts("⚠ Riesgo de cortes");
}
