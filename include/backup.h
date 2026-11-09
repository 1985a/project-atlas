#ifndef BACKUP_H
#define BACKUP_H

/*
 * `audio-profile backup`: empaqueta en un .tar.gz la configuración de audio
 * del usuario (Atlas, PipeWire, WirePlumber, asound.state...) y el estado
 * actual de las tarjetas. Devuelve 0 si se creó el archivo.
 */
int backup_create(void);

#endif
