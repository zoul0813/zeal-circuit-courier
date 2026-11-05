#!/usr/bin/env bash
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
GAME="$HERE/.."
OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT
ZGDK="$ZGDK_PATH"
cc -std=gnu11 -w -g -fsanitize=address,undefined \
    '-D__sfr=static volatile unsigned char' '-D__at(x)=' \
    -I"$HERE/stub" -I"$GAME/src" -I"$ZGDK/include" \
    -I"$ZOS_PATH/sdk/sdcc/include" -I"$ZVB_SDK_PATH/include" \
    "$HERE/host_room.c" "$HERE/physics_reference.c" "$GAME/src/room.c" "$ZGDK/src/physics/body/"*.c "$ZGDK/src/utils/clamp_i16.c" \
    "$ZGDK/src/tilemap/flags/"*.c "$ZGDK/src/tilemap/load.c" -o "$OUT/room"
"$OUT/room"
