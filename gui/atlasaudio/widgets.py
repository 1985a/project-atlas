"""Widgets dibujados con cairo: curva de ecualizador, mapa 5.1 y medidor."""
from __future__ import annotations

import math
from typing import ClassVar

import cairo
import gi

gi.require_version("Gtk", "4.0")
gi.require_version("PangoCairo", "1.0")
from gi.repository import Gtk, Pango, PangoCairo  # noqa: E402  (tras require_version)


def rgb(hexstr: str, a: float = 1.0):
    h = hexstr.lstrip("#")
    return (int(h[0:2], 16) / 255, int(h[2:4], 16) / 255, int(h[4:6], 16) / 255, a)


ACCENT = "#ff4d6d"
ACCENT2 = "#ff8fa3"
GREEN = "#3ddc97"
AMBER = "#ffc77a"
BLUE = "#6aa8ff"
VIOLET = "#a78bfa"
GRID = rgb("#252b34")
MUTED = rgb("#8b95a3")
TEXT = rgb("#e8ecf1")
PANEL = rgb("#161a20")


def draw_text(cr, x, y, text, size=8.5, color=MUTED, align="left", valign="center", bold=False):
    layout = PangoCairo.create_layout(cr)
    fd = Pango.FontDescription.from_string(f"JetBrains Mono,DejaVu Sans Mono,monospace {size}")
    if bold:
        fd.set_weight(Pango.Weight.SEMIBOLD)
    layout.set_font_description(fd)
    layout.set_text(text, -1)
    w, h = layout.get_pixel_size()
    if align == "right":
        x -= w
    elif align == "center":
        x -= w / 2
    if valign == "center":
        y -= h / 2
    elif valign == "bottom":
        y -= h
    cr.set_source_rgba(*color)
    cr.move_to(x, y)
    PangoCairo.show_layout(cr, layout)


class EqCurve(Gtk.DrawingArea):
    """Respuesta aproximada del ecualizador de 10 bandas (escala log)."""

    BANDS = (31, 62, 125, 250, 500, 1000, 2000, 4000, 8000, 16000)

    def __init__(self, height=150):
        super().__init__(content_height=height, hexpand=True)
        self.gains = [0.0] * 10
        self.enabled = True
        self.set_draw_func(self._draw)

    def set_gains(self, gains, enabled=True):
        self.gains = list(gains)
        self.enabled = enabled
        self.queue_draw()

    def _response(self, f):
        # suma de campanas gaussianas en octavas (aproximación visual)
        total = 0.0
        for fc, g in zip(self.BANDS, self.gains, strict=False):
            d = math.log2(f / fc)
            total += g * math.exp(-(d * d) / (2 * 0.42 ** 2))
        return total

    def _draw(self, area, cr, w, h):
        left, right, top, bottom = 40, 10, 10, 22
        pw, ph = w - left - right, h - top - bottom
        rng = 12.0
        lo, hi = math.log10(20), math.log10(20000)

        def X(f):
            return left + (math.log10(f) - lo) / (hi - lo) * pw

        def Y(db):
            return top + ph / 2 - max(-rng, min(rng, db)) / rng * ph / 2

        cr.set_line_width(1)
        for db in (-12, -6, 0, 6, 12):
            cr.set_source_rgba(*(rgb("#3a4250") if db == 0 else GRID))
            cr.move_to(left, Y(db) + 0.5)
            cr.line_to(w - right, Y(db) + 0.5)
            cr.stroke()
            draw_text(cr, left - 6, Y(db), f"{db:+d}", align="right", size=7.5)
        for f in self.BANDS:
            cr.set_source_rgba(*GRID)
            cr.move_to(X(f) + 0.5, top)
            cr.line_to(X(f) + 0.5, top + ph)
            cr.stroke()
            draw_text(cr, X(f), h - 10, f"{f // 1000}k" if f >= 1000 else str(f), align="center", size=7.5)

        color = rgb(ACCENT) if self.enabled else MUTED
        pts = [(X(f), Y(self._response(f))) for f in (20 * (1000 ** (i / 200)) for i in range(201))]
        grad = cairo.LinearGradient(0, top, 0, top + ph)
        grad.add_color_stop_rgba(0, *rgb(ACCENT, 0.28 if self.enabled else 0.08)[:3], 0.28 if self.enabled else 0.08)
        grad.add_color_stop_rgba(1, *rgb(ACCENT, 0.0)[:3], 0.0)
        cr.move_to(pts[0][0], Y(0))
        for x, y in pts:
            cr.line_to(x, y)
        cr.line_to(pts[-1][0], Y(0))
        cr.close_path()
        cr.set_source(grad)
        cr.fill()
        cr.set_line_width(2.2)
        cr.set_line_join(cairo.LINE_JOIN_ROUND)
        cr.move_to(*pts[0])
        for p in pts[1:]:
            cr.line_to(*p)
        cr.set_source_rgba(*color)
        cr.stroke()
        for f, g in zip(self.BANDS, self.gains, strict=False):
            cr.arc(X(f), Y(g), 3.5, 0, 2 * math.pi)
            cr.set_source_rgba(*color)
            cr.fill()


class SpeakerMap(Gtk.DrawingArea):
    """Mapa 5.1 visto desde arriba. Clic en un altavoz -> callback(canal)."""

    # posición relativa (x, y) y etiqueta; índice = canal FL FR RL RR FC LFE
    POS = ((0.22, 0.22, "FL"), (0.78, 0.22, "FR"), (0.16, 0.80, "RL"), (0.84, 0.80, "RR"),
           (0.50, 0.12, "C"), (0.70, 0.50, "SUB"))

    def __init__(self, on_click):
        super().__init__(content_height=300, hexpand=True)
        self.on_click = on_click
        self.active = -1
        self.hover = -1
        self.set_draw_func(self._draw)
        click = Gtk.GestureClick()
        click.connect("pressed", self._pressed)
        self.add_controller(click)
        motion = Gtk.EventControllerMotion()
        motion.connect("motion", self._motion)
        motion.connect("leave", lambda *_: self._set_hover(-1))
        self.add_controller(motion)
        self.set_cursor_from_name("pointer")

    def flash(self, ch):
        from gi.repository import GLib
        self.active = ch
        self.queue_draw()
        GLib.timeout_add(1200, self._unflash)

    def _unflash(self):
        self.active = -1
        self.queue_draw()
        return False

    def _hit(self, x, y):
        w, h = self.get_width(), self.get_height()
        for i, (px, py, _) in enumerate(self.POS):
            if math.hypot(x - px * w, y - py * h) < 30:
                return i
        return -1

    def _set_hover(self, i):
        if i != self.hover:
            self.hover = i
            self.queue_draw()

    def _motion(self, ctl, x, y):
        self._set_hover(self._hit(x, y))

    def _pressed(self, gesture, n, x, y):
        i = self._hit(x, y)
        if i >= 0:
            self.flash(i)
            self.on_click(i)

    def _draw(self, area, cr, w, h):
        cx, cy = w / 2, h * 0.55
        # sala
        cr.set_source_rgba(*GRID)
        cr.set_line_width(1)
        for r in (0.18, 0.32, 0.46):
            cr.arc(cx, cy, min(w, h) * r, 0, 2 * math.pi)
            cr.stroke()
        # oyente
        cr.set_source_rgba(*rgb("#2c333e"))
        cr.arc(cx, cy, 16, 0, 2 * math.pi)
        cr.fill()
        draw_text(cr, cx, cy, "tú", size=8, color=TEXT, align="center")
        for i, (px, py, label) in enumerate(self.POS):
            x, y = px * w, py * h
            on = i == self.active
            hov = i == self.hover
            if on:
                for k in range(3):
                    cr.set_source_rgba(*rgb(ACCENT, 0.25 - k * 0.07))
                    cr.arc(x, y, 30 + k * 10, 0, 2 * math.pi)
                    cr.fill()
            cr.set_source_rgba(*(rgb(ACCENT) if on else rgb("#2a313b" if hov else "#1f252e")))
            if label == "SUB":
                cr.rectangle(x - 24, y - 24, 48, 48)
            else:
                cr.arc(x, y, 22, 0, 2 * math.pi)
            cr.fill()
            cr.set_source_rgba(*(rgb(ACCENT2) if hov or on else rgb("#3a4250")))
            cr.set_line_width(1.5)
            if label == "SUB":
                cr.rectangle(x - 24, y - 24, 48, 48)
            else:
                cr.arc(x, y, 22, 0, 2 * math.pi)
            cr.stroke()
            draw_text(cr, x, y, label, size=9, color=TEXT, align="center", bold=True)


class Meter(Gtk.DrawingArea):
    """Barra de estado de cuatro niveles (servicios/conexión/hardware/acústica)."""

    COLORS: ClassVar[dict] = {"ok": GREEN, "warn": AMBER, "fail": "#ff6b6b", "unknown": "#5b6472"}

    def __init__(self):
        super().__init__(content_height=10, hexpand=True)
        self.levels = ["unknown"] * 4
        self.set_draw_func(self._draw)

    def set_levels(self, levels):
        self.levels = levels
        self.queue_draw()

    def _draw(self, area, cr, w, h):
        n = len(self.levels)
        gap = 6
        seg = (w - gap * (n - 1)) / n
        for i, lv in enumerate(self.levels):
            cr.set_source_rgba(*rgb(self.COLORS.get(lv, "#5b6472")))
            x = i * (seg + gap)
            r = h / 2
            cr.new_sub_path()
            cr.arc(x + r, r, r, math.pi / 2, 3 * math.pi / 2)
            cr.arc(x + seg - r, r, r, -math.pi / 2, math.pi / 2)
            cr.close_path()
            cr.fill()
