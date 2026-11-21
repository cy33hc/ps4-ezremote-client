#!/usr/bin/env bash
# Build ezRemote Server as an OpenOrbis gdd daemon eboot.
#
#   ./build_server.sh
#
# Stages:
#   data/daemon/ezremote-server.self
#   data/daemon/ezremote-server.md5
#   data/daemon/ezremote-server.sfo
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT"

if [[ -n "${OPENORBIS:-}" && -d "$OPENORBIS" ]]; then
    SDK="$OPENORBIS"
elif [[ -d "$ROOT/.sdk/pacbrew/ps4/openorbis" ]]; then
    SDK="$ROOT/.sdk/pacbrew/ps4/openorbis"
elif [[ -d /opt/pacbrew/ps4/openorbis ]]; then
    SDK="/opt/pacbrew/ps4/openorbis"
else
    echo "error: OpenOrbis SDK not found (set OPENORBIS)" >&2
    exit 1
fi

export OPENORBIS="$SDK"
export OO_PS4_TOOLCHAIN="$SDK"

AUTH_INFO="${AUTH_INFO:-010000000010003800000000001c004000ff00000000008000000000000000000000000000000000000000c000400040000000000000008000000000000000f00040ffff000000f000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000}"
BUILD_DIR="${BUILD_DIR:-$ROOT/build_server}"
TOOLCHAIN="$SDK/cmake/ps4.cmake"

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

cmake -S "$ROOT/ps4-ezremote-server" -B "$BUILD_DIR" \
    -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" \
    -DOPENORBIS="$SDK" \
    -DEZREMOTE_DAEMON_AUTHINFO="$AUTH_INFO"
cmake --build "$BUILD_DIR" --target eboot -j8

md5sum "$ROOT/data/daemon/ezremote-server.self" | cut -d' ' -f1 > "$ROOT/data/daemon/ezremote-server.md5"
"$ROOT/scripts/make_server_sfo.sh" "$SDK"

echo
echo "Built:"
echo "  $BUILD_DIR/eboot.bin                       (gdd daemon eboot)"
echo "  $ROOT/data/daemon/ezremote-server.self     (staged copy)"
echo "  $ROOT/data/daemon/ezremote-server.md5      (MD5 hash)"
echo "  $ROOT/data/daemon/ezremote-server.sfo      (gdd param.sfo)"
echo "  $BUILD_DIR/ezremote-server-1.12.elf        (linked ELF)"
