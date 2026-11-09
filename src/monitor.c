#include "monitor.h"
#include <stdio.h>
#include <unistd.h>
#include <signal.h>

#include "ppm_pipewire.h"

static volatile sig_atomic_t running = 1;

static void on_sigint(int sig) {
    (void)sig;
    running = 0;
}

static void bar(double pct) {
    int width = 32;
    int filled = (int)((pct / 100.0) * width);

    if (filled < 0) filled = 0;
    if (filled > width) filled = width;

    putchar('[');
    for (int i = 0; i < width; i++)
        putchar(i < filled ? '#' : '.');
    putchar(']');
}

void monitor_run(void) {
    PpmAudioNode n;

    signal(SIGINT, on_sigint);
    signal(SIGTERM, on_sigint);

    setvbuf(stdout, NULL, _IONBF, 0);

    printf("\033[?25l");
    printf("\033[2J");

    while (running) {
        printf("\033[H");

        puts("PPM Monitor - PipeWire Profile Manager");
        puts("======================================");
        puts("");

        if (!ppm_get_main_audio_node(&n)) {
            puts("No pude detectar el nodo principal.");
            printf("\033[J");
            sleep(1);
            continue;
        }

        double latency = ppm_latency_ms(&n);
        double budget = ppm_budget_us(&n);
        double usage = ppm_dsp_usage_percent(&n);

        printf("Nodo:      %s\n", n.name);
        printf("ID:        %d\n", n.id);
        printf("Formato:   %s / %d canales / %d Hz\n", n.format, n.channels, n.format_rate);
        puts("");

        printf("Rate:      %d Hz\n", n.rate);
        printf("Quantum:   %d samples\n", n.quantum);
        printf("Latencia:  %.2f ms\n", latency);
        puts("");

        printf("Busy:      %.2f us\n", n.busy_us);
        printf("Budget:    %.2f us\n", budget);
        printf("DSP Load:  %.2f %% ", usage);
        bar(usage);
        puts("");

        printf("W/Q:       %.3f\n", n.wq);
        printf("B/Q:       %.3f\n", n.bq);
        printf("ERR count: %d\n", n.err);
        puts("");

        if (usage < 30.0)
            puts("Estado:    Excelente");
        else if (usage < 60.0)
            puts("Estado:    Bueno");
        else if (usage < 85.0)
            puts("Estado:    Carga alta");
        else
            puts("Estado:    Riesgo de cortes");

        puts("");
        puts("Ctrl+C para salir.");

        printf("\033[J");
        sleep(1);
    }

    printf("\033[?25h");
    printf("\n");
}
