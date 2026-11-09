# Continuidad · Atlas 0.2 (2026-11-04)

Estado al pausar el trabajo. Rama `feature/atlas-0.2`, sin push ni merge.

## Hecho e instalado
- CLI `audio-profile` 0.2.0 y GUI `atlas-audio` en `~/.local` (`make install`, `make install-gui`).
  La v0.1 de `/usr/local` está borrada.
- `audio-profile-watch.service` (usuario) habilitado y activo: quiesce antes de S3,
  recover verificado al reanudar, perfiles automáticos, seguimiento del jack.
- Base de volumen importada de `~/.config/asound.state` → `~/.config/audio-profile/hardware/ca0132/dsp/{base,volumes,personal}.conf`.
- `base hw-lock on` activo (`~/.config/wireplumber/wireplumber.conf.d/60-atlas-hw-mixer-lock.conf`).
- Sway: `include ~/.local/share/audio-profile/sway/atlas-audio.conf` en `~/.config/i3/config`
  (`$mod+a` abre la app, `$mod+Shift+a` modo Audio con iconos).
- 11/11 suites de pruebas pasan (`make test`).

## 2026-11-04 (tarde)
- BUG-001 cerrado. El daemon relee `atlas.conf` en caliente.
- Base = asound.state completo (incl. Master y efectos), vigilada cada 10 s por el daemon.
  Los perfiles ya no pisan tus efectos (`profiles.effects=personal`).
- Copia de las capas anteriores: `~/.config/audio-profile/hardware/ca0132/dsp/backup-20261104-183952/`.
- Consumo: el daemon escucha eventos ALSA (sin sondeo con amixer) y `pactl subscribe`
  (qué suena). CPU real con streams presentes: ~0,19 % (el 0,07 % anterior se midió en pausa).
  Servicio con Nice=10 y E/S idle. La GUI no consulta nada sin foco.
- Modo Automático | Manual en la GUI (cabecera y Perfiles). Elegir un perfil a mano pausa el
  automático solo hasta que cambia lo que suena (`lock_winner` en auto.json).
- Detección de mpv (streams nativos de PipeWire) corregida vía `application.id`.
- Ojo al probar: `audio-profile apply X` pausa el automático; terminar con `audio-profile auto on`.

## 2026-11-08: revisión de buenas prácticas
- Inyección de comandos corregida (capa ppm_*, sin shell), Steam fuera del bucle del
  daemon, `backup` arreglado, 100 archivos muertos fuera, docs v0.1 a `docs/historico/`.
- Antes de cada commit: `make test && make test-asan && make lint` (ver docs/DESARROLLO.md).

## Pendiente
1. ~~Cerrar BUG-001~~ — cerrado el 2026-11-04 (ver `docs/BUG-001.md`).
2. Probar auriculares enchufando de verdad (el jack de la SBZ siempre marca "on";
   el cambio automático solo reacciona a transiciones). Ahora va por eventos ALSA.
5. Reabrir la GUI para ver el selector Automático | Manual (la abierta era la versión vieja).
3. Opcional: `sudo pacman -S --asdeps libltc` para los plugins x42 en `/mnt/disco16t/plugins_audio`.
4. Decidir push/merge de `feature/atlas-0.2`.

## Para retomar
```bash
cd ~/GIT/audio-profile && git log --oneline -3
audio-profile history
audio-profile doctor
systemctl --user status audio-profile-watch.service
audio-profile auto status        # modo y perfil actual
```
