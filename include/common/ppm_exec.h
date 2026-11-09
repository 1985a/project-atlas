#ifndef PPM_EXEC_H
#define PPM_EXEC_H

/*
 * Ejecuta un programa con sus argumentos, SIN pasar por la shell:
 *
 *     ppm_exec("wpctl", "set-volume", id, "0.5", NULL);
 *
 * Cada argumento llega tal cual al programa, así que un nombre de nodo o un
 * valor de un perfil con comillas, ';' o '$(...)' no puede ejecutar nada.
 * La lista termina en NULL. Muestra la orden como "[EXEC] ..." y devuelve
 * el código de salida del programa (o -1 si no se pudo lanzar).
 */
int ppm_exec(const char *program, ...);

#endif
