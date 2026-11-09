# Changelog

## Sin publicar

### Revisión de buenas prácticas (2026-11-08)
- **Seguridad**: `ppm_exec` ya no usa `system()`. Volumen, mute, salida por defecto y
  `move` ejecutan sin shell; un valor de perfil o un nombre de nodo con `;`, comillas o
  `$(...)` ya no puede ejecutar comandos. El volumen se valida (0-150).
- **Daemon**: la consulta de géneros de Steam ya no bloquea el bucle (hasta ~20 s, con
  riesgo de no proteger el audio si se suspendía en ese momento): va en un proceso aparte.
- **Daemon**: `pactl subscribe` avisa de qué suena; la comprobación periódica pasa de 2 s
  a 6 s. CPU medida con 3 streams: 0,38 % → 0,19 %. (La cifra anterior de 0,07 % se midió
  con el automático en pausa y no era representativa.)
- `backup` guardaba `~/.config/audio-profiles` (carpeta de la v0.1): ahora guarda la
  configuración real, `asound.state` y el estado de las tarjetas, sin shell.
- Steam: nombre sin inicializar si solo respondía la tienda; JSON de `steam-genre` y de
  `--version` con json-glib (antes a mano, se rompía con comillas).
- GUI: `NameError` al fallar la recuperación (`e` usada tras el `except`).
- `atlas_run`: `/dev/null` comprobado, grupo de procesos fijado también en el padre,
  sin devolver salida cortada si falta memoria. `atlas_read_file` reserva solo lo que
  ocupa el archivo y avisa si lo corta.
- Funciones de registro con formato comprobado por el compilador; API de procesos
  `const` correcta; mensajes de la capa v0.1 en español.
- Limpieza: 100 archivos muertos u obsoletos fuera del repositorio (copias `.bak`,
  informes, código que no se compilaba, scripts viejos). Documentos de la v0.1 movidos a
  `docs/historico/`.
- Nuevos: `docs/ARQUITECTURA.md`, `docs/DESARROLLO.md`, `make test-asan`, `make lint`.


### Tu audio base manda siempre
- La base es tu `~/.config/asound.state` completo: altavoces, volúmenes **con Master**
  (`base.include_master=yes` por defecto) y tus efectos/EQ («Mi sonido»).
- `profiles.effects=personal` (por defecto): los perfiles solo cambian rate/quantum y
  conservan tus efectos. `profiles.effects=profile` vuelve a los efectos de cada perfil.
- El daemon comprueba la tarjeta cada `base.enforce_seconds` (10) y restaura lo que se
  desvíe de tu base (`base.enforce`). Un control que el driver no mantiene se deja tras
  3 intentos. Ya no hace falta `alsactl --file ~/.config/asound.state restore`.
- `dsp set` (y la GUI) con efectos personales guarda el cambio en `personal.conf`, para
  que el daemon no lo revierta. `dsp apply <preset>` aplica exactamente ese preset.
- El daemon relee `atlas.conf` cuando cambia: los ajustes de la GUI llegan sin reiniciarlo.

### Modo automático / manual
- Elegir un perfil a mano ya no pausa el automático para siempre: se recuerda qué habría
  elegido Atlas en ese momento (`lock_winner`) y el automático vuelve solo cuando eso
  cambia (otra aplicación o un juego). `auto off` = modo manual permanente.
- GUI: selector **Automático | Manual** en la cabecera y en Perfiles, con el estado
  «Automático · en pausa». La cabecera lee `auto.json` (sin lanzar la CLI).
- Detección: streams nativos de PipeWire (mpv…) sin `application.process.binary` usan
  `application.id`; antes la regla `binary=mpv` nunca se cumplía.

### Consumo
- El daemon escucha los eventos del mezclador ALSA (jack y base) en vez de sondear con
  amixer; el sondeo queda como red de seguridad (30 s / 5 min). CPU medida en reposo con
  audio sonando: 0,54 % → 0,07 % de un núcleo (medido con el automático en pausa; ver
  la revisión de buenas prácticas). Corrección de la base en ~2,5 s.
- Tarjeta destino de la vigilancia en caché (5 min): sin 6 llamadas a pactl por revisión.
- Perfiles automáticos: sin streams no se consulta a sway ni a MPRIS.
- Servicio con `Nice=10`, E/S idle y límite de memoria (48/96 MB).
- GUI: no consulta el estado mientras la ventana no tiene el foco.

## 0.2.0 — 2026-11-03

### BUG-001 (audio perdido tras suspender)
- Sonda funcional `hw_ptr`, pasiva o con flujo de silencio. Detecta el DMA atascado
  que antes daba falso positivo en status y doctor.
- Salud por niveles: servicios, conexión, hardware y acústica (esta última se declara
  "no medible").
- `recover` reescrito como escalera verificada: reabrir tarjeta → pipewire-pulse →
  PipeWire completo + reparación del portal. Captura evidencia antes de actuar y
  restaura defaults, perfiles, volumen por canal y rate/quantum.
- Daemon `watch`: inhibidor delay de logind, cierra el PCM antes de S3, restaura y
  verifica al reanudar, y deja un historial por ciclo.
- `history`, `report`, `test-tone`, `check`.

### Detección y salida
- MPRIS: el título y la URL de lo que suena (pestañas en segundo plano) se asocian a su
  stream por PID o por programa. Condiciones `url~` y `title~` sobre MPRIS.
- Género automático de juegos de Steam (`genre=`): etiquetas votadas de SteamSpy, con la
  página de la tienda como segunda fuente y caché local. Comando `steam-genre`. Nueva suite
  `test_steam`.
- Modo auriculares (`output`): perfil de tarjeta estéreo, capa `headphones`, variantes `.hp`
  de los 13 presets y cambio automático por transición del jack. Tecla `h` en sway.
- `base hw-lock`: `api.alsa.soft-mixer` por tarjeta, para que PipeWire no reescriba el
  mezclador de hardware. Lo vigila doctor.
- Master excluido de la base por defecto (`base.include_master`): es el mando de volumen
  del usuario.

### Base de volumen
- `base import|save|show|apply`: el `asound.state` del usuario se convierte en capas
  `base` (altavoces), `volumes` (canales) y `personal` (perfil «Mi sonido»). Todos los
  perfiles se aplican sobre esa base. El daemon la aplica al iniciar sesión.
- El `base.conf` de fábrica queda mínimo. La gestión de graves a 120 Hz pasa a ser el
  preset opcional `x530-graves-al-sub`.
- `dsp save` ya no guarda controles de volumen ni de altavoces.
- Nueva suite `test_base` (parser de alsactl con el formato real).

### Perfiles y DSP
- 13 perfiles de fábrica (nuevos: `browsing`, `video`, `retro`, `fps`, `racing`, `adventure`) con
  presets DSP por hardware para Sound Blaster Z + Logitech X-530.
- `dsp get|set|apply|save|presets`. Preset opcional de graves a 120 Hz para X-530.
- Perfiles automáticos por ejecutable, juego de Steam o título de ventana (`auto ...`).
- Perfiles del usuario en `~/.config/audio-profile/profiles` (tienen prioridad).
- `alsa-save` guarda en la carpeta del usuario y nunca sobrescribe presets versionados.
  `apply` hace copia ALSA previa.

### Hardware DB (EPIC-04)
- Descubrimiento estable por sysfs, emparejamiento único/ninguno/ambiguo/parcial,
  registro sin duplicados, `--link` sin tocar archivos versionados, `hardware-validate`
  e integración en doctor.

### Seguridad y calidad
- Los comandos nuevos y los reescritos usan `fork/execvp` sin shell. Se eliminó la
  inyección en `hardware-info`, `save` y `load`.
- `status` interpreta bien `clock.force-*` (la "anomalía" de v0.1 era de formato).
- Orden de canales del volumen preservado (regresión cubierta en `test_auto`).
- 3 suites de pruebas nuevas: `test_probe`, `test_hwdb`, `test_auto`.

### GUI
- Atlas Audio (GTK4/libadwaita) con páginas Inicio, Perfiles, Efectos, Ecualizador,
  Altavoces (prueba por canal), Mezclador, Recuperación, Hardware y Diagnóstico.
