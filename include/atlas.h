#ifndef ATLAS_H
#define ATLAS_H

/*
 * Project Atlas - núcleo v0.2
 *
 * Utilidades comunes: ejecución segura de procesos (sin shell), rutas de
 * datos/estado, configuración del usuario y salida JSON.
 */

#include <stddef.h>
#include <json-glib/json-glib.h>

#define ATLAS_VERSION "0.2.0"

/* ---------------------------------------------------------------- salida */

extern int atlas_json_mode;          /* --json */
extern int atlas_quiet;              /* --quiet */

void atlas_json_print(JsonBuilder *b);       /* imprime y libera */
void atlas_json_kv_str(JsonBuilder *b, const char *k, const char *v);
void atlas_json_kv_int(JsonBuilder *b, const char *k, long long v);
void atlas_json_kv_bool(JsonBuilder *b, const char *k, int v);
void atlas_json_kv_double(JsonBuilder *b, const char *k, double v);

/* ------------------------------------------------------------- procesos */

#define ATLAS_RUN_MERGE_STDERR 0x1   /* stderr -> salida capturada */
#define ATLAS_RUN_SHOW_STDERR  0x2   /* stderr se hereda (por defecto se descarta) */

/*
 * Ejecuta argv (argv[0] se busca en PATH) sin shell.
 * Si out != NULL devuelve la salida estándar en memoria (liberar con free).
 * Devuelve el código de salida, 124 si se agotó el tiempo, -1 si no se
 * pudo ejecutar.
 */
int atlas_run(const char *const argv[], char **out, int timeout_ms, int flags);

/* Variante con lista terminada en NULL. */
int atlas_runl(char **out, int timeout_ms, const char *arg0, ...);

/* Lanza un proceso en segundo plano con stdin desde fd_in (-1 = /dev/null). */
int atlas_spawn(const char *const argv[], int fd_in, int *pid_out);
int atlas_spawn_wait(int pid, int timeout_ms);
void atlas_spawn_kill(int pid);

int atlas_have_cmd(const char *cmd);
void atlas_msleep(int ms);
long long atlas_now_ms(void);
void atlas_timestamp(char *buf, size_t n, int compact);

/* --------------------------------------------------------------- rutas */

const char *atlas_data_dir(void);     /* profiles/, hardware/, presets/ */
const char *atlas_config_dir(void);   /* ~/.config/audio-profile */
const char *atlas_state_dir(void);    /* ~/.local/state/audio-profile */
int atlas_mkdirs(const char *path);
int atlas_path_join(char *dst, size_t n, const char *a, const char *b);
int atlas_file_exists(const char *path);
int atlas_dir_exists(const char *path);
char *atlas_read_file(const char *path, size_t max);   /* free() */
int atlas_write_file_atomic(const char *path, const char *data);
int atlas_append_line(const char *path, const char *line);

/* Nombres seguros: [A-Za-z0-9._-], sin '.' ni '..', longitud 1..63 */
int atlas_safe_name(const char *s);

/* ------------------------------------------------------------ config */

/* ~/.config/audio-profile/atlas.conf (clave=valor). Valores por defecto
 * integrados si no existe. */
const char *atlas_conf_get(const char *key);
int atlas_conf_int(const char *key);
int atlas_conf_bool(const char *key);

#endif
