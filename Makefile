CC ?= gcc

PKGS = libpipewire-0.3 wireplumber-0.5 json-glib-1.0 gio-2.0 gio-unix-2.0 alsa

# CFLAGS, CPPFLAGS, LDFLAGS y LDLIBS quedan para el usuario (`make CFLAGS=-O3` no
# pierde los includes ni las librerías); lo imprescindible va en las variables ATLAS_*.
CFLAGS ?= -O2
ATLAS_CPPFLAGS = -Iinclude -Iinclude/common $(shell pkg-config --cflags $(PKGS))
ATLAS_CFLAGS   = -std=gnu17 -Wall -Wextra -MMD -MP
ATLAS_LDLIBS   = $(shell pkg-config --libs $(PKGS)) -lm

# Instalación: por defecto en el usuario (no necesita root ni espacio en /).
#   make install                      -> ~/.local
#   sudo make install PREFIX=/usr     -> sistema
PREFIX ?= $(HOME)/.local
DESTDIR ?=
BINDIR   = $(DESTDIR)$(PREFIX)/bin
SHAREDIR = $(DESTDIR)$(PREFIX)/share/audio-profile
LIBDIR   = $(DESTDIR)$(PREFIX)/lib/audio-profile
APPSDIR  = $(DESTDIR)$(PREFIX)/share/applications
ICONDIR  = $(DESTDIR)$(PREFIX)/share/icons/hicolor/scalable/apps
ifeq ($(PREFIX),$(HOME)/.local)
UNITDIR  = $(DESTDIR)$(HOME)/.config/systemd/user
else
UNITDIR  = $(DESTDIR)$(PREFIX)/lib/systemd/user
endif

SRC = \
	src/main.c \
	src/atlas_util.c src/atlas_audio.c src/atlas_probe.c src/atlas_health.c \
	src/atlas_doctor.c src/atlas_recover.c src/atlas_hwdb.c src/atlas_dsp.c \
	src/atlas_auto.c src/atlas_watch.c src/atlas_base.c src/atlas_steam.c src/atlas_output.c \
	src/profile.c src/profile_apply.c \
	src/pipewire_cmd.c \
	src/status.c \
	src/latency.c \
	src/monitor.c \
	src/backup.c \
	src/ppm_pipewire.c \
	src/recover.c \
	src/ppm.c \
	src/ppm_context.c \
	src/ppm_object.c \
	src/ppm_node.c \
	src/ppm_port.c \
	src/ppm_link.c \
	src/ppm_graph.c \
	src/ppm_control.c \
	src/common/ppm_lookup.c \
	src/ppm_metadata.c \
	src/ppm_default.c \
	src/ppm_volume.c \
	src/ppm_profile_state.c \
	src/ppm_alsa_preset.c \
	src/ppm_hardware_db.c \
	src/core/ppm_models.c src/core/ppm_runtime.c src/core/ppm_backend.c src/core/ppm_pipewire_backend.c \
	src/ppm_wp_core.c \
	src/ppm_wp_objects.c \
	src/common/ppm_exec.c src/common/ppm_memory.c src/common/ppm_vector.c src/common/ppm_string.c \
	src/ppm_registry.c \
	src/ppm_device.c

OBJ = $(SRC:.c=.o)
DEP = $(OBJ:.o=.d)

.DEFAULT_GOAL := all
.PHONY: all clean test test-asan lint install install-gui uninstall run-gui print-src

print-src:
	@echo $(SRC)

all: audio-profile

audio-profile: $(OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJ) $(ATLAS_LDLIBS) $(LDLIBS)

# Cada objeto depende solo de las cabeceras que incluye (archivos .d de -MMD).
%.o: %.c
	$(CC) $(ATLAS_CPPFLAGS) $(CPPFLAGS) $(ATLAS_CFLAGS) $(CFLAGS) -c -o $@ $<

-include $(DEP)

test: audio-profile
	./tests/run-tests.sh

# Pruebas con AddressSanitizer + UndefinedBehaviorSanitizer + LeakSanitizer:
# detectan fugas, accesos fuera de rango y comportamiento indefinido.
SANITIZE = -fsanitize=address,undefined -fno-omit-frame-pointer -g
test-asan:
	TEST_CC="$(CC) $(SANITIZE)" ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 \
		./tests/run-tests.sh
	$(MAKE) -C tests clean >/dev/null

# Análisis estático de C (gcc -fanalyzer), Python (ruff) y shell (shellcheck).
lint:
	./scripts/lint.sh

install: audio-profile
	install -Dm755 audio-profile $(BINDIR)/audio-profile
	install -d $(SHAREDIR)
	cp -a profiles hardware presets rules $(SHAREDIR)/
	install -Dm644 data/sway/atlas-audio.conf $(SHAREDIR)/sway/atlas-audio.conf
	sed -i 's|@BINDIR@|$(PREFIX)/bin|g' $(SHAREDIR)/sway/atlas-audio.conf
	install -Dm644 data/audio-profile-watch.service $(UNITDIR)/audio-profile-watch.service
	sed -i 's|@BINDIR@|$(PREFIX)/bin|' $(UNITDIR)/audio-profile-watch.service
	$(MAKE) install-gui

install-gui:
	install -d $(LIBDIR)/atlasaudio
	install -Dm644 -t $(LIBDIR)/atlasaudio gui/atlasaudio/*.py
	install -Dm644 -t $(LIBDIR)/icons gui/data/icons/*.svg
	install -Dm644 gui/data/org.atlas.AudioControl.svg $(LIBDIR)/icons/org.atlas.AudioControl.svg
	install -Dm755 gui/bin/atlas-audio $(BINDIR)/atlas-audio
	install -Dm644 gui/data/org.atlas.AudioControl.svg $(ICONDIR)/org.atlas.AudioControl.svg
	install -Dm644 gui/data/org.atlas.AudioControl.desktop $(APPSDIR)/org.atlas.AudioControl.desktop
	sed -i 's|@BINDIR@|$(PREFIX)/bin|' $(APPSDIR)/org.atlas.AudioControl.desktop

uninstall:
	rm -f $(BINDIR)/audio-profile $(BINDIR)/atlas-audio
	rm -rf $(SHAREDIR) $(LIBDIR)
	rm -f $(UNITDIR)/audio-profile-watch.service
	rm -f $(APPSDIR)/org.atlas.AudioControl.desktop $(ICONDIR)/org.atlas.AudioControl.svg

run-gui: audio-profile
	ATLAS_CLI=./audio-profile ./gui/bin/atlas-audio

clean:
	rm -f $(OBJ) $(DEP) audio-profile
