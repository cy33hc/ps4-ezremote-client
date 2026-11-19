#!/usr/bin/env bash
# Build the headless payload ELF with ps4-payload-sdk (for elfldr).
#
#   ./build_elf.sh
#
# Override the build directory with BUILD_DIR=..., the payload SDK with
# PS4_PAYLOAD_SDK=..., and the OpenOrbis SDK with OPENORBIS=...
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT"

PAYLOAD_SDK="${PS4_PAYLOAD_SDK:-/opt/ps4-payload-sdk}"
if [[ ! -f "$PAYLOAD_SDK/toolchain/orbis.cmake" ]]; then
    echo "error: ps4-payload-sdk not found at $PAYLOAD_SDK (set PS4_PAYLOAD_SDK)" >&2
    exit 1
fi
# The SDK's linker wrapper reads this environment variable.
export PS4_PAYLOAD_SDK="$PAYLOAD_SDK"

# OpenOrbis is only needed for the orbis/ headers and the libSceBgft.so stub.
if [[ -n "${OPENORBIS:-}" && -d "$OPENORBIS" ]]; then
    ORBIS_SDK="$OPENORBIS"
elif [[ -d "$ROOT/.sdk/pacbrew/ps4/openorbis" ]]; then
    ORBIS_SDK="$ROOT/.sdk/pacbrew/ps4/openorbis"
elif [[ -d /opt/pacbrew/ps4/openorbis ]]; then
    ORBIS_SDK="/opt/pacbrew/ps4/openorbis"
else
    echo "error: OpenOrbis SDK not found (set OPENORBIS); it provides orbis/ headers for the payload build" >&2
    exit 1
fi

BUILD_DIR="${BUILD_DIR:-$ROOT/build_elf}"
TOOLCHAIN="$PAYLOAD_SDK/toolchain/orbis.cmake"

# A build directory configured with a different toolchain (or a now-missing
# compiler) cannot be reused.
if [[ -f "$BUILD_DIR/CMakeCache.txt" ]]; then
    cached_toolchain="$(sed -n 's/^CMAKE_TOOLCHAIN_FILE:[A-Z]*=//p' "$BUILD_DIR/CMakeCache.txt")"
    cached_cc="$(sed -n 's/^CMAKE_C_COMPILER:[A-Z]*=//p' "$BUILD_DIR/CMakeCache.txt")"
    needs_clean=0
    [[ "$cached_toolchain" != "$TOOLCHAIN" ]] && needs_clean=1
    [[ -n "$cached_cc" && ! -x "$cached_cc" ]] && needs_clean=1

    if [[ $needs_clean -eq 1 ]]; then
        if [[ "$BUILD_DIR" == "$ROOT"/* ]]; then
            echo "recreating $BUILD_DIR (toolchain or compiler changed)"
            rm -rf "$BUILD_DIR"
        else
            echo "error: $BUILD_DIR is stale (toolchain '$cached_toolchain', compiler '$cached_cc')" >&2
            exit 1
        fi
    fi
fi

cmake -S "$ROOT" -B "$BUILD_DIR" \
    -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" \
    -DEZREMOTE_ORBIS_SDK="$ORBIS_SDK"
cmake --build "$BUILD_DIR" --target elf -j8
cmake --build "$BUILD_DIR" --target payload-data

echo
echo "Built:"
echo "  $BUILD_DIR/ezremote_client.elf              (load with elfldr)"
echo "  $ROOT/data/daemon/ezremote_client.elf       (stripped client payload)"
echo "  $ROOT/data/daemon/ezremote-server.elf       (ezRemote Server payload)"
echo "  $BUILD_DIR/payload-data/assets              (copy to /data/ezremote-client/assets)"
