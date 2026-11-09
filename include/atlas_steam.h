#ifndef ATLAS_STEAM_H
#define ATLAS_STEAM_H

/*
 * Género de juegos de Steam para los perfiles automáticos.
 *
 * Se consultan las etiquetas de la comunidad (SteamSpy, con votos) una sola
 * vez por juego y se guarda la clase en ~/.cache/audio-profile/steam/<id>.json.
 * Solo se envía el número del juego. Desactivable: auto.steam_lookup=no.
 *
 * Clases: racing · fps · adventure · gaming (resto: estrategia, simulación,
 * casual...). Se suma el peso (votos) de las etiquetas de cada clase y gana
 * la mayor.
 */

#include <stddef.h>

/* Clasifica a partir del JSON de SteamSpy (pura, para pruebas).
 * Devuelve 0 y rellena cls ("racing"...) y name; -1 si no hay etiquetas. */
int atlas_steam_classify_json(const char *json, char *cls, size_t n, char *name, size_t nn);

/* Igual, con las etiquetas de la página de la tienda (InitAppTagModal). */
int atlas_steam_classify_store_html(const char *html, char *cls, size_t n);

/* Clase del juego (caché -> red). "" si no se sabe. */
int atlas_steam_genre(const char *appid, char *cls, size_t n);
/* Daemon: no esperar a la red. Si falta la caché se lanza
 * `<cli_path> steam-genre <id>` aparte y se devuelve "" por ahora.
 * NULL o "" = consultar en el momento (CLI). */
void atlas_steam_set_background_lookups(const char *cli_path);

int atlas_cmd_steam_genre(int argc, char **argv);

#endif
