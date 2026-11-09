# Cómo está hecho Atlas

Guía para quien abre el código por primera vez. Para usar el programa, mira el
[README](../README.md); para compilar y contribuir, [DESARROLLO.md](DESARROLLO.md).

## Las tres piezas

```
                 ┌──────────────────────────────┐
  tú ──────────► │  atlas-audio  (GUI, Python)  │  solo muestra y guarda ajustes
                 └──────────────┬───────────────┘
                                │ ejecuta la CLI con --json
                 ┌──────────────▼───────────────┐
  tú ──────────► │  audio-profile  (CLI, C)     │  hace el trabajo
                 └──────────────┬───────────────┘
                                │ el mismo binario, en modo daemon
                 ┌──────────────▼───────────────┐
  systemd ─────► │  audio-profile watch         │  suspensión, perfiles
                 │  (audio-profile-watch.service)│  automáticos, base de volumen
                 └──────────────────────────────┘
```

- **La CLI y el daemon son el mismo programa en C.** `audio-profile watch` es el
  modo que se queda corriendo (lo arranca systemd al iniciar sesión).
- **La GUI no toca el audio**: llama a la CLI y lee sus archivos de estado. Por
  eso se puede cerrar y todo sigue funcionando.
- **Nunca se usa la shell** para ejecutar programas: todo pasa por `atlas_run()`
  con una lista de argumentos (ver `include/atlas.h`).

## Dónde vive cada cosa

| Carpeta | Contenido |
|---|---|
| `src/`, `include/` | Código C. Cada `atlas_X.c` tiene su `include/atlas_X.h`, que explica el módulo en su primer comentario. |
| `gui/atlasaudio/` | La GUI: `app.py` (ventanas), `backend.py` (llamadas a la CLI), `widgets.py` (dibujos: ecualizador, altavoces). |
| `tests/` | Pruebas en C. `run-tests.sh` las ejecuta aisladas del usuario real. |
| `profiles/` | Perfiles de fábrica (`profiles/<id>/profile.conf`). |
| `hardware/<id>/` | Hardware DB: identidad de cada tarjeta, avisos conocidos y presets DSP. |
| `rules/auto.rules` | Reglas de fábrica de los perfiles automáticos. |
| `data/` | Servicio systemd y modo Audio de sway. |
| `docs/` | Esta guía, BUG-001, la nota de continuidad y las RFC. `docs/historico/` es el diseño v0.1. |

## Módulos en C

**Núcleo**

| Módulo | Qué hace |
|---|---|
| `main.c` | Lee el comando y llama al módulo que corresponde. |
| `atlas_util.c` | Utilidades comunes: lanzar procesos sin shell, leer/escribir archivos, rutas, `atlas.conf`, salida JSON. |
| `atlas_watch.c` | El daemon: suspensión (logind por D-Bus), eventos de la tarjeta (ALSA), perfiles automáticos y vigilancia de la base. |

**Audio**

| Módulo | Qué hace |
|---|---|
| `atlas_audio.c` | Foto del estado de audio (tarjetas, sinks, streams) vía `pactl -f json`. |
| `atlas_health.c`, `atlas_probe.c` | ¿Funciona el audio? Por niveles, hasta mirar si la tarjeta consume muestras de verdad. |
| `atlas_recover.c` | Recuperación escalonada: reabrir la tarjeta, reiniciar pipewire-pulse, reiniciar PipeWire. |
| `atlas_doctor.c` | Diagnóstico completo (`audio-profile doctor`). |
| `atlas_output.c` | Altavoces o auriculares (sigue el jack). |

**Perfiles y sonido**

| Módulo | Qué hace |
|---|---|
| `profile.c` | Lee los `profile.conf`. |
| `profile_apply.c` | Aplica un perfil: latencia (`pipewire_cmd.c`), DSP, sink/source por defecto, volumen. |
| `atlas_dsp.c` | Controles de la tarjeta por capas (ver abajo). |
| `atlas_base.c` | Tu audio base (importado de `asound.state`) y su vigilancia. |
| `atlas_auto.c` | Perfiles automáticos: qué suena, qué regla gana, pausa manual. |
| `atlas_steam.c` | Género de juegos de Steam (con caché; en el daemon, en segundo plano). |

**Hardware**

| Módulo | Qué hace |
|---|---|
| `atlas_hwdb.c` | Hardware DB: reconocer tarjetas por IDs estables, registrar nuevas, validar. |

**Capa heredada `ppm_*` (v0.1)**: los comandos de bajo nivel (`sinks`, `streams`,
`graph`, `move`, `volume`...) usan un registro propio de PipeWire
(`ppm_context.c`, `ppm_registry.c`...). Funciona, pero tiene otro estilo de
código; los módulos nuevos no deberían depender de ella.

## Flujos importantes

### Aplicar un perfil (`audio-profile apply video`)

1. `profile.c` lee `profiles/video/profile.conf`.
2. `profile_apply.c` fija rate/quantum en PipeWire.
3. `atlas_dsp.c` aplica las capas de la tarjeta, en este orden:
   `base` (altavoces) → `volumes` (tu volumen) → `headphones` (si hay
   auriculares) → efectos.
4. Los efectos son **los tuyos** (`personal.conf`), salvo que en `atlas.conf`
   pongas `profiles.effects=profile`.
5. Si lo pediste tú (no el automático), `atlas_auto.c` pausa el modo automático
   hasta que cambie lo que suena.

### Suspender y reanudar (BUG-001)

1. logind avisa con `PrepareForSleep(true)`; el daemon tiene un inhibidor que le
   da tiempo.
2. Guarda una foto, apaga el perfil de las tarjetas (cierra el PCM) y suelta el
   inhibidor.
3. Al reanudar: restaura perfiles y volúmenes, comprueba con la sonda y, si hace
   falta, recupera por niveles. Todo queda en `audio-profile history`.

### Tu audio base se mantiene sola

El daemon escucha los **eventos ALSA** de la tarjeta: no consulta nada mientras
no cambie un control. Cuando algo cambia, espera 2 s (para agrupar ráfagas) y
compara la tarjeta con tus capas; lo que no coincida lo restaura y lo anota en
el registro (`journalctl --user -u audio-profile-watch`). Si lo cambias tú desde
la GUI, `dsp set` lo guarda antes en `personal.conf`, así que no se revierte.

### Qué suena (perfiles automáticos)

El daemon mantiene abierto `pactl subscribe`, que avisa al instante cuando una
aplicación empieza o deja de sonar. Con cada aviso, `atlas_auto.c` mira los
streams, aplica las reglas y elige el de **mayor prioridad** (`priority=` en el
perfil; p. ej. `music` 80 gana a `cinema` 70). El ganador tiene que mantenerse
3 s antes de aplicarse. Como un cambio de pestaña en el navegador no genera
evento, hay además una comprobación cada 6 s. Si nada suena, el perfil no cambia.

## Archivos del usuario

| Ruta | Qué guarda |
|---|---|
| `~/.config/audio-profile/atlas.conf` | Ajustes (`clave=valor`). Los valores por defecto están en `atlas_util.c` (`conf_defaults`). El daemon lo relee si cambia. |
| `~/.config/audio-profile/hardware/<id>/dsp/` | Tus capas: `base.conf`, `volumes.conf`, `personal.conf`. |
| `~/.config/audio-profile/auto.rules` | Tus reglas de perfiles automáticos (mandan sobre las de fábrica). |
| `~/.local/state/audio-profile/` | Estado: `auto.json` (modo y perfil actual), `history.jsonl`, informes y copias. |
| `~/.cache/audio-profile/steam/` | Géneros de juegos ya consultados. |
