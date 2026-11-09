#!/usr/bin/env bash
# Análisis estático de todo el proyecto (lo ejecuta `make lint`).
#
#   C      gcc -fanalyzer (fugas, nulos, descriptores) sobre lo que se compila
#   Python ruff, si está uv (uvx) o ruff instalado
#   Shell  shellcheck, si está instalado o vía uvx
#
# Sale con error si alguna herramienta encuentra algo.
set -u
cd "$(dirname "$0")/.." || exit 1

status=0
pkgs=(libpipewire-0.3 wireplumber-0.5 json-glib-1.0 gio-2.0 gio-unix-2.0 alsa)
read -r -a cflags <<< "$(pkg-config --cflags "${pkgs[@]}") -Iinclude -Iinclude/common -std=gnu17 -O2"
read -r -a sources <<< "$(make -s print-src)"

echo "== C: gcc -fanalyzer (${#sources[@]} archivos)"
report=$(mktemp)
for f in "${sources[@]}"; do
    gcc "${cflags[@]}" -fanalyzer -c "$f" -o /dev/null 2>>"$report"
done
# Falsos positivos conocidos y revisados: devnull en child_setup_stdio se deja
# abierto a propósito cuando ocupa 0-2, y buf nunca es NULL tras el realloc.
grep -E 'warning:' "$report" | grep -v -E 'atlas_util\.c:.*(devnull|.buf.)' > "$report.real" || true
if [ -s "$report.real" ]; then
    cat "$report.real"
    status=1
else
    echo "   sin avisos"
fi
rm -f "$report" "$report.real"

py_dirs=(gui/atlasaudio gui/bin/atlas-audio)
ruff_rules="E,F,W,B,UP,SIM,PL,RUF,C4,PIE,RET"
ruff_ignore="PLR0911,PLR0912,PLR0913,PLR0915,PLR0917,PLR2004,PLC0415"
echo "== Python: ruff"
if command -v ruff >/dev/null; then ruff_cmd=(ruff)
elif command -v uvx >/dev/null; then ruff_cmd=(uvx ruff)
else ruff_cmd=(); echo "   (omitido: instala uv o ruff)"; fi
if [ ${#ruff_cmd[@]} -gt 0 ]; then
    "${ruff_cmd[@]}" check --select "$ruff_rules" --ignore "$ruff_ignore" \
        --line-length 120 --target-version py312 "${py_dirs[@]}" || status=1
fi

echo "== Shell: shellcheck"
mapfile -t scripts < <(git ls-files '*.sh')
if command -v shellcheck >/dev/null; then shellcheck "${scripts[@]}" || status=1
elif command -v uvx >/dev/null; then uvx --from shellcheck-py shellcheck "${scripts[@]}" || status=1
else echo "   (omitido: instala shellcheck o uv)"; fi

exit $status
