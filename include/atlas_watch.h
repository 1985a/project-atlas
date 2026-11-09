#ifndef ATLAS_WATCH_H
#define ATLAS_WATCH_H

/*
 * Daemon de usuario (audio-profile-watch.service):
 *  - Toma un inhibidor "delay" de logind. En PrepareForSleep(true) guarda
 *    una instantánea, apaga el perfil de las tarjetas (cierra el PCM antes
 *    de S3) y suelta el inhibidor.
 *  - En PrepareForSleep(false) restaura perfiles, defaults y volúmenes,
 *    ejecuta la verificación/recuperación escalonada y lo registra en el
 *    historial (evidencia por ciclo).
 *  - Cada 2 s evalúa los perfiles automáticos.
 */

void atlas_watch_before_sleep(void);
void atlas_watch_after_resume(void);
int atlas_cmd_watch(int argc, char **argv);

#endif
