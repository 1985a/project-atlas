CC=gcc

PKGS = libpipewire-0.3 wireplumber-0.5 json-glib-1.0 gio-2.0 gio-unix-2.0 alsa
PKG_CFLAGS := $(shell pkg-config --cflags $(PKGS))
PKG_LIBS := $(shell pkg-config --libs $(PKGS)) -lm

CFLAGS=\
-Wall \
-Wextra \
-std=gnu17 -Wno-format-truncation \
-I../include \
-I../include/common \
$(PKG_CFLAGS)

MEMORY=../src/common/ppm_memory.c
VECTOR=../src/common/ppm_vector.c
STRING=../src/common/ppm_string.c
RUNTIME=../src/core/ppm_runtime.c
BACKEND=../src/core/ppm_backend.c
PROFILE=../src/profile.c ../src/atlas_util.c
PROFILE_APPLY=../src/profile_apply.c

# Núcleo completo (todo salvo main.c) para las pruebas de Atlas v0.2
CORE=$(filter-out ../src/main.c,$(addprefix ../,$(shell $(MAKE) -s -C .. print-src)))

TESTS = test_vector test_string test_runtime test_backend test_profile test_profile_apply \
        test_probe test_hwdb test_auto test_base test_steam test_conf

all: $(TESTS)

test_vector:
	$(CC) $(CFLAGS) test_vector.c $(MEMORY) $(VECTOR) -o test_vector

test_string:
	$(CC) $(CFLAGS) test_string.c $(MEMORY) $(STRING) -o test_string

test_runtime:
	$(CC) $(CFLAGS) test_runtime.c $(MEMORY) $(RUNTIME) $(BACKEND) -o test_runtime

test_backend:
	$(CC) $(CFLAGS) test_backend.c $(BACKEND) -o test_backend

test_profile:
	$(CC) $(CFLAGS) test_profile.c $(PROFILE) -o test_profile $(PKG_LIBS)

test_profile_apply:
	$(CC) $(CFLAGS) test_profile_apply.c $(PROFILE_APPLY) -o test_profile_apply

test_probe test_hwdb test_auto test_base test_steam test_conf:
	$(CC) $(CFLAGS) $@.c $(CORE) -o $@ $(PKG_LIBS)

clean:
	rm -f $(TESTS)
