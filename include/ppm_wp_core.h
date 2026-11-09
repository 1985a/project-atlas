#ifndef PPM_WP_CORE_H
#define PPM_WP_CORE_H

typedef struct
{
    void *core;
} PpmWpCore;

int ppm_wp_core_open(PpmWpCore *ctx);
void ppm_wp_core_close(PpmWpCore *ctx);

int ppm_wp_core_available(void);
void ppm_wp_core_print_info(void);

#endif
