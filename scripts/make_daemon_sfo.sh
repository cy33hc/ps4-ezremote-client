#!/usr/bin/env bash
# Generate the gdd daemon param.sfo for the launcher's data/daemon payload.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SDK="${1:-${OPENORBIS:-$ROOT/.sdk/pacbrew/ps4/openorbis}}"
PKGTOOL="$SDK/bin/linux/PkgTool.Core"
SFO="$ROOT/data/daemon/ezremote_client_webui.sfo"

if [[ ! -x "$PKGTOOL" ]]; then
    echo "error: PkgTool.Core not found at $PKGTOOL" >&2
    exit 1
fi

export DOTNET_SYSTEM_GLOBALIZATION_INVARIANT=1
if [[ -d "$ROOT/.sdk/libssl1" ]]; then
    export LD_LIBRARY_PATH="$ROOT/.sdk/libssl1${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi

mkdir -p "$ROOT/data/daemon"
rm -f "$SFO"
"$PKGTOOL" sfo_new "$SFO" >/dev/null

set_entry() {
    "$PKGTOOL" sfo_setentry "$SFO" "$1" --type "$2" --maxsize "$3" --value "$4" >/dev/null
}

set_entry APP_TYPE Integer 4 1
set_entry APP_VER Utf8 8 "02.12"
set_entry ATTRIBUTE Integer 4 0
set_entry CATEGORY Utf8 4 "gdd"
set_entry CONTENT_ID Utf8 48 "IV0001-EZRC00001_00-EZRC000010201200"
set_entry DOWNLOAD_DATA_SIZE Integer 4 0
set_entry FORMAT Utf8 4 "obs"
set_entry SYSTEM_VER Integer 4 1020
set_entry TITLE Utf8 128 "ezRemote Client"
set_entry TITLE_ID Utf8 12 "EZRC00001"
set_entry VERSION Utf8 8 "02.12"

echo "Generated $SFO"
