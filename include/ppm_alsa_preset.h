#ifndef PPM_ALSA_PRESET_H
#define PPM_ALSA_PRESET_H

int ppm_alsa_apply_ca0132_optimal(void);
int ppm_alsa_apply_preset(const char *preset);

/* Guarda el estado de 'card' como preset del usuario (~/.config/...). */
int ppm_alsa_save_preset(const char *name, const char *card, int force);

#endif
