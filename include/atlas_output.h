#ifndef ATLAS_OUTPUT_H
#define ATLAS_OUTPUT_H

/*
 * Modo de salida: altavoces (5.1) o auriculares.
 *
 * Auriculares =
 *   - perfil de tarjeta estéreo en PipeWire (mezcla correcta del 5.1),
 *   - capa DSP "headphones" (Output Select=Headphone) tras la base de volumen,
 *   - variante "<perfil>.hp" de cada preset (surround virtual, menos X-Bass).
 * La base de volumen del usuario se mantiene en ambos modos.
 *
 * El jack de la Sound Blaster Z puede marcar "on" siempre, así que el modo
 * automático reacciona a CAMBIOS del jack, no a su valor.
 */

typedef enum { ATLAS_OUT_SPEAKERS = 0, ATLAS_OUT_HEADPHONES = 1 } AtlasOutputMode;

AtlasOutputMode atlas_output_mode(void);
const char *atlas_output_mode_name(AtlasOutputMode m);

/* Cambia el modo y reaplica el perfil actual. 0 si OK. */
int atlas_output_set(AtlasOutputMode m, const char *reason);

/* Estado del jack de auriculares: 1 on, 0 off, -1 desconocido. */
int atlas_output_jack(void);

/* Tick del daemon: sigue los cambios del jack si output.follow_jack=yes. */
void atlas_output_tick(void);

int atlas_cmd_output(int argc, char **argv);

#endif
