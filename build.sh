#!/usr/bin/env bash
# Unified build for the Haruka Editor + its engine with a SINGLE module mask (FetchContent version).
# Espejo de Survival/build.sh: misma forma de compilar que el juego.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
VERSION="${VERSION:-0.1}"

BRANCH_NAME="${BRANCH_NAME:-main}" MODULES=""; CLEAN=0; RELEASE=0; REMOTE=0; ENGINE_LOCAL=""; RPM=0
CMAKE_EXTRA=()   # args -D... del IDE (-DPROJECT_NAME, -DHARUKA_ENGINE_LOCAL, …) → se reenvían a cmake
for arg in "$@"; do
    case "$arg" in
        BRANCH=*) BRANCH_NAME="${arg#BRANCH=}" ;;
        MODULES=*) MODULES="${arg#MODULES=}" ;;
        ENGINE=*)  ENGINE_LOCAL="${arg#ENGINE=}" ;; # ruta a un motor local concreto
        release)   RELEASE=1 ;;
        rpm)       RPM=1; RELEASE=1 ;;  # paquete instalable (implica release: empaqueta lo instalado)
        clean)     CLEAN=1 ;;
        remote)    REMOTE=1 ;;      # forzar el motor de GitHub (ignora el ../haruka-cpp local)
        -D*)       CMAKE_EXTRA+=("$arg") ;;
        *) echo "unknown arg: $arg"; exit 2 ;;
    esac
done

# 'rpm' cambia el LAYOUT de instalación (FHS bajo /usr en vez de todo junto en dist/) y ese
# layout se compila DENTRO del binario (HARUKA_EDITOR_ENGINE_DIR). Mismo motivo que el guard
# de RELEASE de más abajo: una caché con el layout contrario da un binario que busca el motor
# donde el paquete no lo pone.
if [ "$RPM" = "1" ]; then LAYOUT=system; else LAYOUT=portable; fi

if [ "$CLEAN" = "1" ]; then rm -rf "$HERE/build" "$HERE/dist" "$HERE/packages"; fi

# Selección del motor: 'remote' lo fuerza a GitHub; ENGINE=ruta usa un local concreto; por
# defecto, el CMake del editor autodetecta ../haruka-cpp (local) o cae a GitHub (CI/clon).
ENGINE_ARGS=()
if [ "$REMOTE" = "1" ]; then
    ENGINE_ARGS=(-DHARUKA_ENGINE_LOCAL="")          # vacío → fuerza remoto
    ENGINE_MODE="REMOTO (GitHub @ $BRANCH_NAME)"
elif [ -n "$ENGINE_LOCAL" ]; then
    ENGINE_ARGS=(-DHARUKA_ENGINE_LOCAL="$ENGINE_LOCAL")
    ENGINE_MODE="LOCAL @ $ENGINE_LOCAL"
else
    ENGINE_MODE="auto (LOCAL ../haruka-cpp si existe, si no GitHub)"
fi

echo "==> Building Editor + Engine — engine=$ENGINE_MODE, MODULES='${MODULES:-<all on>}', release=$RELEASE"

WANT_RELEASE=$([ "$RELEASE" = "1" ] && echo ON || echo OFF)

# Blindaje contra caché pegada: HARUKA_RELEASE cambia el LAYOUT de assets en TIEMPO DE
# COMPILACIÓN (asset_paths.h: maps()→scenes/ en dev vs assets/maps/ en release). Si un build
# previo dejó RELEASE en la caché con el valor contrario, mezclar binario+assets de modos
# distintos da el clásico "No se pudo abrir assets/maps/menu". Mismo guard que Survival/build.sh.
CACHE="$HERE/build/CMakeCache.txt"
if [ -f "$CACHE" ] && ! grep -qx "RELEASE:BOOL=$WANT_RELEASE" "$CACHE"; then
    echo "==> RELEASE en caché != $WANT_RELEASE → reconfigurando limpio (evita layout de assets cruzado)"
    rm -rf "$HERE/build"
elif [ -f "$CACHE" ] && ! grep -qx "HARUKA_INSTALL_LAYOUT:STRING=$LAYOUT" "$CACHE"; then
    echo "==> Layout en caché != $LAYOUT → reconfigurando limpio (rutas de instalación compiladas)"
    rm -rf "$HERE/build"
fi

cmake -S "$HERE" -B "$HERE/build" \
      -DBRANCH_NAME="$BRANCH_NAME" \
      -DMODULES="$MODULES" \
      -DRELEASE=$WANT_RELEASE \
      -DHARUKA_INSTALL_LAYOUT="$LAYOUT" \
      "${ENGINE_ARGS[@]}" "${CMAKE_EXTRA[@]}" >/dev/null

# PARALELISMO ACOTADO POR RAM, no por núcleos. Cada g++ pide ~0.4-0.5 GB de pico (medido):
# `-j` sin número abre TODOS los trabajos y el build tira de swap. Se calcula desde MemAvailable
# (lo que el kernel puede reclamar de verdad), dejando 1.5 GB al sistema. Override: JOBS=8
if [ -z "${JOBS:-}" ]; then
    CORES=$(nproc 2>/dev/null || echo 4)
    AVAIL_MB=$(awk '/^MemAvailable:/ {print int($2/1024)}' /proc/meminfo 2>/dev/null || echo 0)
    if [ "$AVAIL_MB" -gt 0 ]; then
        USABLE_MB=$(( AVAIL_MB - 1536 ))            # reserva para el sistema
        [ "$USABLE_MB" -lt 600 ] && USABLE_MB=600
        JOBS=$(( USABLE_MB / 600 ))                 # ~0.6 GB por trabajo (margen sobre los 0.5 medidos)
        [ "$JOBS" -lt 1 ] && JOBS=1
        [ "$JOBS" -gt "$CORES" ] && JOBS=$CORES
    else
        JOBS=$CORES
    fi
fi
echo "==> Compilando con -j$JOBS (nproc=$(nproc 2>/dev/null || echo ?), MemAvailable=$(awk '/^MemAvailable:/ {print int($2/1024)}' /proc/meminfo 2>/dev/null || echo ?) MB). Override: JOBS=N $0"

cmake --build "$HERE/build" -j"$JOBS"

if [ "$RELEASE" = "1" ] && [ "$RPM" = "0" ]; then
    echo "==> Packaging release layout"
    cmake --install "$HERE/build" --prefix "$HERE/dist" >/dev/null
    echo "==> Release at: $HERE/dist"
fi

if [ "$RPM" = "1" ]; then
    command -v rpmbuild >/dev/null || { echo "✗ falta rpmbuild (sudo dnf install rpm-build)"; exit 1; }
    echo "==> Empaquetando .rpm"
    # -B packages/, NO dist/: cpack deja ahí su staging (_CPack_Packages) además del paquete,
    # y dist/ es el layout portable que se copia tal cual — no debe llevar dentro un .rpm.
    ( cd "$HERE/build" && cpack -G RPM -B "$HERE/packages" )
    echo "==> RPM en: $(ls -1 "$HERE"/packages/*.rpm 2>/dev/null | tail -1)"
    echo "    Instalar:    sudo dnf install $HERE/packages/*.rpm"
    echo "    Desinstalar: sudo dnf remove haruka-editor"
fi
echo "==> Done."
