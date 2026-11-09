# Desarrollo

Cómo compilar, probar y escribir código para Atlas. Para entender la estructura
del programa, lee antes [ARQUITECTURA.md](ARQUITECTURA.md).

## Compilar e instalar

Dependencias (Arch): `pipewire`, `wireplumber`, `json-glib`, `glib2`, `alsa-lib`,
`gcc`, `make`, `pkgconf`; para la GUI, `python-gobject`, `gtk4` y `libadwaita`.

```bash
make                 # compila ./audio-profile
make install         # CLI, GUI y servicio en ~/.local (sin root)
systemctl --user daemon-reload
systemctl --user restart audio-profile-watch.service
make run-gui         # la GUI contra el binario del repositorio
```

## Comprobar antes de un commit

```bash
make test        # 12 suites de pruebas en C, aisladas de tu configuración real
make test-asan   # las mismas, con AddressSanitizer/UBSan/LeakSanitizer
make lint        # gcc -fanalyzer + ruff (Python) + shellcheck (shell)
```

Las tres tienen que salir limpias. `make lint` usa `uvx` para ruff y shellcheck
si no están instalados.

`tests/run-tests.sh` ejecuta cada prueba con `ATLAS_CONFIG_DIR` y
`ATLAS_STATE_DIR` en un directorio temporal, así que nunca toca
`~/.config/audio-profile`.

Cuando un cambio toca el daemon (temporizadores, bucles, eventos), además se
**mide**: CPU con `systemctl --user show audio-profile-watch -p CPUUsageNSec`
antes y después durante un minuto, y memoria con `-p MemoryCurrent`.

## Reglas del código

Valen para todo lo nuevo y para lo que se toque.

**Para que se entienda**
- Nombres que digan qué es (`schedule_enforce`, no `se2`). Los módulos nuevos
  usan el prefijo `atlas_`.
- Funciones pequeñas con una sola tarea. La lógica que decide algo va en una
  función **pura** (sin E/S), declarada en el `.h` y con su prueba en `tests/`
  (ejemplos: `atlas_value_matches`, `atlas_auto_should_release`).
- Cada `.h` empieza con un comentario que explica el módulo. Los comentarios del
  `.c` cuentan el **porqué**, no el qué.
- Constantes con nombre (`#define BACKGROUND_RETRY_MS ...`) en vez de números
  sueltos.
- Código y mensajes en español, como el resto del proyecto.

**Seguridad**
- **Nunca** `system()` ni `popen()`: se ejecuta con `atlas_run()`,
  `atlas_runl()` o `ppm_exec()`, que pasan los argumentos sin shell. Nombres
  de nodos, títulos de ventanas y valores de perfiles vienen de fuera y pueden
  traer comillas o `;`.
- Nombres que acaban en rutas (perfiles, presets) se validan con
  `atlas_safe_name()`.
- Texto con `snprintf` y tamaño del búfer; nada de `strcpy`, `strcat` ni
  `sprintf`.
- JSON siempre con json-glib (`atlas_json_kv_str`...), nunca a mano con `printf`.

**Recursos**
- Todo lo que se abre se cierra en el mismo nivel: `fopen`/`fclose`,
  `json_parser_new`/`g_object_unref`, `malloc`/`free`. Una sola salida al final
  de la función si hay varios recursos.
- El daemon **no espera**: nada de red ni comandos lentos en su bucle (la
  consulta de Steam va en un proceso aparte). Si suspende mientras está
  bloqueado, no llega a proteger el audio (BUG-001).
- Nada de sondeos si hay un evento que avisa (ALSA, D-Bus). Si hace falta un
  temporizador de respaldo, que sea largo.
- Funciones con formato tipo `printf` llevan
  `__attribute__((format(printf, N, M)))` para que el compilador revise los
  argumentos.

**Python (GUI)**
- Nada bloqueante en el hilo de la interfaz: las llamadas a la CLI van por
  `backend.run_async()`.
- `subprocess.run(..., check=False)` explícito cuando el código revisa
  `returncode` él mismo.
- Si un valor se usa en una lambda diferida (`GLib.idle_add`), se copia antes:
  Python borra la variable de un `except` al salir del bloque.

## Avisos que se aceptan

- `-Wformat-truncation` en rutas: se arman en búferes de `PATH_MAX` (4096) que
  en la práctica no se llenan. Si una ruta nueva se usa para **escribir** un
  archivo, comprueba el valor que devuelve `snprintf` (o usa
  `atlas_path_join()`, que lo hace).
- Dos avisos de `-fanalyzer` en `atlas_util.c` (`devnull` y `buf`) son falsos
  positivos revisados; `scripts/lint.sh` los filtra con un comentario que
  explica por qué.
