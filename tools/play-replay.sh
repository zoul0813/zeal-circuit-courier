#!/usr/bin/env bash
# Build a replay and run it with optional X11 screenshots. Requires Linux/X11.
# Usage: tools/play-replay.sh "<replay script>" [d:Key u:Key w:sec s:name ...]
set -euo pipefail
if [ "${1:-}" = --help ] || [ "$#" -lt 1 ]; then
  echo 'Usage: tools/play-replay.sh "<replay script>" [drive steps...]'
  [ "${1:-}" = --help ] && exit 0
  exit 2
fi
GAME="$(cd "$(dirname "$0")/.." && pwd)"
cd "$GAME"
EMU="${ZEAL_NATIVE:-zeal-native}"
ROM="${ZOS_ROM:-${ZOS_PATH:-${ZDE_PATH:+$ZDE_PATH/home/Zeal-8-bit-OS}}/build/os_with_romdisk.img}"
ZDE="${ZDE:-zde}"
command -v "$EMU" >/dev/null 2>&1 || { echo 'Set ZEAL_NATIVE to the zeal-native executable' >&2; exit 1; }
[ -f "$ROM" ] || { echo 'Set ZOS_ROM to a ZOS ROM image' >&2; exit 1; }
python3 tools/replay.py "$1"; shift
export ZDE_SOFT_EXIT=0
"$ZDE" --no-tty exec cmake -B build-replay -DREPLAY=ON -DBIN_DIR=/src/build-replay/out
"$ZDE" --no-tty cmake build-replay
OUT="$GAME/run/replay"; mkdir -p "$OUT"
export DISPLAY="${ZPLAY_DISPLAY:-:99}"
pgrep -f "Xvfb $DISPLAY" >/dev/null || { (Xvfb "$DISPLAY" -screen 0 1280x800x24 >/dev/null 2>&1 &); sleep 2; }
"$EMU" -r "$ROM" -H "$GAME/build-replay/out" >"$OUT/emu.log" 2>&1 &
PID=$!; trap 'kill "$PID" 2>/dev/null || true' EXIT
sleep 4
python3 tools/xtype.py 'cd H:/\nexec courier_replay.bin\n'
sleep 1.5
python3 tools/drive.py "$OUT" "$@"
