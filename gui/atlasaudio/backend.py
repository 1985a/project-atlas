"""Acceso a la CLI `audio-profile --json` y a pactl/amixer.

Toda la lógica de diagnóstico, recuperación, perfiles y Hardware DB vive en
la CLI (una sola fuente de verdad). La GUI solo la invoca y presenta los
resultados; los cambios rápidos (volumen, controles DSP) usan pactl/amixer
directamente para que los deslizadores respondan al instante.
"""
from __future__ import annotations

import contextlib
import json
import math
import os
import shutil
import struct
import subprocess
import threading
from pathlib import Path

HERE = Path(__file__).resolve().parent


def _find_cli() -> str:
    env = os.environ.get("ATLAS_CLI")
    if env:
        return env
    # árbol de desarrollo: gui/atlasaudio -> raíz del repo
    dev = HERE.parent.parent / "audio-profile"
    if dev.is_file() and os.access(dev, os.X_OK):
        return str(dev)
    # instalado junto a atlas-audio
    for cand in (Path.home() / ".local/bin/audio-profile", Path("/usr/local/bin/audio-profile"),
                 Path("/usr/bin/audio-profile")):
        if cand.is_file():
            return str(cand)
    return shutil.which("audio-profile") or "audio-profile"


CLI = _find_cli()
CONFIG_DIR = Path(os.environ.get("ATLAS_CONFIG_DIR") or
                  Path(os.environ.get("XDG_CONFIG_HOME", Path.home() / ".config")) / "audio-profile")
STATE_DIR = Path(os.environ.get("ATLAS_STATE_DIR") or
                 Path(os.environ.get("XDG_STATE_HOME", Path.home() / ".local/state")) / "audio-profile")


def display_path(path) -> str:
    """Ruta para mostrar en pantalla: la carpeta personal como "~" (sin el nombre de usuario)."""
    text = str(path)
    home = str(Path.home())
    return "~" + text[len(home):] if text == home or text.startswith(home + "/") else text


class CliError(RuntimeError):
    pass


def run(*args: str, timeout: float = 30, json_out: bool = True):
    """Ejecuta la CLI. Con json_out devuelve el objeto JSON; si no, el texto."""
    cmd = [CLI, *args] + (["--json"] if json_out else [])
    try:
        p = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout, check=False)
    except FileNotFoundError as e:
        raise CliError(f"No encuentro la CLI: {CLI}") from e
    except subprocess.TimeoutExpired as e:
        raise CliError(f"Tiempo agotado: {' '.join(args)}") from e
    if not json_out:
        return p.returncode, (p.stdout + p.stderr).strip()
    out = p.stdout.strip()
    if not out:
        raise CliError(p.stderr.strip() or f"{' '.join(args)} no devolvió datos")
    try:
        return json.loads(out)
    except json.JSONDecodeError:
        # salida con texto previo (p. ej. avisos): tomar el último bloque JSON
        start = min([i for i in (out.find("{"), out.find("[")) if i >= 0] or [0])
        return json.loads(out[start:])


def run_async(fn, done, *args):
    """Ejecuta fn(*args) en un hilo y llama done(resultado, error) en el hilo de GTK."""
    from gi.repository import GLib

    def work():
        try:
            r, err = fn(*args), None
        except Exception as e:
            r, err = None, e
        GLib.idle_add(lambda: (done(r, err), False)[1])

    threading.Thread(target=work, daemon=True).start()


def stream_lines(args: list[str], on_line, on_done):
    """Ejecuta la CLI con --json y entrega cada línea JSON (eventos de recover)."""
    from gi.repository import GLib

    def work():
        rc = -1
        try:
            p = subprocess.Popen([CLI, *args, "--json"], stdout=subprocess.PIPE,
                                 stderr=subprocess.STDOUT, text=True)
            for raw in p.stdout:
                line = raw.strip()
                if not line:
                    continue
                try:
                    ev = json.loads(line)
                except json.JSONDecodeError:
                    ev = {"event": "info", "level": 0, "message": line}
                GLib.idle_add(lambda e=ev: (on_line(e), False)[1])
            rc = p.wait()
        except Exception as e:
            # Python borra `e` al salir del except: guardar el texto antes de diferir
            message = str(e)
            GLib.idle_add(lambda: (on_line({"event": "fail", "level": 0, "message": message}), False)[1])
        GLib.idle_add(lambda: (on_done(rc), False)[1])

    threading.Thread(target=work, daemon=True).start()


# --- consultas -----------------------------------------------------------

def status(probe=False):
    return run("status", *(["--probe"] if probe else []), timeout=20)


def doctor():
    return run("doctor", timeout=40)


def profiles():
    return run("list")


def auto_status():
    return run("auto", "status")


def devices():
    return run("devices")


def hardware_info(hw_id):
    return run("hardware-info", hw_id)


def hardware_validate():
    return run("hardware-validate")


def dsp_state():
    return run("dsp", "get")


def dsp_presets():
    """Lista de presets; vacía si la tarjeta no está en la Hardware DB."""
    try:
        return run("dsp", "presets")
    except (CliError, ValueError):
        return []


def history(n=50):
    return run("history", "-n", str(n))


# --- acciones (texto) ----------------------------------------------------

def apply_profile(pid):
    return run("apply", pid, json_out=False, timeout=30)


def auto_mode() -> dict:
    """Modo actual leído del estado (sin lanzar la CLI): auto, auto-paused o manual."""
    import json
    try:
        d = json.loads((STATE_DIR / "auto.json").read_text())
    except (OSError, ValueError):
        d = {}
    enabled = d.get("enabled", True)
    mode = "manual" if not enabled else ("auto-paused" if d.get("manual_lock") else "auto")
    return {"mode": mode, "current": d.get("current", ""), "lock_winner": d.get("lock_winner", "")}


def auto_set(on: bool):
    return run("auto", "on" if on else "off", json_out=False)


def auto_assign(pid, cond):
    return run("auto", "assign", pid, *cond.split(), json_out=False)


def dsp_set(name, value):
    return run("dsp", "set", name, str(value), json_out=False, timeout=5)


def dsp_apply(preset):
    return run("dsp", "apply", preset, json_out=False)


def dsp_save(name):
    return run("dsp", "save", name, json_out=False)


def output_status():
    return run("output", "status")


def output_set(mode):
    return run("output", mode, json_out=False, timeout=30)


def base_import(path=None):
    return run("base", "import", *([path] if path else []), json_out=False)


def base_save():
    return run("base", "save", json_out=False)


def base_apply():
    return run("base", "apply", json_out=False)


HWLOCK = Path(os.environ.get("XDG_CONFIG_HOME", Path.home() / ".config")) / \
    "wireplumber/wireplumber.conf.d/60-atlas-hw-mixer-lock.conf"


def hwlock_on() -> bool:
    return HWLOCK.exists()


def hwlock_set(on: bool):
    return run("base", "hw-lock", "on" if on else "off", json_out=False, timeout=40)


def hardware_register(card, extra=()):
    return run("hardware-register", str(card), *extra, json_out=False)


def report():
    return run("report")


def test_tone(sink=None):
    return run("test-tone", *(["--sink", sink] if sink else []), timeout=15)


def reset_clock():
    return run("reset", json_out=False)


# --- pactl directo (respuesta inmediata) --------------------------------

def _pactl(*a):
    return subprocess.run(["pactl", *a], capture_output=True, text=True, timeout=5, check=False)


def set_volume(kind: str, name: str, percent: int):
    _pactl(f"set-{kind}-volume", name, f"{int(percent)}%")


def set_mute(kind: str, name: str, mute: bool):
    _pactl(f"set-{kind}-mute", name, "1" if mute else "0")


def set_default(kind: str, name: str):
    _pactl(f"set-default-{kind}", name)


def sink_inputs():
    p = _pactl("-f", "json", "list", "sink-inputs")
    try:
        return json.loads(p.stdout or "[]")
    except json.JSONDecodeError:
        return []


def set_input_volume(index: int, percent: int):
    _pactl("set-sink-input-volume", str(index), f"{int(percent)}%")


def set_input_mute(index: int, mute: bool):
    _pactl("set-sink-input-mute", str(index), "1" if mute else "0")


# --- tono por canal -------------------------------------------------------

CHANNELS_51 = ["FL", "FR", "RL", "RR", "FC", "LFE"]


def channel_tone(sink: str | None, channel: int, seconds=1.0, rate=48000):
    """Tono suave solo en un canal del 5.1 (440 Hz en los satélites, 60 Hz en
    el LFE). Se reproduce en un hilo."""
    def work():
        n = int(seconds * rate)
        freq = 60.0 if CHANNELS_51[channel] == "LFE" else 440.0
        fade = rate // 20
        buf = bytearray()
        for i in range(n):
            env = min(1.0, i / fade, (n - i) / fade)
            s = int(math.sin(2 * math.pi * freq * i / rate) * 0.18 * env * 32767)
            frame = [0] * 6
            frame[channel] = s
            buf += struct.pack("<6h", *frame)
        buf += bytes(6 * 2 * rate // 3)
        cmd = ["pw-cat", "--playback", "--raw", "--rate", str(rate), "--channels", "6",
               "--channel-map", "FL,FR,RL,RR,FC,LFE", "--format", "s16",
               "-P", '{ media.name="Atlas test tone" node.description="Atlas channel test" }']
        if sink:
            cmd += ["--target", sink]
        cmd.append("-")
        # el tono de prueba es opcional: si pw-cat falla, no hay nada que avisar
        with contextlib.suppress(OSError, subprocess.SubprocessError):
            subprocess.run(cmd, input=bytes(buf), timeout=seconds + 6, check=False)

    threading.Thread(target=work, daemon=True).start()


# --- servicio y configuración --------------------------------------------

WATCH_UNIT = "audio-profile-watch.service"


def watch_state() -> dict:
    p = subprocess.run(["systemctl", "--user", "show", WATCH_UNIT, "-p",
                        "LoadState,ActiveState,UnitFileState"], capture_output=True, text=True, check=False)
    d = dict(line.split("=", 1) for line in p.stdout.splitlines() if "=" in line)
    return {"loaded": d.get("LoadState") == "loaded", "active": d.get("ActiveState") == "active",
            "enabled": d.get("UnitFileState") == "enabled"}


def watch_enable(on: bool):
    args = ["enable", "--now"] if on else ["disable", "--now"]
    return subprocess.run(["systemctl", "--user", *args, WATCH_UNIT], capture_output=True, text=True, check=False)


def conf_get(key: str, default: str = "") -> str:
    path = CONFIG_DIR / "atlas.conf"
    try:
        for line in path.read_text().splitlines():
            if "=" in line and not line.lstrip().startswith("#"):
                k, v = line.split("=", 1)
                if k.strip() == key:
                    return v.strip()
    except OSError:
        pass
    return default


def conf_set(key: str, value: str):
    CONFIG_DIR.mkdir(parents=True, exist_ok=True)
    path = CONFIG_DIR / "atlas.conf"
    lines = path.read_text().splitlines() if path.exists() else [
        "# Project Atlas · configuración del usuario (clave=valor)"]
    out, done = [], False
    for line in lines:
        if "=" in line and not line.lstrip().startswith("#") and line.split("=", 1)[0].strip() == key:
            out.append(f"{key}={value}")
            done = True
        else:
            out.append(line)
    if not done:
        out.append(f"{key}={value}")
    tmp = path.with_suffix(".tmp")
    tmp.write_text("\n".join(out) + "\n")
    tmp.replace(path)
