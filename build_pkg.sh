#!/usr/bin/env bash
# Build the normal PS4 app and package with the OpenOrbis SDK.
#
#   ./build_pkg.sh
#
# Override the build directory with BUILD_DIR=... and the SDK with OPENORBIS=...
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT"

# Optional: ./build_pkg.sh upload
# Uploads the built pkg and data/daemon via the CMake upload target.
UPLOAD=0
for arg in "$@"; do
    case "$arg" in
        upload|--upload) UPLOAD=1 ;;
        *)
            echo "usage: $0 [upload]" >&2
            exit 1
            ;;
    esac
done

# Locate the OpenOrbis SDK: $OPENORBIS, the in-tree .sdk, or the system install.
if [[ -n "${OPENORBIS:-}" && -d "$OPENORBIS" ]]; then
    SDK="$OPENORBIS"
elif [[ -d "$ROOT/.sdk/pacbrew/ps4/openorbis" ]]; then
    SDK="$ROOT/.sdk/pacbrew/ps4/openorbis"
elif [[ -d /opt/pacbrew/ps4/openorbis ]]; then
    SDK="/opt/pacbrew/ps4/openorbis"
else
    echo "error: OpenOrbis SDK not found (set OPENORBIS or install it under /opt/pacbrew/ps4/openorbis)" >&2
    exit 1
fi

export OPENORBIS="$SDK"
export OO_PS4_TOOLCHAIN="$SDK"

# PkgTool is a self-contained .NET Core 3 application: it needs invariant
# globalization and OpenSSL 1.1 on the build host.
export DOTNET_SYSTEM_GLOBALIZATION_INVARIANT=1
if [[ -d "$ROOT/.sdk/libssl1" ]]; then
    export LD_LIBRARY_PATH="$ROOT/.sdk/libssl1${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi

BUILD_DIR="${BUILD_DIR:-$ROOT/build}"
TOOLCHAIN="$SDK/cmake/ps4.cmake"

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

# The client pkg bundles the server daemon, so stage the server first.
BUILD_DIR="${SERVER_BUILD_DIR:-$ROOT/build_server}" "$ROOT/build_server.sh"

cmake -S "$ROOT" -B "$BUILD_DIR" -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN"
cmake --build "$BUILD_DIR" --target ezremote_client_pkg -j8

if [[ $UPLOAD -eq 1 ]]; then
    echo
    echo "Uploading package and daemons..."
    cmake --build "$BUILD_DIR" --target upload
fi

echo
echo "Built:"
ls -1 "$BUILD_DIR"/*.pkg
