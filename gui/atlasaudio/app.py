"""Atlas Audio · interfaz GTK4 + libadwaita (Wayland).

Centro de control de audio al estilo de los paneles de las tarjetas de
sonido (efectos, ecualizador, altavoces, mezclador, perfiles) más todo lo de
Project Atlas: salud por niveles, recuperación escalonada, protección al
suspender, Hardware DB y diagnóstico.
"""
from __future__ import annotations

import os
import sys
from typing import ClassVar

os.environ.setdefault("GDK_BACKEND", "wayland")

import gi

gi.require_version("Gtk", "4.0")
gi.require_version("Adw", "1")
# gi.require_version() debe ir antes de importar gi.repository (por eso E402)
from gi.repository import Adw, Gdk, Gio, GLib, Gtk, Pango  # noqa: E402

from . import APP_ID, __version__  # noqa: E402
from . import backend as be  # noqa: E402
from .widgets import EqCurve, Meter, SpeakerMap  # noqa: E402

REFRESH_MS = 4000

CSS = """
window.atlas { background-color: #0e1014; color: #e8ecf1; }
.atlas headerbar { background-color: #12151a; box-shadow: inset 0 -1px #252b34; }
.sidebar { background-color: #101318; border-right: 1px solid #252b34; }
.sidebar row { border-radius: 8px; padding: 10px 12px; margin: 2px 6px; color: #b4bdc9; }
.sidebar row:selected { background-color: #1e1a20; color: #e8ecf1; font-weight: 600; }
.sidebar row:selected image { color: #ff4d6d; }
.side-title { color: #6b7482; font-size: 11px; font-weight: 600; letter-spacing: 1px; margin: 14px 18px 4px 18px; }
.panel { background-color: #161a20; border: 1px solid #252b34; border-radius: 14px; padding: 16px 18px; }
.tile { background-color: #161a20; border: 1px solid #252b34; border-radius: 12px; padding: 12px 14px; }
.tile.ok { border-color: #1f4a39; } .tile.warn { border-color: #5a4524; } .tile.fail { border-color: #6a2a2f; }
.tile-title { color: #9aa4b2; font-size: 12px; }
.tile-value { font-family: "JetBrains Mono", monospace; font-weight: 600; font-size: 22px; }
.tile-sub { color: #8b95a3; font-size: 11px; }
.mono { font-family: "JetBrains Mono", "DejaVu Sans Mono", monospace; }
.section-title { font-size: 14px; font-weight: 600; color: #c7cfda; }
.page-title { font-size: 22px; font-weight: 700; }
.dim { color: #9aa4b2; } .small { font-size: 11px; }
.pill { background-color: #1f252e; color: #c7cfda; border-radius: 999px; padding: 3px 10px;
        font-family: "JetBrains Mono", monospace; font-size: 12px; }
.pill.ok { background-color: #15241d; color: #7ff0bd; }
.pill.warn { background-color: #2a2116; color: #ffc77a; }
.pill.fail { background-color: #2a1717; color: #ff9b9b; }
.pill.accent { background-color: #2a141a; color: #ff8fa3; }
.pill.unknown { background-color: #1f252e; color: #9aa4b2; }
.icon-ok { color: #3ddc97; } .icon-warn { color: #ffc77a; } .icon-fail { color: #ff8a8a; }
.icon-unknown { color: #6aa8ff; }
.profile-card { background-color: #1b2028; border: 1px solid #2c333e; border-radius: 14px; padding: 14px; }
.profile-card.current { border-color: #ff4d6d; background-color: #211a20; }
.profile-name { font-weight: 700; font-size: 15px; }
.profile-icon { color: #ff8fa3; }
.fx-card { background-color: #1b2028; border: 1px solid #2c333e; border-radius: 14px; padding: 14px 16px; }
.fx-card.on { border-color: #ff4d6d; }
.fx-name { font-weight: 700; font-size: 15px; }
.fx-value { font-family: "JetBrains Mono", monospace; font-size: 20px; font-weight: 600; color: #ff8fa3; }
.eq-band { font-family: "JetBrains Mono", monospace; font-size: 11px; color: #9aa4b2; }
.eq-db { font-family: "JetBrains Mono", monospace; font-size: 11px; color: #ff8fa3; }
.statusbar { background-color: #101318; border-top: 1px solid #252b34; padding: 6px 16px; color: #8b95a3;
             font-size: 12px; }
.check-row { padding: 10px 4px; border-bottom: 1px solid #222831; }
.fix { background-color: #0e1014; border: 1px solid #252b34; border-radius: 8px; padding: 6px 10px;
       font-family: "JetBrains Mono", monospace; font-size: 12px; }
.log { background-color: #0b0d10; border: 1px solid #252b34; border-radius: 10px; }
.log text { background-color: #0b0d10; color: #c7cfda; }
.hero { background: linear-gradient(135deg, #2a141a 0%, #161a20 60%); border: 1px solid #3a2028;
        border-radius: 16px; padding: 18px 20px; }
.hero-title { font-size: 20px; font-weight: 700; }
.big-button { padding: 10px 18px; font-weight: 600; }
scale.accent trough highlight { background-color: #ff4d6d; }
scale.eq trough { min-width: 6px; }
scale.eq trough highlight { background-color: #ff4d6d; }
.warnbox { background-color: #2a2116; color: #ffd9a3; border-radius: 10px; padding: 10px 12px; }
"""
CSS_ACCENT_OLD = "@define-color accent_bg_color #ff4d6d; @define-color accent_color #ff8fa3; " \
                 "@define-color accent_fg_color #1a0709;"
CSS_ACCENT_VARS = ":root { --accent-bg-color: #ff4d6d; --accent-color: #ff8fa3; --accent-fg-color: #1a0709; }"

LEVEL_ICON = {"ok": "atlas-ok-symbolic", "warn": "dialog-warning-symbolic",
              "fail": "dialog-error-symbolic", "unknown": "dialog-question-symbolic"}
CATEGORIES = [("juegos", "Juegos"), ("multimedia", "Vídeo y navegación"), ("musica", "Música"),
              ("comunicacion", "Comunicación"), ("sistema", "Sistema")]


# --- utilidades ---------------------------------------------------------------

def lbl(text="", css=(), xalign=0.0, **kw) -> Gtk.Label:
    return Gtk.Label(label=text, xalign=xalign, css_classes=list(css), **kw)


def clear(box: Gtk.Widget):
    child = box.get_first_child()
    while child:
        nxt = child.get_next_sibling()
        box.remove(child)
        child = nxt


def panel(title: str | None = None, spacing=12, subtitle: str | None = None) -> Gtk.Box:
    b = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=spacing, css_classes=["panel"])
    if title:
        b.append(lbl(title, ["section-title"]))
    if subtitle:
        b.append(lbl(subtitle, ["dim", "small"], wrap=True))
    return b


def page_box() -> Gtk.Box:
    return Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=16, margin_top=20, margin_bottom=24,
                   margin_start=24, margin_end=24)


def scrolled(child) -> Gtk.ScrolledWindow:
    sw = Gtk.ScrolledWindow(hscrollbar_policy=Gtk.PolicyType.NEVER, vexpand=True)
    sw.set_child(child)
    return sw


def page_header(title: str, subtitle: str) -> Gtk.Box:
    b = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=2)
    b.append(lbl(title, ["page-title"]))
    b.append(lbl(subtitle, ["dim"], wrap=True))
    return b


def pill(text, kind="") -> Gtk.Label:
    return lbl(text, ["pill"] + ([kind] if kind else []))


def icon(name, size=16, css=()):
    im = Gtk.Image.new_from_icon_name(name)
    im.set_pixel_size(size)
    for c in css:
        im.add_css_class(c)
    return im


def short_dev(desc: str) -> str:
    """'CA0132 Sound Core3D [Sound Blaster ...] (SB1570 ...) Analog Surround 5.1' -> legible."""
    if "[" in desc and "]" in desc:
        head = desc.split("[", maxsplit=1)[0].strip()
        tail = desc.rsplit(")", 1)[-1].strip() if ")" in desc else ""
        return f"{head} · {tail}" if tail else head
    return desc


class Debounce:
    """Agrupa cambios rápidos de deslizadores en una sola acción."""

    def __init__(self, ms, fn):
        self.ms, self.fn, self.src, self.args = ms, fn, 0, None

    def __call__(self, *args):
        self.args = args
        if self.src:
            GLib.source_remove(self.src)
        self.src = GLib.timeout_add(self.ms, self._fire)

    def _fire(self):
        self.src = 0
        be.run_async(self.fn, lambda *_: None, *self.args)
        return False


def dsp_map(state) -> dict:
    return {c["name"]: c for c in (state or {}).get("controls", [])}


def ctl_int(c) -> int:
    try:
        return int(str(c["values"]).split(",")[0])
    except (KeyError, ValueError, IndexError):
        return 0


def ctl_on(c) -> bool:
    return str(c.get("values", "")).split(",")[0] == "on"


def ctl_enum(c) -> int:
    return ctl_int(c)


# --- Inicio -------------------------------------------------------------------

class LevelTile(Gtk.Box):
    def __init__(self, title):
        super().__init__(orientation=Gtk.Orientation.VERTICAL, spacing=6, css_classes=["tile"], hexpand=True)
        top = Gtk.Box(spacing=8)
        self.img = icon("dialog-question-symbolic", 18, ["icon-unknown"])
        top.append(self.img)
        top.append(lbl(title, ["tile-title"], hexpand=True))
        self.state = pill("…", "unknown")
        top.append(self.state)
        self.append(top)
        self.detail = lbl("", ["tile-sub"], wrap=True, wrap_mode=Pango.WrapMode.WORD_CHAR, lines=3,
                          ellipsize=Pango.EllipsizeMode.END)
        self.append(self.detail)

    def set(self, level, text, detail):
        for k in ("ok", "warn", "fail", "unknown"):
            self.remove_css_class(k)
        self.add_css_class(level if level != "unknown" else "tile")
        self.img.set_from_icon_name(LEVEL_ICON.get(level, LEVEL_ICON["unknown"]))
        self.img.set_css_classes([f"icon-{level}"])
        self.state.set_label(text)
        self.state.set_css_classes(["pill", level])
        self.detail.set_label(detail)


class DeviceCard(Gtk.Box):
    """Salida o entrada por defecto con volumen y mute."""

    def __init__(self, win, kind):
        super().__init__(orientation=Gtk.Orientation.VERTICAL, spacing=10, css_classes=["panel"], hexpand=True)
        self.win, self.kind, self.name, self._upd = win, kind, None, False
        head = Gtk.Box(spacing=10)
        head.append(icon("audio-speakers-symbolic" if kind == "sink" else "audio-input-microphone-symbolic", 22,
                         ["profile-icon"]))
        col = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, hexpand=True)
        col.append(lbl("Salida" if kind == "sink" else "Entrada", ["tile-title"]))
        self.dev = lbl("—", ["section-title"], ellipsize=Pango.EllipsizeMode.END)
        col.append(self.dev)
        head.append(col)
        self.mute = Gtk.ToggleButton(icon_name="audio-volume-muted-symbolic", valign=Gtk.Align.CENTER,
                                     tooltip_text="Silenciar")
        self.mute.connect("toggled", self._muted)
        head.append(self.mute)
        if kind == "sink":
            tone = Gtk.Button(icon_name="media-playback-start-symbolic", valign=Gtk.Align.CENTER,
                              tooltip_text="Tono de prueba")
            tone.connect("clicked", lambda *_: win.test_tone())
            head.append(tone)
        self.append(head)
        row = Gtk.Box(spacing=10)
        self.scale = Gtk.Scale.new_with_range(Gtk.Orientation.HORIZONTAL, 0, 150, 1)
        self.scale.set_hexpand(True)
        self.scale.add_css_class("accent")
        self.scale.add_mark(100, Gtk.PositionType.BOTTOM, None)
        self.scale.set_draw_value(False)
        self._deb = Debounce(80, be.set_volume)
        self.scale.connect("value-changed", self._vol)
        row.append(self.scale)
        self.val = lbl("—", ["tile-value"], width_chars=5, xalign=1)
        row.append(self.val)
        self.append(row)

    def update(self, node):
        self._upd = True
        if node:
            self.name = node["name"]
            self.dev.set_label(short_dev(node["description"]))
            self.dev.set_tooltip_text(node["description"])
            v = max(0, node.get("volume_percent", 0))
            if not self.scale.has_css_class("dragging"):
                self.scale.set_value(round(v))
            self.val.set_label(f"{v:.0f}%")
            self.mute.set_active(bool(node.get("mute")))
        else:
            self.dev.set_label("sin dispositivo")
        self._upd = False

    def _vol(self, s):
        self.val.set_label(f"{s.get_value():.0f}%")
        if not self._upd and self.name:
            self._deb(self.kind, self.name, int(s.get_value()))

    def _muted(self, b):
        if not self._upd and self.name:
            be.run_async(be.set_mute, lambda *_: None, self.kind, self.name, b.get_active())


class HomePage(Gtk.Box):
    def __init__(self, win):
        super().__init__(orientation=Gtk.Orientation.VERTICAL)
        self.win = win
        box = page_box()

        hero = Gtk.Box(spacing=16, css_classes=["hero"])
        col = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=6, hexpand=True)
        self.hero_title = lbl("Comprobando el audio…", ["hero-title"])
        col.append(self.hero_title)
        self.hero_sub = lbl("", ["dim"], wrap=True)
        col.append(self.hero_sub)
        self.meter = Meter()
        self.meter.set_margin_top(6)
        col.append(self.meter)
        hero.append(col)
        btns = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=8, valign=Gtk.Align.CENTER)
        b = Gtk.Button(label="Comprobar ahora", css_classes=["big-button"])
        b.connect("clicked", lambda *_: win.refresh(probe=True))
        btns.append(b)
        self.fix_btn = Gtk.Button(label="Recuperar audio", css_classes=["big-button", "suggested-action"])
        self.fix_btn.connect("clicked", lambda *_: win.go("recover", run=True))
        btns.append(self.fix_btn)
        hero.append(btns)
        box.append(hero)

        grid = Gtk.Grid(column_spacing=12, row_spacing=12, column_homogeneous=True)
        self.tiles = [LevelTile(t) for t in ("Servicios", "Conexión", "Hardware", "Acústica")]
        for i, t in enumerate(self.tiles):
            grid.attach(t, i, 0, 1, 1)
        box.append(grid)

        devs = Gtk.Box(spacing=12, homogeneous=True)
        self.sink = DeviceCard(win, "sink")
        self.source = DeviceCard(win, "source")
        devs.append(self.sink)
        devs.append(self.source)
        box.append(devs)

        info = Gtk.Box(spacing=12, homogeneous=True)
        self.clock = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=4, css_classes=["tile"])
        self.clock.append(lbl("Reloj de PipeWire", ["tile-title"]))
        self.clock_v = lbl("—", ["tile-value"])
        self.clock.append(self.clock_v)
        self.clock_s = lbl("", ["tile-sub"])
        self.clock.append(self.clock_s)
        info.append(self.clock)
        self.prof = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=4, css_classes=["tile"])
        self.prof.append(lbl("Perfil", ["tile-title"]))
        self.prof_v = lbl("—", ["tile-value"])
        self.prof.append(self.prof_v)
        self.prof_s = lbl("", ["tile-sub"], ellipsize=Pango.EllipsizeMode.END)
        self.prof.append(self.prof_s)
        info.append(self.prof)
        self.prot = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=4, css_classes=["tile"])
        self.prot.append(lbl("Protección al suspender", ["tile-title"]))
        self.prot_v = lbl("—", ["tile-value"])
        self.prot.append(self.prot_v)
        self.prot_s = lbl("", ["tile-sub"], ellipsize=Pango.EllipsizeMode.END)
        self.prot.append(self.prot_s)
        info.append(self.prot)
        box.append(info)

        quick = panel("Perfiles rápidos")
        self.quick = Gtk.FlowBox(selection_mode=Gtk.SelectionMode.NONE, max_children_per_line=7,
                                 column_spacing=8, row_spacing=8, homogeneous=True)
        quick.append(self.quick)
        box.append(quick)
        self.append(scrolled(box))

    def set_profiles(self, profiles):
        clear(self.quick)
        for pid in ("gaming", "fps", "racing", "adventure", "retro", "video", "music", "browsing", "cinema",
                    "night"):
            p = next((x for x in profiles if x["id"] == pid), None)
            if not p:
                continue
            b = Gtk.Button(css_classes=["big-button"])
            c = Gtk.Box(spacing=8)
            c.append(icon(p.get("icon") or "audio-x-generic-symbolic", 16, ["profile-icon"]))
            c.append(lbl(p["name"]))
            b.set_child(c)
            b.connect("clicked", lambda _b, i=pid: self.win.apply_profile(i))
            self.quick.append(b)

    def update(self, st):
        h = st.get("health", {})
        lv = h.get("levels", {})
        order = ("services", "connection", "hardware", "acoustic")
        names = {"ok": "OK", "warn": "Aviso", "fail": "Fallo", "unknown": "Sin verificar"}
        for t, k in zip(self.tiles, order, strict=False):
            d = lv.get(k, {})
            level = d.get("level", "unknown")
            t.set(level, names.get(level, level), d.get("detail", ""))
        self.meter.set_levels([lv.get(k, {}).get("level", "unknown") for k in order])
        overall = h.get("overall", "unknown")
        hw = lv.get("hardware", {}).get("level")
        if overall == "fail":
            self.hero_title.set_label("El audio no está llegando a los altavoces")
            self.hero_sub.set_label(lv.get("hardware", {}).get("detail") or lv.get("connection", {}).get("detail", ""))
            self.fix_btn.set_visible(True)
        elif hw == "ok":
            self.hero_title.set_label("Todo suena bien")
            self.hero_sub.set_label("La tarjeta está consumiendo audio de verdad (verificado en hardware).")
            self.fix_btn.set_visible(False)
        else:
            self.hero_title.set_label("Servicios y conexión correctos")
            self.hero_sub.set_label("No hay reproducción que medir ahora. «Comprobar ahora» envía un instante de "
                                    "silencio a la tarjeta para verificarla.")
            self.fix_btn.set_visible(False)

        audio = h.get("audio", {})
        sink = next((s for s in audio.get("sinks", []) if s.get("default")), None)
        src = next((s for s in audio.get("sources", []) if s.get("default")), None)
        self.sink.update(sink)
        self.source.update(src)

        c = st.get("clock", {})
        if c.get("ok"):
            rate = c["force_rate"] or c["rate"]
            q = c["force_quantum"] or c["quantum"]
            self.clock_v.set_label(f"{c.get('latency_ms', 0):.1f} ms")
            self.clock_s.set_label(f"{rate} Hz · quantum {q}{' (forzado)' if c['force_quantum'] else ''}")
        self.prof_v.set_label(self.win.profile_name(st.get("profile", "—")))
        a = self.win.auto or {}
        if a.get("manual_lock"):
            self.prof_s.set_label("manual · automático en pausa")
        elif a.get("enabled"):
            self.prof_s.set_label("automático · " + (a.get("reason") or "esperando audio"))
        else:
            self.prof_s.set_label("automático desactivado")
        w = self.win.watch or {}
        self.prot_v.set_label("Activa" if w.get("active") else "Inactiva")
        self.prot_s.set_label("cierra la tarjeta antes de dormir y verifica al volver" if w.get("active")
                              else "actívala en Recuperación")


# --- Perfiles -----------------------------------------------------------------

class ModeSwitch(Gtk.Box):
    """Selector Automático | Manual; refleja el modo que está funcionando."""

    def __init__(self, on_change):
        super().__init__(css_classes=["linked"], valign=Gtk.Align.CENTER)
        self._upd = False
        self._cb = on_change
        self.auto_btn = Gtk.ToggleButton(label="Automático")
        self.man_btn = Gtk.ToggleButton(label="Manual", group=self.auto_btn)
        self.auto_btn.set_tooltip_text("Atlas elige el perfil según la aplicación o el juego que suena")
        self.man_btn.set_tooltip_text("Los perfiles solo cambian cuando los eliges tú")
        for b, m in ((self.auto_btn, "auto"), (self.man_btn, "manual")):
            b.connect("toggled", lambda btn, m=m: None if self._upd or not btn.get_active() else self._cb(m))
            self.append(b)

    def set_mode(self, mode, current_name=""):
        self._upd = True
        self.auto_btn.set_active(mode != "manual")
        self.man_btn.set_active(mode == "manual")
        self._upd = False
        if mode == "auto-paused":
            self.auto_btn.set_label("Automático · en pausa")
            self.auto_btn.set_tooltip_text(f"Elegiste «{current_name}» a mano: Atlas vuelve a elegir solo "
                                           "cuando cambie lo que suena. Pulsa para volver ya.")
        else:
            self.auto_btn.set_label("Automático")
            self.auto_btn.set_tooltip_text("Atlas elige el perfil según la aplicación o el juego que suena")


class ProfilesPage(Gtk.Box):
    def __init__(self, win):
        super().__init__(orientation=Gtk.Orientation.VERTICAL)
        self.win = win
        self._upd = False
        box = page_box()
        box.append(page_header("Perfiles", "Cada perfil ajusta la latencia de PipeWire para un uso concreto. "
                                           "Tus efectos y ecualizador se mantienen con todos."))

        auto = panel()
        top = Gtk.Box(spacing=12)
        col = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=2, hexpand=True)
        col.append(lbl("Modo de perfiles", ["section-title"]))
        self.auto_lbl = lbl("", ["dim"], wrap=True)
        col.append(self.auto_lbl)
        top.append(col)
        self.resume_btn = Gtk.Button(label="Volver a automático ya", valign=Gtk.Align.CENTER)
        self.resume_btn.connect("clicked", lambda *_: self.win.set_mode("auto"))
        top.append(self.resume_btn)
        self.mode_sw = ModeSwitch(self.win.set_mode)
        top.append(self.mode_sw)
        auto.append(top)
        auto.append(lbl("Aplicaciones sonando ahora (asigna un perfil para recordarlo):", ["dim", "small"]))
        self.streams = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=6)
        auto.append(self.streams)
        box.append(auto)

        self.groups = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=16)
        box.append(self.groups)
        self.append(scrolled(box))
        self.profiles = []


    def set_profiles(self, profiles, current):
        self.profiles = profiles
        clear(self.groups)
        for cat, title in CATEGORIES:
            items = [p for p in profiles if (p.get("category") or "sistema") == cat]
            if not items:
                continue
            g = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=10)
            g.append(lbl(title, ["section-title"]))
            fb = Gtk.FlowBox(selection_mode=Gtk.SelectionMode.NONE, max_children_per_line=4, min_children_per_line=2,
                             column_spacing=12, row_spacing=12, homogeneous=True)
            for p in items:
                fb.append(self._card(p, p["id"] == current))
            g.append(fb)
            self.groups.append(g)

    def _card(self, p, current):
        c = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=8,
                    css_classes=["profile-card"] + (["current"] if current else []))
        top = Gtk.Box(spacing=10)
        top.append(icon(p.get("icon") or "audio-x-generic-symbolic", 24, ["profile-icon"]))
        top.append(lbl(p["name"], ["profile-name"], hexpand=True, ellipsize=Pango.EllipsizeMode.END))
        if current:
            top.append(pill("activo", "accent"))
        c.append(top)
        c.append(lbl(p.get("description", ""), ["dim", "small"], wrap=True, lines=3,
                     ellipsize=Pango.EllipsizeMode.END, vexpand=True, yalign=0))
        pills = Gtk.Box(spacing=6)
        lat = p.get("latency_ms") or 0
        pills.append(pill(f"{lat:.1f} ms" if lat else "latencia auto"))
        pills.append(pill(f"{p['rate'] // 1000 if p['rate'] % 1000 == 0 else p['rate'] / 1000} kHz"
                          if p.get("rate") else "rate nativo"))
        if p.get("source") == "user":
            pills.append(pill("tuyo", "accent"))
        c.append(pills)
        b = Gtk.Button(label="Aplicado" if current else "Aplicar",
                       css_classes=[] if current else ["suggested-action"], sensitive=not current)
        b.connect("clicked", lambda *_: self.win.apply_profile(p["id"]))
        c.append(b)
        return c

    def set_auto(self, a):
        mode = a.get("mode") or ("manual" if not a.get("enabled") else
                                 "auto-paused" if a.get("manual_lock") else "auto")
        cur = self.win.profile_name(a.get("current"))
        self.mode_sw.set_mode(mode, cur)
        self.resume_btn.set_visible(mode == "auto-paused")
        if mode == "manual":
            self.auto_lbl.set_label("Modo manual: los perfiles solo cambian cuando los aplicas tú.")
        elif mode == "auto-paused":
            self.auto_lbl.set_label(f"Automático en pausa: elegiste «{cur}» a mano. Atlas vuelve a elegir "
                                    "solo cuando cambie lo que suena (otra aplicación o un juego).")
        else:
            self.auto_lbl.set_label("Atlas elige el perfil según la aplicación o el juego que suena. "
                                    + (f"Ahora: {a['reason']}." if a.get("reason") else ""))
        clear(self.streams)
        ids = [p["id"] for p in self.profiles]
        names = ["(sin regla)"] + [p["name"] for p in self.profiles]
        shown = 0
        for s in a.get("streams", []):
            if not (s.get("binary") or s.get("steam_appid")):
                continue
            shown += 1
            row = Gtk.Box(spacing=10, css_classes=["check-row"])
            kind_icon = "applications-games-symbolic" if s.get("steam_appid") else "application-x-executable-symbolic"
            row.append(icon(kind_icon, 16, ["dim"]))
            who = s.get("binary") or s.get("app")
            if s.get("steam_appid"):
                who += f"  · Steam {s['steam_appid']}"
            col = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, hexpand=True)
            col.append(lbl(who, ["mono"]))
            if s.get("mpris"):
                col.append(lbl("▶ " + s["mpris"], ["small"], ellipsize=Pango.EllipsizeMode.END))
            elif s.get("title"):
                col.append(lbl(s["title"], ["dim", "small"], ellipsize=Pango.EllipsizeMode.END))
            row.append(col)
            if s.get("genre"):
                row.append(pill({"racing": "carreras", "fps": "shooter", "adventure": "aventura",
                                 "gaming": "juego"}.get(s["genre"], s["genre"]), "accent"))
            if s.get("corked"):
                row.append(pill("en pausa"))
            dd = Gtk.DropDown.new_from_strings(names)
            cur = s.get("profile")
            dd.set_selected(ids.index(cur) + 1 if cur in ids else 0)
            dd.connect("notify::selected", lambda d, _p, st=s: self._assign(st, d.get_selected()))
            row.append(dd)
            self.streams.append(row)
        if not shown:
            self.streams.append(lbl("Nada sonando ahora mismo.", ["dim", "small"]))

    def _assign(self, s, idx):
        if idx == 0:
            return
        pid = self.profiles[idx - 1]["id"]
        cond = f"steam={s['steam_appid']}" if s.get("steam_appid") else f"binary={s['binary']}"
        be.run_async(be.auto_assign, lambda r, e: (self.win.toast(f"Regla guardada: {pid} ← {cond}"),
                                                   self.win.refresh_auto()), pid, cond)


# --- Efectos (DSP) --------------------------------------------------------------

FX_INFO = {
    "Surround": ("Surround", "Amplía el estéreo y lo envuelve en los altavoces.", "%"),
    "Crystalizer": ("Crystalizer", "Devuelve brillo y detalle al audio comprimido.", "%"),
    "Dialog Plus": ("Dialog Plus", "Separa y refuerza las voces.", "%"),
    "Smart Volume": ("Smart Volume", "Nivela el volumen entre fuentes y escenas.", "%"),
    "X-Bass": ("X-Bass", "Recupera graves perdidos con altavoces pequeños.", "%"),
}


class EffectCard(Gtk.Box):
    def __init__(self, win, key, ctl):
        super().__init__(orientation=Gtk.Orientation.VERTICAL, spacing=10, css_classes=["fx-card"])
        self.win, self.key, self._upd = win, key, False
        title, desc, _ = FX_INFO.get(key, (key, "", "%"))
        top = Gtk.Box(spacing=10)
        col = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, hexpand=True)
        col.append(lbl(title, ["fx-name"]))
        col.append(lbl(desc, ["dim", "small"], wrap=True))
        top.append(col)
        self.sw = Gtk.Switch(valign=Gtk.Align.START)
        self.sw.connect("notify::active", self._toggled)
        top.append(self.sw)
        self.append(top)
        row = Gtk.Box(spacing=10)
        self.scale = Gtk.Scale.new_with_range(Gtk.Orientation.HORIZONTAL, 0, 100, 1)
        self.scale.set_hexpand(True)
        self.scale.set_draw_value(False)
        self.scale.add_css_class("accent")
        self._deb = Debounce(120, be.dsp_set)
        self.scale.connect("value-changed", self._moved)
        row.append(self.scale)
        self.val = lbl("—", ["fx-value"], width_chars=4, xalign=1)
        row.append(self.val)
        self.append(row)
        self.extra = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=6)
        self.append(self.extra)
        self.update(ctl)

    def update(self, ctl):
        self._upd = True
        sw, vol = ctl.get("switch"), ctl.get("volume")
        if sw:
            self.sw.set_active(ctl_on(sw))
        if vol:
            self.scale.set_range(vol["min"], vol["max"])
            self.scale.set_value(ctl_int(vol))
            self.val.set_label(f"{ctl_int(vol)}")
        self.scale.set_sensitive(self.sw.get_active())
        if self.sw.get_active():
            self.add_css_class("on")
        else:
            self.remove_css_class("on")
        self._upd = False

    def _toggled(self, s, _p):
        on = s.get_active()
        self.scale.set_sensitive(on)
        (self.add_css_class if on else self.remove_css_class)("on")
        if not self._upd:
            be.run_async(be.dsp_set, lambda *_: None, f"FX: {self.key} Playback Switch", "on" if on else "off")

    def _moved(self, s):
        self.val.set_label(f"{int(s.get_value())}")
        if not self._upd:
            self._deb(f"FX: {self.key} Playback Volume", int(s.get_value()))


class EffectsPage(Gtk.Box):
    def __init__(self, win):
        super().__init__(orientation=Gtk.Orientation.VERTICAL)
        self.win = win
        self._upd = False
        box = page_box()
        box.append(page_header("Efectos", "Procesado por hardware en el DSP de la tarjeta: no consume CPU ni "
                                          "añade latencia."))
        bar = panel()
        row = Gtk.Box(spacing=10)
        row.append(lbl("Procesado de efectos", ["section-title"], hexpand=True))
        self.master = Gtk.Switch(valign=Gtk.Align.CENTER)
        self.master.connect("notify::active", self._master)
        row.append(self.master)
        bar.append(row)
        prow = Gtk.Box(spacing=8)
        prow.append(lbl("Preset del DSP:", ["dim"]))
        self.presets = Gtk.DropDown.new_from_strings(["—"])
        prow.append(self.presets)
        ap = Gtk.Button(label="Aplicar preset")
        ap.connect("clicked", self._apply_preset)
        prow.append(ap)
        prow.append(Gtk.Box(hexpand=True))
        self.save_name = Gtk.Entry(placeholder_text="nombre (p. ej. fps)", width_chars=16)
        prow.append(self.save_name)
        sv = Gtk.Button(label="Guardar actuales")
        sv.set_tooltip_text("Guarda los efectos y el ecualizador actuales como preset tuyo. Si el nombre "
                            "coincide con un perfil (fps, music…), ese perfil usará tu versión.")
        sv.connect("clicked", self._save)
        prow.append(sv)
        bar.append(prow)
        box.append(bar)
        self.grid = Gtk.FlowBox(selection_mode=Gtk.SelectionMode.NONE, max_children_per_line=3,
                                min_children_per_line=1, column_spacing=12, row_spacing=12, homogeneous=True)
        box.append(self.grid)
        self.missing = lbl("", ["warnbox"], wrap=True)
        self.missing.set_visible(False)
        box.append(self.missing)
        self.append(scrolled(box))
        self.cards = {}
        self.preset_names = []

    def update(self, state, presets):
        m = dsp_map(state)
        fx = {}
        for name in m:
            if name.startswith("FX: ") and name.endswith(" Playback Switch"):
                key = name[4:-16]
                if key in ("Equalizer",):
                    continue
                fx[key] = {"switch": m[name], "volume": m.get(f"FX: {key} Playback Volume")}
        if not fx:
            self.missing.set_label("Esta tarjeta no expone efectos por hardware. Los perfiles siguen ajustando "
                                   "la latencia de PipeWire.")
            self.missing.set_visible(True)
        order = ["Surround", "Crystalizer", "Dialog Plus", "Smart Volume", "X-Bass"]
        keys = [k for k in order if k in fx] + [k for k in fx if k not in order]
        if True:  # se reconstruye siempre: un perfil cambia también modo y crossover
            clear(self.grid)
            self.cards = {}
            for k in keys:
                card = EffectCard(self.win, k, fx[k])
                if k == "Smart Volume" and "FX: Smart Volume Setting" in m:
                    c = m["FX: Smart Volume Setting"]
                    dd = Gtk.DropDown.new_from_strings(c["items"])
                    dd.set_selected(ctl_enum(c))
                    dd.connect("notify::selected", lambda d, _p, items=c["items"]: be.run_async(
                        be.dsp_set, lambda *_: None, "FX: Smart Volume Setting", items[d.get_selected()]))
                    r = Gtk.Box(spacing=8)
                    r.append(lbl("Modo", ["dim"], hexpand=True))
                    r.append(dd)
                    card.extra.append(r)
                    card.smart_dd = dd
                if k == "X-Bass" and "FX: X-Bass Crossover Playback Volume" in m:
                    c = m["FX: X-Bass Crossover Playback Volume"]
                    r = Gtk.Box(spacing=8)
                    r.append(lbl("Crossover", ["dim"]))
                    sc = Gtk.Scale.new_with_range(Gtk.Orientation.HORIZONTAL, c["min"], c["max"], 1)
                    sc.set_hexpand(True)
                    sc.set_draw_value(False)
                    sc.add_css_class("accent")
                    hz = lbl("", ["mono", "small"], width_chars=7, xalign=1)
                    deb = Debounce(120, be.dsp_set)

                    def moved(s, hz=hz, deb=deb, card=card):
                        hz.set_label(f"{int(s.get_value()) * 10} Hz")
                        if not card._upd:
                            deb("FX: X-Bass Crossover Playback Volume", int(s.get_value()))
                    sc.connect("value-changed", moved)
                    card._upd = True
                    sc.set_value(ctl_int(c))
                    hz.set_label(f"{ctl_int(c) * 10} Hz")
                    card._upd = False
                    r.append(sc)
                    r.append(hz)
                    card.extra.append(r)
                self.cards[k] = card
                self.grid.append(card)
        if "Enable OutFX Playback Switch" in m:
            self._upd = True
            self.master.set_active(ctl_on(m["Enable OutFX Playback Switch"]))
            self._upd = False
        names = [p["name"] for p in presets or []]
        if names != self.preset_names:
            self.preset_names = names
            self.presets.set_model(Gtk.StringList.new(names or ["—"]))
        cur = self.win.status.get("profile")
        if cur in names:
            self.presets.set_selected(names.index(cur))

    def _master(self, s, _p):
        if not self._upd:
            be.run_async(be.dsp_set, lambda *_: None, "Enable OutFX Playback Switch",
                         "on" if s.get_active() else "off")

    def _apply_preset(self, *_):
        if not self.preset_names:
            return
        name = self.preset_names[self.presets.get_selected()]
        be.run_async(be.dsp_apply, lambda r, e: (self.win.toast(f"Preset DSP «{name}» aplicado"),
                                                 self.win.load_dsp()), name)

    def _save(self, *_):
        name = self.save_name.get_text().strip()
        if not name:
            self.win.toast("Escribe un nombre para el preset")
            return
        be.run_async(be.dsp_save, lambda r, e: (self.win.toast(r[1] if r else str(e)), self.win.load_dsp()), name)


# --- Ecualizador ---------------------------------------------------------------

class EqPage(Gtk.Box):
    BANDS = ("31", "62", "125", "250", "500", "1k", "2k", "4k", "8k", "16k")

    def __init__(self, win):
        super().__init__(orientation=Gtk.Orientation.VERTICAL)
        self.win = win
        self._upd = False
        box = page_box()
        box.append(page_header("Ecualizador", "10 bandas en el DSP de la tarjeta (±12 dB)."))
        top = panel()
        row = Gtk.Box(spacing=10)
        row.append(lbl("Ecualizador", ["section-title"], hexpand=True))
        row.append(lbl("Preset de fábrica", ["dim"]))
        self.preset = Gtk.DropDown.new_from_strings(["Flat"])
        self.preset.connect("notify::selected", self._preset)
        row.append(self.preset)
        flat = Gtk.Button(label="Plano")
        flat.connect("clicked", self._flat)
        row.append(flat)
        self.sw = Gtk.Switch(valign=Gtk.Align.CENTER)
        self.sw.connect("notify::active", self._toggle)
        row.append(self.sw)
        top.append(row)
        self.curve = EqCurve()
        top.append(self.curve)
        sliders = Gtk.Box(spacing=4, homogeneous=True, margin_top=6)
        self.scales, self.dbs = [], []
        self._deb = [Debounce(120, be.dsp_set) for _ in self.BANDS]
        for i, b in enumerate(self.BANDS):
            col = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=4)
            db = lbl("0", ["eq-db"], xalign=0.5)
            col.append(db)
            sc = Gtk.Scale.new_with_range(Gtk.Orientation.VERTICAL, -12, 12, 1)
            sc.set_inverted(True)
            sc.set_draw_value(False)
            sc.set_size_request(-1, 200)
            sc.add_css_class("eq")
            sc.add_mark(0, Gtk.PositionType.RIGHT, None)
            sc.connect("value-changed", self._moved, i)
            col.append(sc)
            col.append(lbl(b, ["eq-band"], xalign=0.5))
            sliders.append(col)
            self.scales.append(sc)
            self.dbs.append(db)
        top.append(sliders)
        box.append(top)
        box.append(lbl("Los cambios se aplican al instante y se guardan como tu sonido: se mantienen con "
                       "todos los perfiles y tras reiniciar.", ["dim", "small"], wrap=True))
        self.append(scrolled(box))
        self.items = []

    def update(self, state):
        m = dsp_map(state)
        self._upd = True
        gains = []
        for i, sc in enumerate(self.scales):
            c = m.get(f"EQ Band{i} Playback Volume")
            v = ctl_int(c) - 24 if c else 0
            sc.set_sensitive(c is not None)
            sc.set_value(v)
            self.dbs[i].set_label(f"{v:+d}")
            gains.append(v)
        sw = m.get("FX: Equalizer Playback Switch")
        if sw:
            self.sw.set_active(ctl_on(sw))
        pr = m.get("FX: Equalizer Preset Switch")
        if pr and pr["items"] != self.items:
            self.items = pr["items"]
            self.preset.set_model(Gtk.StringList.new(self.items))
        if pr:
            self.preset.set_selected(ctl_enum(pr))
        self.curve.set_gains(gains, self.sw.get_active())
        self._upd = False

    def _moved(self, sc, i):
        v = int(sc.get_value())
        self.dbs[i].set_label(f"{v:+d}")
        self.curve.set_gains([int(s.get_value()) for s in self.scales], self.sw.get_active())
        if not self._upd:
            self._deb[i](f"EQ Band{i} Playback Volume", v + 24)

    def _toggle(self, s, _p):
        self.curve.set_gains([int(x.get_value()) for x in self.scales], s.get_active())
        if not self._upd:
            be.run_async(be.dsp_set, lambda *_: None, "FX: Equalizer Playback Switch",
                         "on" if s.get_active() else "off")

    def _preset(self, d, _p):
        if self._upd or not self.items:
            return
        name = self.items[d.get_selected()]
        be.run_async(be.dsp_set, lambda *_: GLib.timeout_add(300, lambda: (self.win.load_dsp(), False)[1]),
                     "FX: Equalizer Preset Switch", name)

    def _flat(self, *_):
        for sc in self.scales:
            sc.set_value(0)


# --- Altavoces -----------------------------------------------------------------

class SpeakersPage(Gtk.Box):
    def __init__(self, win):
        super().__init__(orientation=Gtk.Orientation.VERTICAL)
        self.win = win
        self._upd = False
        box = page_box()
        box.append(page_header("Altavoces", "Configuración 5.1, gestión de graves y prueba de cada altavoz."))

        outp = panel()
        orow = Gtk.Box(spacing=12)
        ocol = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=2, hexpand=True)
        ocol.append(lbl("Salida", ["section-title"]))
        self.out_lbl = lbl("", ["dim", "small"], wrap=True)
        ocol.append(self.out_lbl)
        orow.append(ocol)
        seg = Gtk.Box(css_classes=["linked"], valign=Gtk.Align.CENTER)
        self.btn_spk = Gtk.ToggleButton()
        sb = Gtk.Box(spacing=6)
        sb.append(icon("audio-speakers-symbolic", 16))
        sb.append(lbl("Altavoces 5.1"))
        self.btn_spk.set_child(sb)
        self.btn_hp = Gtk.ToggleButton(group=self.btn_spk)
        hb = Gtk.Box(spacing=6)
        hb.append(icon("audio-headphones-symbolic", 16))
        hb.append(lbl("Auriculares"))
        self.btn_hp.set_child(hb)
        self.btn_spk.connect("toggled", lambda b: b.get_active() and self._out("speakers"))
        self.btn_hp.connect("toggled", lambda b: b.get_active() and self._out("headphones"))
        seg.append(self.btn_spk)
        seg.append(self.btn_hp)
        orow.append(seg)
        jcol = Gtk.Box(spacing=6, valign=Gtk.Align.CENTER)
        jcol.append(lbl("Cambiar al enchufar", ["dim", "small"]))
        self.sw_jack = Gtk.Switch(valign=Gtk.Align.CENTER)
        self.sw_jack.connect("notify::active", lambda s, _p: None if self._upd else
                             be.conf_set("output.follow_jack", "yes" if s.get_active() else "no"))
        jcol.append(self.sw_jack)
        orow.append(jcol)
        outp.append(orow)
        box.append(outp)

        row = Gtk.Box(spacing=12, homogeneous=True)
        left = panel("Prueba de altavoces", subtitle="Haz clic en un altavoz para enviarle un tono suave.")
        self.map = SpeakerMap(self._test)
        left.append(self.map)
        allb = Gtk.Button(label="Probar todos en secuencia")
        allb.connect("clicked", self._test_all)
        left.append(allb)
        row.append(left)

        right = panel("Configuración")
        self.out = self._enum_row(right, "Salida", "Output Select")
        self.cfg = self._enum_row(right, "Canales", "Surround Channel Config")
        self.frf = self._switch_row(right, "Frontales full-range", "Full-Range Front Speakers",
                                    "Desactívalo si los satélites no reproducen graves (X-530).")
        self.frr = self._switch_row(right, "Traseros full-range", "Full-Range Rear Speakers", "")
        self.br = self._switch_row(right, "Redirección de graves al subwoofer", "Bass Redirection",
                                   "Envía al subwoofer los graves de los canales no full-range.")
        xr = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=4)
        h = Gtk.Box(spacing=8)
        h.append(lbl("Crossover", hexpand=True))
        self.xo_lbl = lbl("—", ["fx-value"])
        h.append(self.xo_lbl)
        xr.append(h)
        self.xo = Gtk.Scale.new_with_range(Gtk.Orientation.HORIZONTAL, 1, 30, 1)
        self.xo.set_draw_value(False)
        self.xo.add_css_class("accent")
        for v in (8, 12, 16):
            self.xo.add_mark(v, Gtk.PositionType.BOTTOM, f"{v * 10}")
        self._xdeb = Debounce(150, be.dsp_set)
        self.xo.connect("value-changed", self._xo)
        xr.append(self.xo)
        xr.append(lbl("Solo actúa en los altavoces que no son full-range. Para satélites de X-530 se "
                      "recomienda 120 Hz.", ["dim", "small"], wrap=True))
        right.append(xr)
        brow = Gtk.Box(spacing=8, homogeneous=True)
        x530 = Gtk.Button(label="Probar graves al sub (120 Hz)")
        x530.set_tooltip_text("Satélites como no full-range y redirección a 120 Hz. Es una prueba: "
                              "«Guardar como base» lo hace permanente, «Volver a mi base» lo deshace.")
        x530.connect("clicked", lambda *_: be.run_async(be.dsp_apply, lambda *_: (
            self.win.toast("Prueba de graves aplicada (no es permanente)"), self.win.load_dsp()),
            "x530-graves-al-sub"))
        brow.append(x530)
        right.append(brow)
        row.append(right)
        box.append(row)

        # --- base de volumen ---------------------------------------------------
        vb = panel("Volumen base por altavoz",
                   subtitle="Tu referencia estable (importada de ~/.config/asound.state). Se aplica con todos "
                            "los perfiles y Atlas la restaura sola si algo la cambia.")
        grid = Gtk.Grid(column_spacing=14, row_spacing=8)
        self.vol = {}
        vb.append(lbl("Incluye el Master de la tarjeta. Tus teclas de volumen siguen funcionando: con la "
                      "protección activada mueven el volumen de PipeWire, no el de la tarjeta.",
                      ["dim", "small"], wrap=True))
        lk = Gtk.Box(spacing=8, css_classes=["check-row"])
        lkc = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, hexpand=True)
        lkc.append(lbl("Proteger la base frente a PipeWire"))
        lkc.append(lbl("PipeWire reescribe el mezclador de hardware al reconstruir la tarjeta (suspensión, "
                       "recuperación, auriculares). Activado, el volumen de las apps es software y tu base no "
                       "se mueve nunca.", ["dim", "small"], wrap=True))
        lk.append(lkc)
        self.sw_lock = Gtk.Switch(valign=Gtk.Align.CENTER, active=be.hwlock_on())
        self.sw_lock.connect("notify::active", lambda s, _p: be.run_async(
            be.hwlock_set, lambda r, e: (self.win.toast("Base protegida" if s.get_active() else "Protección quitada"),
                                         self.win.load_dsp(), self.win.refresh()), s.get_active()))
        lk.append(self.sw_lock)
        vb.append(lk)
        chans = [("Master", "Master Playback Volume", 1), ("Frontales", "Front Playback Volume", 2),
                 ("Traseros", "Surround Playback Volume", 2), ("Central", "Center Playback Volume", 1),
                 ("Subwoofer", "LFE Playback Volume", 1), ("PCM", "PCM Playback Volume", 2)]
        for r, (title, ctl, count) in enumerate(chans):
            grid.attach(lbl(title, width_chars=10), 0, r, 1, 1)
            sc = Gtk.Scale.new_with_range(Gtk.Orientation.HORIZONTAL, 0, 99, 1)
            sc.set_hexpand(True)
            sc.set_draw_value(False)
            sc.add_css_class("accent")
            val = lbl("—", ["mono"], width_chars=5, xalign=1)
            sc.ctl, sc.count, sc.val = ctl, count, val
            sc.deb = Debounce(120, be.dsp_set)
            sc.connect("value-changed", self._vol_moved)
            grid.attach(sc, 1, r, 1, 1)
            grid.attach(val, 2, r, 1, 1)
            self.vol[ctl] = sc
        vb.append(grid)
        vrow = Gtk.Box(spacing=8)
        sv = Gtk.Button(label="Guardar como base", css_classes=["suggested-action"])
        sv.set_tooltip_text("Guarda los volúmenes y la configuración de altavoces actuales como tu base")
        sv.connect("clicked", lambda *_: be.run_async(be.base_save, lambda r, e: self.win.toast(
            "Base guardada: se mantendrá con todos los perfiles" if r and r[0] == 0 else f"Error: {r or e}")))
        vrow.append(sv)
        rv = Gtk.Button(label="Volver a mi base")
        rv.connect("clicked", lambda *_: be.run_async(be.base_apply, lambda *_: (
            self.win.toast("Base de volumen y altavoces restaurada"), self.win.load_dsp())))
        vrow.append(rv)
        vrow.append(Gtk.Box(hexpand=True))
        im = Gtk.Button(label="Importar ~/.config/asound.state")
        im.set_tooltip_text("Vuelve a leer tu archivo de alsactl y reemplaza la base")
        im.connect("clicked", lambda *_: be.run_async(be.base_import, lambda r, e: (
            self.win.toast("asound.state importado como base" if r and r[0] == 0 else f"Error: {r or e}"),
            self.win.load_dsp())))
        vrow.append(im)
        vb.append(vrow)
        box.append(vb)
        self.append(scrolled(box))
        self.ctls = {}

    def _out(self, mode):
        if self._upd:
            return
        self.win.toast("Cambiando a " + ("auriculares…" if mode == "headphones" else "altavoces…"))
        be.run_async(be.output_set, lambda r, e: (
            self.win.toast(r[1].splitlines()[-1] if r and r[1] else str(e)),
            self.win.refresh_output(), self.win.load_dsp(), self.win.refresh()), mode)

    def update_output(self, o):
        self._upd = True
        hp = o.get("mode") == "headphones"
        self.btn_hp.set_active(hp)
        self.btn_spk.set_active(not hp)
        self.btn_hp.set_sensitive(bool(o.get("supported")))
        self.sw_jack.set_active(bool(o.get("follow_jack")))
        self.out_lbl.set_label(
            ("Auriculares: amplificador frontal, PipeWire en estéreo y surround virtual en los perfiles. "
             if hp else "Altavoces 5.1 (Logitech X-530). ") +
            "Tu base de volumen se mantiene en ambos modos. El cambio automático reacciona al "
            "enchufar o desenchufar.")
        self._upd = False

    def _vol_moved(self, sc):
        v = int(sc.get_value())
        sc.val.set_label(str(v))
        if not self._upd:
            sc.deb(sc.ctl, ",".join([str(v)] * sc.count))

    def _enum_row(self, parent, title, ctl):
        r = Gtk.Box(spacing=8)
        r.append(lbl(title, hexpand=True))
        dd = Gtk.DropDown.new_from_strings(["—"])
        dd.ctl_name, dd.items = ctl, []
        dd.connect("notify::selected", self._enum_changed)
        r.append(dd)
        parent.append(r)
        return dd

    def _switch_row(self, parent, title, ctl, sub):
        r = Gtk.Box(spacing=8)
        col = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, hexpand=True)
        col.append(lbl(title))
        if sub:
            col.append(lbl(sub, ["dim", "small"], wrap=True))
        r.append(col)
        sw = Gtk.Switch(valign=Gtk.Align.CENTER)
        sw.ctl_name = ctl
        sw.connect("notify::active", self._switch_changed)
        r.append(sw)
        parent.append(r)
        return sw

    def update(self, state):
        m = dsp_map(state)
        self._upd = True
        for dd in (self.out, self.cfg):
            c = m.get(dd.ctl_name)
            dd.set_sensitive(c is not None)
            if c:
                if c["items"] != dd.items:
                    dd.items = c["items"]
                    dd.set_model(Gtk.StringList.new(c["items"]))
                dd.set_selected(ctl_enum(c))
        for sw in (self.frf, self.frr, self.br):
            c = m.get(sw.ctl_name)
            sw.set_sensitive(c is not None)
            if c:
                sw.set_active(ctl_on(c))
        c = m.get("Bass Redirection Crossover")
        self.xo.set_sensitive(c is not None)
        if c:
            self.xo.set_range(1, max(60, ctl_int(c)))
            self.xo.set_value(ctl_int(c))
            self.xo_lbl.set_label(f"{ctl_int(c) * 10} Hz")
        for ctl, sc in self.vol.items():
            c = m.get(ctl)
            sc.set_sensitive(c is not None)
            if c:
                sc.set_range(c["min"], c["max"])
                sc.set_value(ctl_int(c))
                sc.val.set_label(str(ctl_int(c)))
        self._upd = False

    def _enum_changed(self, dd, _p):
        if not self._upd and dd.items:
            be.run_async(be.dsp_set, lambda *_: None, dd.ctl_name, dd.items[dd.get_selected()])

    def _switch_changed(self, sw, _p):
        if not self._upd:
            be.run_async(be.dsp_set, lambda *_: None, sw.ctl_name, "on" if sw.get_active() else "off")

    def _xo(self, s):
        v = int(s.get_value())
        self.xo_lbl.set_label(f"{v * 10} Hz")
        if not self._upd:
            self._xdeb("Bass Redirection Crossover", v)

    def _test(self, ch):
        be.channel_tone(self.win.default_sink, ch)

    def _test_all(self, *_):
        for i in range(6):
            GLib.timeout_add(i * 1500, lambda ch=i: (self.map.flash(ch), self._test(ch), False)[2])


# --- Mezclador -----------------------------------------------------------------

class MixerPage(Gtk.Box):
    def __init__(self, win):
        super().__init__(orientation=Gtk.Orientation.VERTICAL)
        self.win = win
        box = page_box()
        box.append(page_header("Mezclador", "Dispositivos y volumen por aplicación."))
        self.apps = panel("Aplicaciones")
        self.apps_box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=8)
        self.apps.append(self.apps_box)
        box.append(self.apps)
        self.outs = panel("Salidas")
        self.outs_box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=8)
        self.outs.append(self.outs_box)
        box.append(self.outs)
        self.ins = panel("Entradas")
        self.ins_box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=8)
        self.ins.append(self.ins_box)
        box.append(self.ins)
        self.append(scrolled(box))
        self._sig = None

    def _row(self, title, sub, vol, mute, on_vol, on_mute, default=None, on_default=None):
        r = Gtk.Box(spacing=10, css_classes=["check-row"])
        col = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, hexpand=False, width_request=260)
        col.append(lbl(title, ellipsize=Pango.EllipsizeMode.END, max_width_chars=34))
        if sub:
            col.append(lbl(sub, ["dim", "small"], ellipsize=Pango.EllipsizeMode.END, max_width_chars=40))
        r.append(col)
        sc = Gtk.Scale.new_with_range(Gtk.Orientation.HORIZONTAL, 0, 150, 1)
        sc.set_hexpand(True)
        sc.set_draw_value(False)
        sc.add_css_class("accent")
        sc.add_mark(100, Gtk.PositionType.BOTTOM, None)
        sc.set_value(vol)
        val = lbl(f"{vol:.0f}%", ["mono"], width_chars=5, xalign=1)
        deb = Debounce(80, on_vol)
        sc.connect("value-changed", lambda s: (val.set_label(f"{s.get_value():.0f}%"), deb(int(s.get_value()))))
        r.append(sc)
        r.append(val)
        mb = Gtk.ToggleButton(icon_name="audio-volume-muted-symbolic", active=mute, tooltip_text="Silenciar")
        mb.connect("toggled", lambda b: be.run_async(on_mute, lambda *_: None, b.get_active()))
        r.append(mb)
        if on_default:
            db = Gtk.Button(label="Por defecto" if not default else "✓ Por defecto", sensitive=not default)
            db.connect("clicked", lambda *_: be.run_async(on_default, lambda *_: self.win.refresh()))
            r.append(db)
        return r

    def update(self, st):
        audio = st.get("health", {}).get("audio", {})
        sig = (tuple((s["name"], s["default"], s["mute"]) for s in audio.get("sinks", [])),
               tuple((s["name"], s["default"], s["mute"]) for s in audio.get("sources", [])))
        if sig != self._sig:
            self._sig = sig
            clear(self.outs_box)
            for s in audio.get("sinks", []):
                n = s["name"]
                self.outs_box.append(self._row(
                    short_dev(s["description"]), n, max(0, s["volume_percent"]), s["mute"],
                    lambda v, n=n: be.set_volume("sink", n, v), lambda m, n=n: be.set_mute("sink", n, m),
                    s["default"], lambda n=n: be.set_default("sink", n)))
            clear(self.ins_box)
            for s in audio.get("sources", []):
                n = s["name"]
                self.ins_box.append(self._row(
                    short_dev(s["description"]), n, max(0, s["volume_percent"]), s["mute"],
                    lambda v, n=n: be.set_volume("source", n, v), lambda m, n=n: be.set_mute("source", n, m),
                    s["default"], lambda n=n: be.set_default("source", n)))
        be.run_async(be.sink_inputs, self._apps)

    def _apps(self, inputs, err):
        clear(self.apps_box)
        shown = 0
        for i in inputs or []:
            props = i.get("properties", {})
            media = props.get("media.name", "")
            if "Atlas" in media:
                continue
            vols = [v.get("value", 0) for v in i.get("volume", {}).values()]
            pct = (sum(vols) / len(vols) * 100 / 65536) if vols else 100
            idx = i["index"]
            title = props.get("application.name", "?")
            self.apps_box.append(self._row(title, media, pct, i.get("mute", False),
                                           lambda v, idx=idx: be.set_input_volume(idx, v),
                                           lambda m, idx=idx: be.set_input_mute(idx, m)))
            shown += 1
        if not shown:
            self.apps_box.append(lbl("Ninguna aplicación está reproduciendo.", ["dim", "small"]))


# --- Recuperación ----------------------------------------------------------------

class RecoverPage(Gtk.Box):
    def __init__(self, win):
        super().__init__(orientation=Gtk.Orientation.VERTICAL)
        self.win = win
        self._upd = False
        self.running = False
        box = page_box()
        box.append(page_header("Recuperación", "Diagnóstico funcional y recuperación escalonada (BUG-001: audio "
                                               "perdido tras suspender)."))
        act = panel()
        row = Gtk.Box(spacing=8)
        self.btn_check = Gtk.Button(label="Comprobar", css_classes=["big-button"])
        self.btn_check.connect("clicked", lambda *_: self.run(["check"]))
        row.append(self.btn_check)
        self.btn_rec = Gtk.Button(label="Recuperar", css_classes=["big-button", "suggested-action"])
        self.btn_rec.connect("clicked", lambda *_: self.run(["recover"]))
        row.append(self.btn_rec)
        self.btn_force = Gtk.Button(label="Forzar (se oye mal)", css_classes=["big-button"])
        self.btn_force.set_tooltip_text("Ejecuta la escalera aunque la sonda diga que todo va bien")
        self.btn_force.connect("clicked", lambda *_: self.run(["recover", "--force"]))
        row.append(self.btn_force)
        tone = Gtk.Button(label="Tono de prueba", css_classes=["big-button"])
        tone.connect("clicked", lambda *_: self.win.test_tone())
        row.append(tone)
        row.append(Gtk.Box(hexpand=True))
        rep = Gtk.Button(label="Crear informe")
        rep.connect("clicked", self._report)
        row.append(rep)
        act.append(row)
        act.append(lbl("Escalera: 1 · reabrir la tarjeta → 2 · reiniciar pipewire-pulse → 3 · reiniciar PipeWire "
                       "completo y reparar el portal. Tras cada paso se verifica en hardware y solo se sube si "
                       "sigue fallando.", ["dim", "small"], wrap=True))
        self.log = Gtk.TextView(editable=False, monospace=True, cursor_visible=False, wrap_mode=Gtk.WrapMode.WORD_CHAR,
                                top_margin=10, bottom_margin=10, left_margin=12, right_margin=12)
        sw = Gtk.ScrolledWindow(min_content_height=220, css_classes=["log"])
        sw.set_child(self.log)
        act.append(sw)
        box.append(act)

        prot = panel("Protección al suspender")
        self.sw_watch = self._switch(prot, "Daemon de Atlas (audio-profile-watch)",
                                     "Vigila la suspensión y aplica los perfiles automáticos.", self._watch)
        self.sw_quiesce = self._switch(prot, "Cerrar la tarjeta antes de dormir",
                                       "Evita que el DMA quede atascado al reanudar (causa de BUG-001).",
                                       lambda on: be.conf_set("watch.quiesce_before_sleep", "yes" if on else "no"))
        self.sw_auto = self._switch(prot, "Verificar y recuperar al reanudar", "",
                                    lambda on: be.conf_set("watch.auto_recover", "yes" if on else "no"))
        self.sw_notify = self._switch(prot, "Avisarme si no se pudo recuperar", "",
                                      lambda on: be.conf_set("watch.notify", "yes" if on else "no"))
        box.append(prot)

        hist = panel("Historial de reanudaciones y recuperaciones")
        self.hist_sum = lbl("", ["dim"], wrap=True)
        hist.append(self.hist_sum)
        self.hist = Gtk.Box(orientation=Gtk.Orientation.VERTICAL)
        hist.append(self.hist)
        box.append(hist)
        self.append(scrolled(box))

    def _switch(self, parent, title, sub, fn):
        r = Gtk.Box(spacing=8, css_classes=["check-row"])
        col = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, hexpand=True)
        col.append(lbl(title))
        if sub:
            col.append(lbl(sub, ["dim", "small"], wrap=True))
        r.append(col)
        s = Gtk.Switch(valign=Gtk.Align.CENTER)
        s.connect("notify::active", lambda w, _p: None if self._upd else fn(w.get_active()))
        r.append(s)
        parent.append(r)
        return s

    def _watch(self, on):
        be.run_async(be.watch_enable, lambda r, e: (
            self.win.toast("Daemon activado" if on else "Daemon detenido") if r and r.returncode == 0
            else self.win.toast((r.stderr.strip() if r else str(e)) or "No se pudo cambiar el servicio"),
            self.win.refresh_watch()), on)

    def update_settings(self, w):
        self._upd = True
        self.sw_watch.set_active(bool(w.get("active")))
        self.sw_watch.set_sensitive(bool(w.get("loaded")))
        self.sw_quiesce.set_active(be.conf_get("watch.quiesce_before_sleep", "yes") == "yes")
        self.sw_auto.set_active(be.conf_get("watch.auto_recover", "yes") == "yes")
        self.sw_notify.set_active(be.conf_get("watch.notify", "yes") == "yes")
        self._upd = False

    def write(self, text, tag=None):
        buf = self.log.get_buffer()
        buf.insert(buf.get_end_iter(), text + "\n")
        mark = buf.create_mark(None, buf.get_end_iter(), False)
        self.log.scroll_mark_onscreen(mark)

    def run(self, args):
        if self.running:
            return
        self.running = True
        for b in (self.btn_check, self.btn_rec, self.btn_force):
            b.set_sensitive(False)
        self.log.get_buffer().set_text("")
        self.write(f"$ audio-profile {' '.join(args)}")
        if args[0] == "check":
            be.run_async(lambda: be.run("check", timeout=20), self._checked)
            return
        icons = {"ok": "✓", "fail": "✗", "warn": "⚠", "info": "·"}

        def line(ev):
            k = ev.get("event")
            if k == "level":
                self.write(f"\n== Nivel {ev.get('level')}: {ev.get('message')} ==")
            elif k == "result":
                self.write(f"\nResultado: {'recuperado' if ev.get('message') == 'recovered' else 'SIN RECUPERAR'}")
            else:
                self.write(f"  {icons.get(k, '·')} {ev.get('message')}")

        be.stream_lines(args, line, self._done)

    def _checked(self, r, err):
        if err:
            self.write(f"✗ {err}")
        else:
            for k, name in (("services", "Servicios"), ("connection", "Conexión"), ("hardware", "Hardware"),
                            ("acoustic", "Acústica")):
                d = r["levels"][k]
                ic = {"ok": "✓", "warn": "⚠", "fail": "✗"}.get(d["level"], "?")
                self.write(f"  {ic} {name:<10} {d['detail']}")
        self._done(0)

    def _done(self, rc):
        self.running = False
        for b in (self.btn_check, self.btn_rec, self.btn_force):
            b.set_sensitive(True)
        self.win.refresh()
        self.load_history()

    def _report(self, *_):
        be.run_async(be.report, lambda r, e: self.win.toast(f"Informe: {r['report']}" if r else str(e)))

    def load_history(self):
        be.run_async(be.history, self._hist)

    def _hist(self, items, err):
        clear(self.hist)
        items = list(reversed(items or []))
        resumes = [i for i in items if i.get("trigger") == "resume"]
        clean = sum(1 for i in resumes if i.get("final_ok") and not i.get("steps"))
        self.hist_sum.set_label(
            f"{len(resumes)} reanudación(es) registradas · {clean} sanas sin necesitar acción · "
            f"{sum(1 for i in resumes if i.get('final_ok'))} con audio al final. BUG-001 se considera cerrado tras "
            "varios ciclos seguidos sanos sin acción.")
        for it in items[:30]:
            r = Gtk.Box(spacing=10, css_classes=["check-row"])
            ok = it.get("final_ok")
            r.append(icon("atlas-ok-symbolic" if ok else "dialog-error-symbolic", 16,
                          ["icon-ok" if ok else "icon-fail"]))
            r.append(lbl(it.get("time", "")[:19].replace("T", " "), ["mono", "small"]))
            r.append(pill({"resume": "reanudación", "manual": "manual", "gui": "manual"}.get(it.get("trigger"),
                                                                                           it.get("trigger", "")),
                          "accent" if it.get("trigger") == "resume" else ""))
            steps = it.get("steps") or []
            if it.get("dry_run"):
                desc = "simulación"
            elif not steps:
                desc = "sano, sin acciones"
            else:
                desc = " → ".join(f"{s['action']}{'✓' if s['ok'] else '✗'}" for s in steps)
            r.append(lbl(desc, ["mono", "small"], hexpand=True, ellipsize=Pango.EllipsizeMode.END))
            r.append(lbl(it.get("initial_probe", ""), ["dim", "small"]))
            self.hist.append(r)
        if not items:
            self.hist.append(lbl("Sin historial todavía. Cada reanudación quedará registrada aquí.", ["dim"]))


# --- Hardware ---------------------------------------------------------------------

class HardwarePage(Gtk.Box):
    MATCH: ClassVar[dict] = {"unique": ("registrada", "ok"), "none": ("no registrada", "unknown"),
             "partial": ("posible coincidencia", "warn"), "ambiguous": ("ambigua", "fail")}

    def __init__(self, win):
        super().__init__(orientation=Gtk.Orientation.VERTICAL)
        self.win = win
        box = page_box()
        hdr = Gtk.Box(spacing=12)
        hdr.append(page_header("Hardware", "Tarjetas detectadas y su ficha en la Hardware DB. La identidad es "
                                           "estable (PCI/USB + códec), no los números temporales de PipeWire."))
        hdr.get_first_child().set_hexpand(True)
        val = Gtk.Button(label="Validar base de datos", valign=Gtk.Align.CENTER)
        val.connect("clicked", self._validate)
        hdr.append(val)
        box.append(hdr)
        self.list = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=12)
        box.append(self.list)
        self.append(scrolled(box))

    def load(self):
        be.run_async(be.devices, self._loaded)

    def _loaded(self, devs, err):
        clear(self.list)
        if err:
            self.list.append(lbl(str(err), ["warnbox"]))
            return
        for d in devs:
            hw = d["hardware"]
            p = panel()
            top = Gtk.Box(spacing=10)
            top.append(icon("audio-card-symbolic", 24, ["profile-icon"]))
            col = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, hexpand=True)
            col.append(lbl(short_dev(d.get("description") or hw["longname"]), ["section-title"],
                           ellipsize=Pango.EllipsizeMode.END))
            col.append(lbl(f"{hw['stable_key']}  ·  {hw['codec'] or hw['longname']}", ["mono", "small", "dim"]))
            top.append(col)
            txt, kind = self.MATCH.get(d["match"], (d["match"], ""))
            top.append(pill(f"{d['db_id']} · {txt}" if d["db_id"] else txt, kind))
            p.append(top)
            info = Gtk.Box(spacing=8)
            info.append(pill(f"ALSA {hw['alsa_card']}"))
            info.append(pill(hw["driver"] or "?"))
            if d.get("active_profile"):
                info.append(pill(d["active_profile"] if d["active_profile"] != "off" else "apagada"))
            caps = d.get("capabilities", {})
            if caps.get("max_output_channels"):
                info.append(pill(f"{caps['max_output_channels']} canales"))
            if caps.get("capture"):
                info.append(pill("entrada"))
            if caps.get("spdif"):
                info.append(pill("S/PDIF"))
            if caps.get("hdmi"):
                info.append(pill("HDMI"))
            info.append(Gtk.Box(hexpand=True))
            card = hw["alsa_card"]
            if d["match"] == "unique":
                b = Gtk.Button(label="Ver ficha")
                b.connect("clicked", lambda *_, i=d["db_id"]: self._info(i))
                info.append(b)
            elif d["match"] == "partial":
                b = Gtk.Button(label=f"Es la misma que {d['db_id']}")
                b.connect("clicked", lambda *_, c=card, i=d["db_id"]: self._register(c, ["--link", i]))
                info.append(b)
                b2 = Gtk.Button(label="Registrar como nueva")
                b2.connect("clicked", lambda *_, c=card: self._register(c, ["--new"]))
                info.append(b2)
            elif d["match"] == "none":
                b = Gtk.Button(label="Registrar", css_classes=["suggested-action"])
                b.connect("clicked", lambda *_, c=card: self._register(c, []))
                info.append(b)
            p.append(info)
            if hw.get("missing"):
                p.append(lbl(f"No se pudieron leer: {hw['missing']}. Obténlos con: "
                             f"udevadm info -a /sys/class/sound/card{card}", ["warnbox"], wrap=True))
            self.list.append(p)

    def _register(self, card, extra):
        be.run_async(be.hardware_register, lambda r, e: (self.win.toast(r[1] if r else str(e)), self.load()),
                     card, extra)

    def _info(self, hw_id):
        def shown(info, err):
            if err:
                self.win.toast(str(err))
                return
            body = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=8)
            body.append(lbl(f"{info['name']} · {info.get('chip', '')}", ["section-title"]))
            if info.get("speakers"):
                body.append(lbl(f"Altavoces: {info['speakers']}", ["dim"]))
            if info.get("quirks"):
                body.append(lbl(f"Quirks: {info['quirks']}", ["mono", "small"]))
            if info.get("dsp_presets"):
                body.append(lbl(f"Presets DSP: {info['dsp_presets']}", ["small"], wrap=True))
            kb = info.get("known_bugs") or {}
            for b in kb.get("known_bugs", []):
                card = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=4, css_classes=["tile"])
                h = Gtk.Box(spacing=8)
                h.append(lbl(b.get("id", ""), ["mono"], hexpand=True))
                sev = b.get("severity", "")
                h.append(pill(sev, "fail" if sev == "high" else ("warn" if sev in ("important", "warning") else "")))
                card.append(h)
                card.append(lbl(b.get("description", ""), wrap=True))
                card.append(lbl("→ " + b.get("workaround", ""), ["dim", "small"], wrap=True))
                body.append(card)
            for n in kb.get("notes", []):
                body.append(lbl("· " + n, ["dim", "small"], wrap=True))
            sw = Gtk.ScrolledWindow(min_content_height=360, min_content_width=560)
            sw.set_child(body)
            dlg = Adw.AlertDialog(heading=f"Hardware DB · {hw_id}")
            dlg.set_extra_child(sw)
            dlg.add_response("close", "Cerrar")
            dlg.present(self.win)

        be.run_async(be.hardware_info, shown, hw_id)

    def _validate(self, *_):
        def done(r, err):
            if err:
                self.win.toast(str(err))
                return
            if not r["errors"] and not r["warnings"]:
                self.win.toast(f"Hardware DB correcta ({r['entries']} entradas)")
                return
            body = "\n".join(f"{'✗' if i['level'] == 'error' else '⚠'} {i['where']}: {i['message']}"
                             for i in r["issues"])
            dlg = Adw.AlertDialog(heading="Validación de la Hardware DB", body=body)
            dlg.add_response("close", "Cerrar")
            dlg.present(self.win)

        be.run_async(be.hardware_validate, done)


# --- Diagnóstico ------------------------------------------------------------------

class DiagPage(Gtk.Box):
    SECTIONS: ClassVar[dict] = {"servicios": "Servicios", "audio": "Audio", "pipewire": "PipeWire",
                                "hardware": "Hardware",
                "kernel": "Kernel", "bug-001": "BUG-001 · suspensión", "perfiles": "Perfiles"}

    def __init__(self, win):
        super().__init__(orientation=Gtk.Orientation.VERTICAL)
        self.win = win
        box = page_box()
        hdr = Gtk.Box(spacing=12)
        hdr.append(page_header("Diagnóstico", "Comprobación completa con el comando exacto para cada arreglo."))
        hdr.get_first_child().set_hexpand(True)
        self.summary = pill("…")
        self.summary.set_valign(Gtk.Align.CENTER)
        hdr.append(self.summary)
        again = Gtk.Button(label="Ejecutar de nuevo", valign=Gtk.Align.CENTER)
        again.connect("clicked", lambda *_: self.load())
        hdr.append(again)
        box.append(hdr)
        self.body = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=12)
        box.append(self.body)
        self.append(scrolled(box))

    def load(self):
        clear(self.body)
        self.body.append(Adw.Spinner(halign=Gtk.Align.CENTER, width_request=32, height_request=32))
        be.run_async(be.doctor, self._loaded)

    def _loaded(self, r, err):
        clear(self.body)
        if err:
            self.body.append(lbl(str(err), ["warnbox"], wrap=True))
            return
        f, w = r["fails"], r["warnings"]
        self.summary.set_label(f"{f} fallo(s) · {w} aviso(s)")
        self.summary.set_css_classes(["pill", "fail" if f else ("warn" if w else "ok")])
        groups = {}
        for c in r["checks"]:
            groups.setdefault(c["section"], []).append(c)
        for sec, checks in groups.items():
            p = panel(self.SECTIONS.get(sec, sec), spacing=0)
            for c in checks:
                row = Gtk.Box(spacing=12, css_classes=["check-row"])
                row.append(icon(LEVEL_ICON.get(c["level"], LEVEL_ICON["unknown"]), 18, [f"icon-{c['level']}"]))
                col = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=4, hexpand=True)
                col.append(lbl(c["title"]))
                if c["detail"]:
                    col.append(lbl(c["detail"], ["dim", "small"], wrap=True, wrap_mode=Pango.WrapMode.WORD_CHAR))
                if c["fix"]:
                    fx = Gtk.Box(spacing=6)
                    fx.append(lbl(c["fix"], ["fix"], hexpand=True, selectable=True, wrap=True,
                                  wrap_mode=Pango.WrapMode.WORD_CHAR))
                    cp = Gtk.Button(icon_name="edit-copy-symbolic", valign=Gtk.Align.CENTER, tooltip_text="Copiar")
                    cp.connect("clicked", lambda *_, t=c["fix"]: self.win.copy(t))
                    fx.append(cp)
                    col.append(fx)
                row.append(col)
                p.append(row)
            self.body.append(p)


# --- Ventana ------------------------------------------------------------------------

class MainWindow(Adw.ApplicationWindow):
    PAGES = (("home", "Inicio", "go-home-symbolic"),
             ("profiles", "Perfiles", "view-grid-symbolic"),
             ("effects", "Efectos", "starred-symbolic"),
             ("eq", "Ecualizador", "atlas-eq-symbolic"),
             ("speakers", "Altavoces", "audio-speakers-symbolic"),
             ("mixer", "Mezclador", "multimedia-volume-control-symbolic"),
             ("recover", "Recuperación", "view-refresh-symbolic"),
             ("hardware", "Hardware", "audio-card-symbolic"),
             ("diag", "Diagnóstico", "dialog-information-symbolic"))
    GROUPS: ClassVar[dict] = {"home": "AUDIO", "recover": "SISTEMA"}

    def __init__(self, app):
        super().__init__(application=app, title="Atlas Audio", default_width=1280, default_height=880)
        self.add_css_class("atlas")
        self.status = {}
        self.auto = {}
        self.watch = {}
        self.profiles = []
        self.dsp = None
        self.default_sink = None
        self._busy = False

        self.toasts = Adw.ToastOverlay()
        view = Adw.ToolbarView()
        hb = Adw.HeaderBar()
        tbox = Gtk.Box(spacing=10)
        tbox.append(icon(APP_ID, 22))
        tbox.append(lbl("Atlas Audio", ["section-title"]))
        hb.set_title_widget(tbox)
        self.dev_lbl = lbl("", ["dim"], ellipsize=Pango.EllipsizeMode.END, max_width_chars=48)
        hb.pack_start(self.dev_lbl)
        self.health_pill = pill("…", "unknown")
        hb.pack_end(self.health_pill)
        self.prof_pill = pill("", "accent")
        hb.pack_end(self.prof_pill)
        self.out_pill = Gtk.Box(spacing=6, css_classes=["pill", "accent"], visible=False)
        self.out_pill.append(icon("audio-headphones-symbolic", 14))
        self.out_pill.append(lbl("auriculares"))
        hb.pack_end(self.out_pill)
        self.mode_sw = ModeSwitch(self.set_mode)
        hb.pack_end(self.mode_sw)
        self.output = {}
        view.add_top_bar(hb)
        self.banner = Adw.Banner(button_label="Recuperar ahora")
        self.banner.connect("button-clicked", lambda *_: self.go("recover", run=True))
        view.add_top_bar(self.banner)

        body = Gtk.Box()
        side = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, css_classes=["sidebar"])
        side.set_size_request(210, -1)
        self.side = Gtk.ListBox(css_classes=["navigation-sidebar"], vexpand=True)
        self.side.set_header_func(self._side_header)
        side.append(self.side)
        self.stack = Gtk.Stack(transition_type=Gtk.StackTransitionType.CROSSFADE, hexpand=True)
        self.pages = {
            "home": HomePage(self), "profiles": ProfilesPage(self), "effects": EffectsPage(self),
            "eq": EqPage(self), "speakers": SpeakersPage(self), "mixer": MixerPage(self),
            "recover": RecoverPage(self), "hardware": HardwarePage(self), "diag": DiagPage(self),
        }
        self.rows = {}
        for key, text, ic in self.PAGES:
            self.stack.add_named(self.pages[key], key)
            row = Gtk.ListBoxRow()
            rb = Gtk.Box(spacing=10)
            rb.append(icon(ic, 16))
            rb.append(lbl(text, hexpand=True))
            row.set_child(rb)
            row.key = key
            self.rows[key] = row
            self.side.append(row)
        self.side.connect("row-selected", self._selected)
        body.append(side)
        body.append(self.stack)
        view.set_content(body)
        self.statusbar = lbl("", ["statusbar", "mono"])
        view.add_bottom_bar(self.statusbar)
        self.toasts.set_child(view)
        self.set_content(self.toasts)

        self.side.select_row(self.rows.get(self.get_application().start_page, self.rows["home"]))
        self.load_profiles()
        self.refresh_auto()
        self.refresh_watch()
        self.refresh_output()
        self.refresh()
        self._timer = GLib.timeout_add(REFRESH_MS, self._tick)
        self.connect("close-request", self._close)
        self.connect("notify::is-active", lambda w, _p: w.refresh() if w.is_active() else None)

    def _side_header(self, row, before):
        g = self.GROUPS.get(row.key)
        row.set_header(lbl(g, ["side-title"]) if g else None)

    def _selected(self, lb, row):
        if not row:
            return
        self.stack.set_visible_child_name(row.key)
        if row.key in ("effects", "eq", "speakers"):
            self.load_dsp()
            if row.key == "speakers":
                self.refresh_output()
        elif row.key == "hardware":
            self.pages["hardware"].load()
        elif row.key == "diag":
            self.pages["diag"].load()
        elif row.key == "recover":
            self.pages["recover"].load_history()
            self.pages["recover"].update_settings(self.watch)
        elif row.key == "mixer":
            self.pages["mixer"].update(self.status)
        elif row.key == "profiles":
            self.refresh_auto()

    def go(self, key, run=False):
        self.side.select_row(self.rows[key])
        if run and key == "recover":
            self.pages["recover"].run(["recover"])

    # --- datos ---
    def profile_name(self, pid):
        p = next((x for x in self.profiles if x["id"] == pid), None)
        return p["name"] if p else (pid or "—")

    def load_profiles(self):
        def done(r, err):
            if err:
                self.toast(f"No pude leer los perfiles: {err}")
                return
            self.profiles = r
            self.pages["home"].set_profiles(r)
            self.pages["profiles"].set_profiles(r, self.status.get("profile"))
        be.run_async(be.profiles, done)

    def refresh(self, probe=False):
        if self._busy:
            return
        self._busy = True

        def done(r, err):
            self._busy = False
            if err:
                self.health_pill.set_label("sin CLI")
                self.health_pill.set_css_classes(["pill", "fail"])
                self.statusbar.set_label(str(err))
                return
            prev = self.status.get("profile")
            self.status = r
            h = r.get("health", {})
            overall = h.get("overall", "unknown")
            health_text = {"ok": "audio OK", "warn": "con avisos", "fail": "SIN AUDIO"}
            self.health_pill.set_label(health_text.get(overall, overall))
            self.health_pill.set_css_classes(["pill", overall])
            self.prof_pill.set_label(self.profile_name(r.get("profile")))
            sink = next((s for s in h.get("audio", {}).get("sinks", []) if s.get("default")), None)
            self.default_sink = sink["name"] if sink else None
            self.dev_lbl.set_label(short_dev(sink["description"]) if sink else "")
            self.banner.set_title("El audio no llega al hardware: " +
                                  h.get("levels", {}).get("hardware", {}).get("detail", "")[:120])
            self.banner.set_revealed(overall == "fail")
            self.pages["home"].update(r)
            if self.stack.get_visible_child_name() == "mixer":
                self.pages["mixer"].update(r)
            if prev != r.get("profile") and self.profiles:
                self.pages["profiles"].set_profiles(self.profiles, r.get("profile"))
            c = r.get("clock", {})
            self.statusbar.set_label(
                f"Atlas {__version__} · CLI {be.display_path(be.CLI)} · {c.get('rate', '?')} Hz / quantum "
                f"{c.get('force_quantum') or c.get('quantum', '?')} · {h.get('audio', {}).get('playback_streams', 0)} "
                f"stream(s)")
        be.run_async(be.status, done, probe)

    def refresh_auto(self):
        def done(r, err):
            if not err:
                self.auto = r
                self.pages["profiles"].set_auto(r)
            self.update_mode()
        be.run_async(be.auto_status, done)

    def update_mode(self):
        m = be.auto_mode()
        self.mode_sw.set_mode(m["mode"], self.profile_name(m["current"]))

    def set_mode(self, mode):
        def done(r, err):
            self.toast("Modo automático: Atlas elige el perfil según lo que suena" if mode == "auto"
                       else "Modo manual: los perfiles solo cambian cuando los eliges")
            self.refresh_auto()
        be.run_async(be.auto_set, done, mode == "auto")

    def refresh_output(self):
        def done(r, err):
            if not err:
                self.output = r
                self.pages["speakers"].update_output(r)
                self.out_pill.set_visible(r.get("mode") == "headphones")
        be.run_async(be.output_status, done)

    def refresh_watch(self):
        def done(r, err):
            if not err:
                self.watch = r
                self.pages["recover"].update_settings(r)
        be.run_async(be.watch_state, done)

    def load_dsp(self):
        def done(r, err):
            if err:
                self.toast(f"DSP no disponible: {err}")
                return
            state, presets = r
            self.dsp = state
            self.pages["effects"].update(state, presets)
            self.pages["eq"].update(state)
            self.pages["speakers"].update(state)
        be.run_async(lambda: (be.dsp_state(), be.dsp_presets()), done)

    def _tick(self):
        # sin foco (otro escritorio, minimizada): no consultar; se refresca al volver
        if not self.is_active():
            return True
        self.update_mode()   # lectura de archivo, sin lanzar la CLI
        self.refresh()
        if self.stack.get_visible_child_name() == "profiles":
            self.refresh_auto()
        return True

    # --- acciones ---
    def apply_profile(self, pid):
        self.toast(f"Aplicando «{self.profile_name(pid)}»…")

        def done(r, err):
            rc, out = r if r else (1, str(err))
            paused = rc == 0 and be.auto_mode()["mode"] == "auto-paused"
            self.toast(f"Perfil «{self.profile_name(pid)}» aplicado"
                       + (" · el automático vuelve cuando cambie lo que suena" if paused else "")
                       if rc == 0 else f"Error: {out[-200:]}")
            self.refresh()
            self.refresh_auto()
            if self.stack.get_visible_child_name() in ("effects", "eq", "speakers"):
                self.load_dsp()
        be.run_async(be.apply_profile, done, pid)

    def test_tone(self):
        self.toast("Tono de prueba…")
        be.run_async(be.test_tone, lambda r, e: None, self.default_sink)

    def copy(self, text):
        self.get_clipboard().set(text)
        self.toast("Copiado")

    def toast(self, text):
        self.toasts.add_toast(Adw.Toast(title=GLib.markup_escape_text(str(text)), timeout=4))

    def _close(self, *_):
        GLib.source_remove(self._timer)
        return False


class App(Adw.Application):
    def __init__(self, start_page="home"):
        super().__init__(application_id=APP_ID, flags=Gio.ApplicationFlags.DEFAULT_FLAGS)
        self.start_page = start_page

    def do_startup(self):
        Adw.Application.do_startup(self)
        Adw.StyleManager.get_default().set_color_scheme(Adw.ColorScheme.FORCE_DARK)
        prov = Gtk.CssProvider()
        prov.load_from_string(CSS + (CSS_ACCENT_VARS if Gtk.get_minor_version() >= 16 else CSS_ACCENT_OLD))
        disp = Gdk.Display.get_default()
        Gtk.StyleContext.add_provider_for_display(disp, prov, Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION)
        # iconos simbólicos coherentes aunque el tema del sistema sea a color (Breeze, Papirus...)
        Gtk.Settings.get_default().set_property("gtk-icon-theme-name", "Adwaita")
        theme = Gtk.IconTheme.get_for_display(disp)
        base = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
        for d in (os.path.join(base, "data"), os.path.join(base, "data", "icons"), os.path.join(base, "icons")):
            if os.path.isdir(d):
                theme.add_search_path(d)

    def do_activate(self):
        win = self.props.active_window or MainWindow(self)
        win.present()


def main(argv=None) -> int:
    argv = list(argv or sys.argv)
    start_page = "home"
    if "--page" in argv:
        i = argv.index("--page")
        if i + 1 < len(argv):
            start_page = argv[i + 1]
            del argv[i:i + 2]
    if not os.environ.get("WAYLAND_DISPLAY"):
        print("Atlas Audio requiere una sesión Wayland (WAYLAND_DISPLAY no está definido).", file=sys.stderr)
        return 1
    return App(start_page).run(argv)
