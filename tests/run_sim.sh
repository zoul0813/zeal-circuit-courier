#!/usr/bin/env bash
# Build and run the host simulation (cc, no emulator) from the game folder.
# Usage: tests/run_sim.sh [script file] [-t from:to] [-m]   (see tests/host_sim.c)
# Needs ZOS_PATH, ZVB_SDK_PATH, ZGDK_PATH (default: the ZDE deps under $ZDE_PATH/home).
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
GAME="$HERE/.."
DEPS="${ZDE_PATH:+$ZDE_PATH/home}"
ZOS="${ZOS_PATH:-$DEPS/Zeal-8-bit-OS}"; ZVB="${ZVB_SDK_PATH:-$DEPS/Zeal-VideoBoard-SDK}"; ZGDK="${ZGDK_PATH:-$DEPS/zeal-game-dev-kit}"
ZAR="${ZAR_PATH:-$DEPS/zeal-archiver}"
[ -d "$ZGDK/include" ] && [ -d "$ZOS/sdk" ] && [ -d "$ZVB/include" ] || {
  echo 'Set ZDE_PATH, or ZGDK_PATH/ZOS_PATH/ZVB_SDK_PATH, to the installed SDK dependencies' >&2
  exit 1
}
OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT
mkdir -p "$OUT/tiles"
for tmx in "$GAME/assets/rooms/"*.tmx; do
    python3 "$ZVB/tools/tiled2zeal/tiled2zeal.py" -i "$tmx" -o "$OUT/tiles/$(basename "${tmx%.tmx}").ztm"
done
python3 "$GAME/tools/pack_maps.py" --out "$OUT/maps" --tiles "$OUT/tiles" > "$OUT/pack.log" 2>&1 || { cat "$OUT/pack.log"; exit 1; }
cc -DMAPS_DIR="\"$OUT/maps\"" -I"$OUT/maps" -std=gnu11 -w -O1 '-D__sfr=static volatile unsigned char' '-D__at(x)=' '-D__banked=' -D__SDCC_VERSION_MAJOR=4 -D__SDCC_VERSION_MINOR=4 '-D__sdcccall(x)=' \
    -I"$HERE/stub" -I"$GAME/src" -I"$ZGDK/include" -I"$ZOS/sdk/sdcc/include" -I"$ZVB/include" \
    -I"$ZAR/include" \
    -o "$OUT/host_sim" "$HERE/host_sim.c" \
    "$HERE/physics_reference.c" "$ZGDK/src/entities"/*.c "$ZGDK/src/physics/body"/*.c "$ZGDK/src/utils/clamp_i16.c" "$ZGDK/src/physics/platformer"/*.c "$ZGDK/src/tilemap/flags"/*.c "$ZGDK/src/anims"/*.c "$ZGDK/src/entities/presets"/*.c "$ZGDK/src/tilemap/load.c" "$GAME/src/room.c"
cd "$GAME" && "$OUT/host_sim" "$@"
