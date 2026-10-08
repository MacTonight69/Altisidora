#!/usr/bin/env bash
# Altisidora launcher: builds (incremental) if needed and runs the app.
# Uso: ./run.sh   (u opcionalmente pasa argumentos al binario)
set -euo pipefail
cd "$(dirname "$0")"

BUILD_DIR="build"
BIN="$BUILD_DIR/altisidora"

# Configure the build tree on first use.
if [[ ! -f "$BUILD_DIR/CMakeCache.txt" ]]; then
    echo "[run.sh] Configurando build (primera vez)..."
    cmake -S . -B "$BUILD_DIR"
else
    # Re-configure to refresh install rules or new CMake options.
    cmake -S . -B "$BUILD_DIR" >/dev/null
fi

echo "[run.sh] Compilando (incremental)..."
cmake --build "$BUILD_DIR" -j"$(nproc)"

exec "$BIN" "$@"