#!/usr/bin/env bash
# Project Atlas · plugins de audio fuera de la partición raíz.
#
# Descarga paquetes de plugins (LV2/LADSPA/CLAP/VST3) de los repositorios de
# Arch SIN usar root, verifica su SHA-256 contra la base de datos local de
# pacman y extrae solo los plugins en un disco grande. Los hosts (EasyEffects,
# Carla, filter-chain de PipeWire...) los encuentran mediante LV2_PATH y
# compañía, declarados en ~/.config/environment.d/.
#
# Uso:
#   scripts/plugins-external.sh [--dir DIR] [paquete ...]
#   scripts/plugins-external.sh --remove
#
# Por defecto: DIR=/mnt/disco16t/plugins_audio, paquetes zam-plugins-lv2 x42-plugins-lv2
set -euo pipefail

DIR=/mnt/disco16t/plugins_audio
PKGS=()
REMOVE=0
ENVFILE="$HOME/.config/environment.d/60-atlas-audio-plugins.conf"

while [ $# -gt 0 ]; do
    case "$1" in
        --dir) DIR="$2"; shift 2 ;;
        --remove) REMOVE=1; shift ;;
        -h|--help) sed -n 2,16p "$0"; exit 0 ;;
        *) PKGS+=("$1"); shift ;;
    esac
done
[ ${#PKGS[@]} -eq 0 ] && PKGS=(zam-plugins-lv2 x42-plugins-lv2)

if [ "$REMOVE" = 1 ]; then
    rm -f "$ENVFILE"
    echo "Quitado $ENVFILE. Los archivos de $DIR no se borran (hazlo tú si quieres)."
    exit 0
fi

command -v bsdtar >/dev/null || { echo "Falta bsdtar (paquete libarchive)."; exit 1; }
command -v curl >/dev/null || { echo "Falta curl."; exit 1; }

mkdir -p "$DIR"/{pkgs,lv2,ladspa,clap,vst3}

# SHA-256 esperado según la base de datos sync de pacman (fuente de confianza local)
expected_sha() {
    local name="$1" ver="$2" db
    for db in /var/lib/pacman/sync/*.db; do
        bsdtar -xOf "$db" "$name-$ver/desc" 2>/dev/null |
            awk '/^%SHA256SUM%/{getline; print; exit}' && return 0
    done
    return 1
}

missing_deps_total=()
for pkg in "${PKGS[@]}"; do
    url=$(pacman -Sp --print-format '%l' "$pkg" 2>/dev/null | tail -1) || true
    ver=$(pacman -Si "$pkg" 2>/dev/null | awk -F': ' '/^Version/{print $2; exit}') || true
    if [ -z "$url" ] || [ -z "$ver" ]; then
        echo "✗ $pkg: no está en los repositorios (¿pacman -Sy reciente?)"
        continue
    fi
    file="$DIR/pkgs/$(basename "$url")"
    echo "→ $pkg $ver"
    [ -f "$file" ] || curl -fL --progress-bar -o "$file.part" "$url" && { [ -f "$file" ] || mv "$file.part" "$file"; }

    want=$(expected_sha "$pkg" "$ver" | tr -d ' \n') || true
    got=$(sha256sum "$file" | cut -d' ' -f1)
    if [ -z "$want" ] || [ "$want" != "$got" ]; then
        echo "✗ $pkg: SHA-256 no coincide con la base de datos de pacman; se descarta"
        rm -f "$file"
        continue
    fi
    echo "  ✓ SHA-256 verificado"

    tmp=$(mktemp -d)
    bsdtar -xf "$file" -C "$tmp" usr/lib 2>/dev/null || true
    for kind in lv2 ladspa clap vst3; do
        if [ -d "$tmp/usr/lib/$kind" ]; then
            cp -a "$tmp/usr/lib/$kind/." "$DIR/$kind/"
            echo "  ✓ $kind: $(find "$tmp/usr/lib/$kind" -mindepth 1 -maxdepth 1 | wc -l) plugin(s)"
        fi
    done
    rm -rf "$tmp"

    # dependencias de ejecución que faltan en el sistema
    deps=$(pacman -Si "$pkg" | awk -F': ' '/^Depends On/{print $2; exit}')
    for d in $deps; do
        [ "$d" = "None" ] && continue
        case "$d" in lv2-host|*.so*) continue ;; esac
        pacman -T "$d" >/dev/null 2>&1 || missing_deps_total+=("${d%%[<>=]*}")
    done
done

mkdir -p "$(dirname "$ENVFILE")"
cat > "$ENVFILE" <<EOF
# Project Atlas: plugins de audio fuera de la partición raíz ($DIR).
# Se añaden a las rutas estándar; quitar con: scripts/plugins-external.sh --remove
LV2_PATH=\$HOME/.lv2:/usr/local/lib/lv2:/usr/lib/lv2:$DIR/lv2
LADSPA_PATH=\$HOME/.ladspa:/usr/local/lib/ladspa:/usr/lib/ladspa:$DIR/ladspa
CLAP_PATH=\$HOME/.clap:/usr/lib/clap:$DIR/clap
VST3_PATH=\$HOME/.vst3:/usr/lib/vst3:$DIR/vst3
EOF
echo
echo "✓ Rutas declaradas en $ENVFILE"
echo "  Se aplican a los servicios de usuario al reiniciar la sesión (o ahora con:"
echo "  systemctl --user daemon-reexec)."
echo "  Espacio usado en $DIR: $(du -sh "$DIR" | cut -f1) · en / : 0"

if [ ${#missing_deps_total[@]} -gt 0 ]; then
    printf '\n⚠ Bibliotecas que faltan en el sistema (algunos plugins no cargarán sin ellas):\n'
    mapfile -t missing_unique < <(printf '%s\n' "${missing_deps_total[@]}" | sort -u)
    printf '   %s\n' "${missing_unique[@]}"
    echo "   Son pequeñas; instálalas con: sudo pacman -S --asdeps ${missing_unique[*]}"
fi
