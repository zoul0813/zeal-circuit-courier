# Development

`hud_init()` defines this game’s shared `ASCII_MAP` before drawing HUD text.
Sprite and tilemap text use that same map.

All commands run from this repository. No sibling game or parent workspace is required.

## Build variants

`zde cmake` configures `build/` on first use and builds it. ZDE forwards subsequent
arguments to `cmake --build`, so use `zde exec cmake` to set configuration options:

```sh
zde exec cmake -B build-replay -DREPLAY=ON -DBIN_DIR=/src/build-replay/out
zde cmake build-replay
```

The `/src` path is ZDE's mount of this repository. The optional replay build is
for testing and recording; leave the option off for a normal game build.
Use `zde cmake --target clean` to clean normal build products without deleting saves.
Keep variants in separate build directories. Configuration and compiler failures
must be resolved before testing the resulting binaries.

Generate `src/replay.h` with `python3 tools/replay.py "$(cat tools/solution.txt)"`
before building a scripted replay. This deliberately updates a tracked test fixture.

## Assets

Tracked files in `assets/` are the editable sources. The build exports TMX maps
through the SDK's `tiled2zeal` helper where used, and keeps generated tile bytes,
metadata and archives in the build directory. Do not place build outputs in `assets/`.

Optional asset generators and preview tools require Python 3 and Pillow:

```sh
python3 -m venv .venv
. .venv/bin/activate
python3 -m pip install -r requirements-dev.txt
```

Generator scripts: `tools/make_music.py`, `tools/make_tileset.py`.
They recreate tracked art, music or source definitions; review their diffs before committing.

Edit TMX/TSX files in `assets/` with Tiled. Rebuild to export the changes; no
separate map-generator command is needed.

## Tests

Native host tests use a C compiler and SDK source headers. Export `ZGDK_PATH`,
`ZOS_PATH`, `ZVB_SDK_PATH` and, for archive users, `ZAR_PATH` to the installed
SDK checkouts on your host. `ZDE_PATH` can supply dependency defaults in the
simulation runners. Host collision models are test-only; target collision uses
ZGDK assembly.

```sh
bash tests/run_room.sh
bash tests/run_sim.sh
```

## Profiling and screenshots

Use the same emulator version when comparing timings. Install `zeal-native` on
PATH, or set `ZEAL_NATIVE` to its executable. Set `ZOS_ROM` to a ZOS ROM image;
`ZOS_PATH` or `ZDE_PATH` can provide the default ROM location.

```sh
export ZEAL_NATIVE=/path/to/zeal-native
export ZOS_ROM=/path/to/os_with_romdisk.img
mkdir -p perf
python3 tools/bpfps.py build-replay/out/courier_replay.bin 30 --load --dump perf/run
```

Use `--emu` and `--rom` to override those settings for an individual Python tool.
Profilers need the matching `.map` next to the binary and any runtime `.zar` in
the same output folder. `--args` passes additional emulator options, including
scratch disk images when needed. Timing tools change to `H:/` before launching
so relative archive paths work. Profiling evidence stays in ignored `perf/`.

## Optional X11 recording

Recording/input helpers require Linux with X11, Xvfb, ffmpeg, and python-xlib.
They use a 1280x800 virtual display and capture the emulator's 640x480 window at
+320+160. Set `ZPLAY_DISPLAY` to change the default `:99` display. These X11
helpers are not native macOS or Windows recording tools. Use those systems'
screen recording software instead. Emulator and ROM settings are the same as above.

`tools/play-replay.sh` builds via ZDE and uses local `xtype.py` / `drive.py`
for input and timed screenshots. `drive.py` screenshots also require ImageMagick.
Logs and captures are kept in ignored `run/replay/`.

Build output, Python environments, local saves, videos and profiling data are
ignored by Git. Existing ignored files can be retained locally.

Tileset reference previews are stored in `screenshots/assets/`; any map
reference renders are in `screenshots/maps/`.

## Menu control glyphs

`assets/snes-pad.gif` is the editable 13-tile SNES glyph sheet (palette index 0
is magenta, `#FF00FF`, and explicitly transparent), in the order
B, A, Y, X, L, R, Start, Select, D-pad, Up, Down, Left, Right. Start is the
triangle button and Select is the two-line button. All eight games carry the
same reusable sheet and `tools/snes_glyphs.py`, locally for standalone builds.
The asset generator appends these tiles without moving existing tile IDs;
`src/pad_glyphs.h` records the resulting IDs and prompt string macros.

Edit the source GIF, then rerun this game's normal asset generator. To recreate
the stock glyph sheet, run `python3 tools/snes_glyphs.py --create` first.
The glyph palette uses slots 248–255, outside the games' palette animations.
The exporter matches the font tile's background index: opaque for Courier and
Lantern, transparent for the other six games.
Lowercase ASCII slots a–m are reserved for these icons in the shared ASCII_MAP.
`PAD_B`, `PAD_START`, etc. are string macros because prompts concatenate them
with labels; the existing HUD icon character macros remain characters.

Pre-game menus default to a detected SNES pad. New keyboard presses select
keyboard prompts; new pad presses select pad prompts. Releases and an idle pad
do not override the selection; simultaneous new presses favor the keyboard.
The menu checks raw pad activity after its normal input read. This adds one pad
read only in menus. Gameplay and its hints retain the selection for the run.
