#!/usr/bin/env python3
"""Generate assets/tileset.gif (8-bit, 16x16 tiles, 16 per row) for Circuit Courier.

Art follows the original circuit-board concept mockup.
Tile indices must match src/tiles.h. Palette index 0 is transparent.
Also writes assets/anim.bin: the raw pixels (palette indices) of the conveyor and wind tiles, which the game
rotates and uploads again every few frames to animate them (src/game.c, anim_*).
Run: python3 tools/make_tileset.py   (from the game folder)
"""
import os
from PIL import Image

T = 16
COLS = 16
ROWS = 10  # 160 tiles, zone themes 1..3 at 112..159

tiles = {}  # index -> 16x16 list of rgb or None


def new():
    return [[None] * T for _ in range(T)]


def hexc(s):
    s = s.lstrip("#")
    return tuple(int(s[i:i + 2], 16) for i in (0, 2, 4))


def r(t, x, y, w, h, col):
    c = hexc(col)
    for j in range(max(0, y), min(T, y + h)):
        for i in range(max(0, x), min(T, x + w)):
            t[j][i] = c


def sprite(t, rows, pal, flip=False):
    for j, row in enumerate(rows):
        for i in range(len(row)):
            k = row[len(row) - 1 - i] if flip else row[i]
            if k == "." or k not in pal:
                continue
            r(t, i, j, 1, 1, pal[k])


# ---------- L0 backdrop (opaque) ----------
# zone themes: backdrop (bg, via, trace, chip, chip pins) and boards (solid, edge, shadow, etch, pad)
THEMES = [
    # 0 BUS: green PCB
    (("#0c241f", "#12352d", "#113029", "#0f2d27", "#14382f"), ("#1f5c4b", "#5fb59a", "#123a30", "#2f7d66", "#d9a94a")),
    # 1 DMA: blue memory banks
    (("#0b1a2b", "#12284a", "#10223b", "#0e2036", "#16304e"), ("#1f4272", "#6fa6e6", "#11264a", "#30609c", "#d9a94a")),
    # 2 COOLING: grey steel
    (("#13171d", "#1d232c", "#1a2028", "#181d25", "#232a35"), ("#3d4a5b", "#b4d2e6", "#222a35", "#566b80", "#7cc6e8")),
    # 3 CORE: hot copper
    (("#22110d", "#341a12", "#2e1710", "#2a150f", "#3c2016"), ("#6b2f1f", "#f2a564", "#3f1a10", "#8c4630", "#ffe08a")),
]


def backdrop(h, v, th):
    BG, VIA, TRACE = th[0], th[1], th[2]
    t = new()
    r(t, 0, 0, T, T, BG)
    r(t, 7, 7, 2, 2, VIA)
    if h:
        r(t, 0, 7, T, 1, TRACE)
    if v:
        r(t, 7, 0, 1, T, TRACE)
    return t


def backdrops(base, th):
    """8 tiles: plain, h trace, v trace, both, then a 2x2 chip silhouette"""
    tiles[base] = backdrop(False, False, th)
    tiles[base + 1] = backdrop(True, False, th)
    tiles[base + 2] = backdrop(False, True, th)
    tiles[base + 3] = backdrop(True, True, th)
    for k, (pins_top, pins_bot) in enumerate([(True, False), (True, False), (False, True), (False, True)]):
        t = new()
        r(t, 0, 0, T, T, th[0])
        r(t, 0, 2 if pins_top else 0, T, 14, th[3])
        for p in (3, 11):
            if pins_top:
                r(t, p, 0, 3, 2, th[4])
            if pins_bot:
                r(t, p, 14, 3, 2, th[4])
        tiles[base + 4 + k] = t


backdrops(1, THEMES[0][0])

# ---------- L1 solids ----------
def board(top, variant, pad, th):
    SOLID, EDGE, SHADOW, ETCH, GOLD = th
    t = new()
    r(t, 0, 0, T, T, SOLID)
    if top:
        r(t, 0, 0, T, 1, EDGE)
    r(t, 0, 15, T, 1, SHADOW)
    if variant:
        r(t, 2, 5, 12, 1, ETCH)
        r(t, 12, 5, 1, 7, ETCH)
    else:
        r(t, 4, 2, 1, 10, ETCH)
        r(t, 4, 11, 9, 1, ETCH)
    if pad:
        if variant:
            r(t, 11, 10, 3, 3, GOLD)
        else:
            r(t, 3, 3, 3, 3, GOLD)
    return t


def boards(base, th):
    """5 tiles: two tops with a lit edge, three fills"""
    tiles[base] = board(True, 0, False, th)
    tiles[base + 1] = board(True, 1, True, th)
    tiles[base + 2] = board(False, 0, True, th)
    tiles[base + 3] = board(False, 1, False, th)
    tiles[base + 4] = board(False, 1, True, th)


boards(16, THEMES[0][1])
GOLD = THEMES[0][1][4]
# themes 1..3: boards at 112 + 16 * (n - 1), their backdrop 8 tiles further (src/tiles.h T_THEME)
for n in (1, 2, 3):
    boards(112 + 16 * (n - 1), THEMES[n][1])
    backdrops(112 + 16 * (n - 1) + 8, THEMES[n][0])

# one-way platform (header pin strip)
t = new()
r(t, 0, 0, T, 6, "#3e8f78")
r(t, 0, 0, T, 1, "#9be0c9")
r(t, 0, 5, T, 1, "#1d4d40")
r(t, 3, 2, 2, 2, "#1d4d40")
r(t, 11, 2, 2, 2, "#1d4d40")
tiles[21] = t

# ladder (bus lines)
t = new()
r(t, 3, 0, 2, T, "#b9b2a0")
r(t, 11, 0, 2, T, "#b9b2a0")
for k in range(2, T, 5):
    r(t, 3, k, 10, 2, "#b9b2a0")
    r(t, 3, k + 2, 10, 1, "#5c5748")
tiles[22] = t

# ladder top: ladder rails under a one-way lip, so the bot can stand on it and climb down
t = new()
r(t, 3, 0, 2, T, "#b9b2a0")
r(t, 11, 0, 2, T, "#b9b2a0")
for k in range(7, T, 5):
    r(t, 3, k, 10, 2, "#b9b2a0")
    r(t, 3, k + 2, 10, 1, "#5c5748")
r(t, 0, 0, T, 4, "#3e8f78")
r(t, 0, 0, T, 1, "#9be0c9")
r(t, 0, 3, T, 1, "#1d4d40")
tiles[37] = t

# hazard: spikes (exposed pins)
t = new()
for s in range(4):
    for h in range(8):
        w = max(1, (8 - h) // 2)
        r(t, s * 4 + 2 - w // 2, 8 + h, w, 1, "#ffd0c0" if h < 2 else "#c75c5c")
tiles[23] = t

# door closed
t = new()
r(t, 0, 0, T, T, "#2a2440")
for b in range(2, T, 4):
    r(t, b, 0, 2, T, "#c75c5c")
r(t, 0, 7, T, 2, "#7a3a3a")
tiles[24] = t

# socket empty / filled
def socket(filled):
    t = new()
    r(t, 2, 4, 12, 12, "#2a2440")
    r(t, 4, 6, 8, 8, "#0c0a14")
    r(t, 6, 2, 4, 3, "#7cc6e8" if filled else GOLD)
    if filled:
        r(t, 5, 7, 6, 6, "#7cc6e8")
        r(t, 6, 8, 2, 2, "#e8fbff")
    return t


tiles[25] = socket(False)
tiles[26] = socket(True)

# switch off / on (a solid block with a lamp)
def switch(on):
    t = new()
    r(t, 0, 0, T, T, "#3a3550")
    r(t, 0, 0, T, 1, "#6d6690")
    r(t, 0, 15, T, 1, "#1d1a2a")
    r(t, 4, 4, 8, 8, "#0c0a14")
    r(t, 5, 5, 6, 6, "#ffe08a" if on else "#5c2a2a")
    if on:
        r(t, 6, 6, 2, 2, "#ffffff")
    return t


tiles[27] = switch(False)
tiles[28] = switch(True)

# gate (solid, toggled by switches) and its open ghost
t = new()
r(t, 0, 0, T, T, "#1d3a55")
for b in range(1, T, 5):
    r(t, 0, b, T, 2, "#ffe08a")
r(t, 0, 0, 1, T, "#0c0a14")
r(t, 15, 0, 1, T, "#0c0a14")
tiles[29] = t
t = new()
for b in range(1, T, 5):
    r(t, 0, b, 1, 2, "#5a4e2a")
    r(t, 15, b, 1, 2, "#5a4e2a")
tiles[30] = t

# CPU goal (2x2 tiles: 31 32 / 47 48 would break the strip, so use 33..36)
def cpu(part):
    t = new()
    r(t, 0, 0, T, T, "#2c2a3a")
    # pins on outer edges
    for p in (3, 9):
        if part in (0, 1):
            r(t, p, 0, 3, 2, "#d9d4c3")
        if part in (2, 3):
            r(t, p, 14, 3, 2, "#d9d4c3")
    ox = 0 if part in (1, 3) else 4
    oy = 0 if part in (2, 3) else 4
    if part == 0:
        r(t, 4, 4, 12, 12, "#0c0a14")
        r(t, 7, 7, 3, 3, "#ffe08a")
    elif part == 1:
        r(t, 0, 4, 12, 12, "#0c0a14")
    elif part == 2:
        r(t, 4, 0, 12, 12, "#0c0a14")
    else:
        r(t, 0, 0, 12, 12, "#0c0a14")
        r(t, 2, 2, 8, 2, "#7cc6e8")
        r(t, 2, 6, 5, 2, "#7cc6e8")
    return t


for k in range(4):
    tiles[33 + k] = cpu(k)

# conveyor belts (solid). Rows 0..6 repeat every 8 px, so the game can rotate them sideways to move the belt
def conveyor(right):
    t = new()
    r(t, 0, 0, T, 7, "#2a2440")
    r(t, 0, 0, T, 1, "#b9b2a0")
    r(t, 0, 6, T, 1, "#5c5748")
    for j in range(1, 6):
        d = abs(j - 3)
        for base in (0, 8):
            x = base + (3 - d if right else 1 + d)
            r(t, x, j, 2, 1, "#ffe08a")
    r(t, 0, 7, T, 9, "#3a3550")
    r(t, 0, 15, T, 1, "#1d1a2a")
    for cx in (3, 11):   # rollers
        r(t, cx - 2, 9, 5, 5, "#0c0a14")
        r(t, cx - 1, 10, 3, 3, "#706a85")
        r(t, cx, 11, 1, 1, "#b9b2a0")
    return t


tiles[50] = conveyor(False)
tiles[51] = conveyor(True)


# fan grille in the floor (solid), blowing or off
def fan(on):
    t = new()
    r(t, 0, 0, T, T, "#3a3550")
    r(t, 0, 0, T, 1, "#a9e4ff" if on else "#6d6690")
    r(t, 0, 15, T, 1, "#1d1a2a")
    r(t, 2, 3, 12, 10, "#0c0a14")
    for k in range(3):
        r(t, 3 + k * 4, 4, 2, 8, "#7cc6e8" if on else "#4a4560")
    r(t, 2, 7, 12, 2, "#5f5a78")
    return t


tiles[52] = fan(True)
tiles[53] = fan(False)

# updraft (see-through streaks). Repeats every 16 rows, so the game can rotate it upwards
t = new()
for x, y0, ln, col in ((2, 1, 4, "#5f8fa3"), (6, 9, 5, "#a9e4ff"), (10, 4, 3, "#5f8fa3"), (13, 12, 4, "#a9e4ff"),
                       (8, 0, 2, "#5f8fa3")):
    for k in range(ln):
        r(t, x, (y0 + k) % T, 1, 1, col)
tiles[54] = t

# HUD background
t = new()
r(t, 0, 0, T, T, "#0b0a12")
tiles[31] = t

# ---------- sprites ----------
HERO = [
    "................",
    ".....kkkkkk.....",
    "....kHHHHHHk....",
    "...kHHhHHHHHk...",
    "...kkkkkkkkkk...",
    "...kSSSSSSSSk...",
    "...kSwkSSwkSk...",
    "...kSSSSSSSSk...",
    "....kSSSSSSk....",
    "...kkBBBBBBkk...",
    "..kSkBbBBBBkSk..",
    "..kkkBBBBBBkkk..",
    "....kBBkkBBk....",
    "....kBBkkBBk....",
    "...kFFFk.kFFFk..",
    "...kkkkk.kkkkk..",
]
HERO_RUN = [
    "................",
    ".....kkkkkk.....",
    "....kHHHHHHk....",
    "...kHHhHHHHHk...",
    "...kkkkkkkkkk...",
    "...kSSSSSSSSk...",
    "...kSSwkSSwkk...",
    "...kSSSSSSSSk...",
    "....kSSSSSSk....",
    "...kkBBBBBBkk...",
    "..kSkBbBBBBkSk..",
    "..kkkBBBBBBkkk..",
    "....kBBk.kBBk...",
    "...kBBk...kBBk..",
    "..kFFFk...kFFFk.",
    "..kkkkk...kkkkk.",
]
HERO_JUMP = [
    "................",
    ".....kkkkkk.....",
    "....kHHHHHHk....",
    "...kHHhHHHHHk...",
    "...kkkkkkkkkk...",
    "...kSSSSSSSSk...",
    "...kSSwkSSwkk...",
    "..kkSSSSSSSSk...",
    ".kSkkSSSSSSk....",
    ".kkkBBBBBBBkk...",
    "....kBbBBBBkSk..",
    "....kBBBBBBkkk..",
    "...kBBk.kBBBk...",
    "..kFFk...kFFFk..",
    "..kkk.....kkkk..",
    "................",
]
# carrying: both arms raised
HERO_CARRY = [
    "..kk........kk..",
    "..kSk.kkkk.kSk..",
    "..kSkHHHHHHkSk..",
    "..kSHHhHHHHHSk..",
    "...kkkkkkkkkk...",
    "...kSSSSSSSSk...",
    "...kSwkSSwkSk...",
    "...kSSSSSSSSk...",
    "....kSSSSSSk....",
    "...kkBBBBBBkk...",
    "....kBbBBBBk....",
    "....kBBBBBBk....",
    "....kBBkkBBk....",
    "....kBBkkBBk....",
    "...kFFFk.kFFFk..",
    "...kkkkk.kkkkk..",
]
HPAL = {"k": "#0b0a12", "H": "#d9d4c3", "h": "#ffffff", "S": "#2c2a3a", "w": "#7cc6e8",
        "B": "#e0a43a", "b": "#ffe08a", "F": "#706a85"}
for idx, rows in ((40, HERO), (41, HERO_RUN), (42, HERO_JUMP), (43, HERO_CARRY)):
    t = new()
    sprite(t, rows, HPAL)
    tiles[idx] = t

# data bit (8x8 centred) + glow frame
for idx, glow in ((44, False), (45, True)):
    t = new()
    r(t, 4, 4, 8, 8, "#0c0a14")
    r(t, 5, 5, 6, 6, "#7cc6e8")
    r(t, 6, 6, 2, 2, "#e8fbff")
    if glow:
        r(t, 2, 2, 1, 1, "#e8fbff")
        r(t, 13, 8, 1, 1, "#e8fbff")
        r(t, 7, 14, 1, 1, "#e8fbff")
    else:
        r(t, 13, 3, 1, 1, "#e8fbff")
        r(t, 2, 11, 1, 1, "#e8fbff")
    tiles[idx] = t

DRONE = [
    "................",
    "..kk........kk..",
    ".kPPk......kPPk.",
    "..kkkkkkkkkkkk..",
    "......kRRk......",
    "....kkRRRRkk....",
    "...kRRwwRRRRk...",
    "...kRRwkRRRRk...",
    "...kRRRRRRRRk...",
    "....kkRRRRkk....",
    "......kkkk......",
    "................",
    "................",
    "................",
    "................",
    "................",
]
DRONE_B = list(DRONE)
DRONE_B[1] = ".kkk........kkk."
DRONE_B[2] = "kPPPk......kPPPk"
DPAL = {"k": "#0b0a12", "P": "#9b93b0", "R": "#c75c5c", "w": "#ffe08a"}
for idx, rows in ((46, DRONE), (47, DRONE_B)):
    t = new()
    sprite(t, rows, DPAL)
    tiles[idx] = t

# spark particle (2 frames)
t = new()
r(t, 7, 4, 2, 8, "#ffe08a")
r(t, 4, 7, 8, 2, "#ffe08a")
r(t, 7, 7, 2, 2, "#ffffff")
tiles[48] = t
t = new()
for d in range(4):
    r(t, 4 + d, 4 + d, 1, 1, "#ffe08a")
    r(t, 11 - d, 4 + d, 1, 1, "#ffe08a")
    r(t, 4 + d, 11 - d, 1, 1, "#ffe08a")
    r(t, 11 - d, 11 - d, 1, 1, "#ffe08a")
tiles[49] = t

# ---------- font: 5x7 glyphs scaled 2x into 16x16 ----------
FONT = {
    "0": ["01110", "10001", "10011", "10101", "11001", "10001", "01110"],
    "1": ["00100", "01100", "00100", "00100", "00100", "00100", "01110"],
    "2": ["01110", "10001", "00001", "00010", "00100", "01000", "11111"],
    "3": ["11110", "00001", "00001", "01110", "00001", "00001", "11110"],
    "4": ["00010", "00110", "01010", "10010", "11111", "00010", "00010"],
    "5": ["11111", "10000", "11110", "00001", "00001", "10001", "01110"],
    "6": ["00110", "01000", "10000", "11110", "10001", "10001", "01110"],
    "7": ["11111", "00001", "00010", "00100", "01000", "01000", "01000"],
    "8": ["01110", "10001", "10001", "01110", "10001", "10001", "01110"],
    "9": ["01110", "10001", "10001", "01111", "00001", "00010", "01100"],
    "A": ["01110", "10001", "10001", "11111", "10001", "10001", "10001"],
    "B": ["11110", "10001", "10001", "11110", "10001", "10001", "11110"],
    "C": ["01110", "10001", "10000", "10000", "10000", "10001", "01110"],
    "D": ["11100", "10010", "10001", "10001", "10001", "10010", "11100"],
    "E": ["11111", "10000", "10000", "11110", "10000", "10000", "11111"],
    "F": ["11111", "10000", "10000", "11110", "10000", "10000", "10000"],
    "G": ["01110", "10001", "10000", "10111", "10001", "10001", "01111"],
    "H": ["10001", "10001", "10001", "11111", "10001", "10001", "10001"],
    "I": ["01110", "00100", "00100", "00100", "00100", "00100", "01110"],
    "J": ["00111", "00010", "00010", "00010", "00010", "10010", "01100"],
    "K": ["10001", "10010", "10100", "11000", "10100", "10010", "10001"],
    "L": ["10000", "10000", "10000", "10000", "10000", "10000", "11111"],
    "M": ["10001", "11011", "10101", "10101", "10001", "10001", "10001"],
    "N": ["10001", "10001", "11001", "10101", "10011", "10001", "10001"],
    "O": ["01110", "10001", "10001", "10001", "10001", "10001", "01110"],
    "P": ["11110", "10001", "10001", "11110", "10000", "10000", "10000"],
    "Q": ["01110", "10001", "10001", "10001", "10101", "10010", "01101"],
    "R": ["11110", "10001", "10001", "11110", "10100", "10010", "10001"],
    "S": ["01111", "10000", "10000", "01110", "00001", "00001", "11110"],
    "T": ["11111", "00100", "00100", "00100", "00100", "00100", "00100"],
    "U": ["10001", "10001", "10001", "10001", "10001", "10001", "01110"],
    "V": ["10001", "10001", "10001", "10001", "10001", "01010", "00100"],
    "W": ["10001", "10001", "10001", "10101", "10101", "10101", "01010"],
    "X": ["10001", "10001", "01010", "00100", "01010", "10001", "10001"],
    "Y": ["10001", "10001", "10001", "01010", "00100", "00100", "00100"],
    "Z": ["11111", "00001", "00010", "00100", "01000", "10000", "11111"],
    "/": ["00001", "00010", "00010", "00100", "01000", "01000", "10000"],
    "!": ["00100", "00100", "00100", "00100", "00100", "00000", "00100"],
    "-": ["00000", "00000", "00000", "11111", "00000", "00000", "00000"],
    ":": ["00000", "00100", "00100", "00000", "00100", "00100", "00000"],
}
FONT_FG, FONT_SH, FONT_BG = "#9be0c9", "#000001", "#0b0a12"


def glyph(rows):
    t = new()
    r(t, 0, 0, T, T, FONT_BG)
    for j, row in enumerate(rows):
        for i, b in enumerate(row):
            if b == "1":
                r(t, 3 + i * 2 + 1, 1 + j * 2 + 1, 2, 2, "#1d4d40")
    for j, row in enumerate(rows):
        for i, b in enumerate(row):
            if b == "1":
                r(t, 3 + i * 2, 1 + j * 2, 2, 2, FONT_FG)
    return t


FONT_BASE = 64  # "0".."9" = 64..73, "A".."Z" = 74..99, "/" 100, "!" 101, "-" 102, ":" 103
for k, ch in enumerate("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ/!-:"):
    tiles[FONT_BASE + k] = glyph(FONT[ch])

# ---------- write GIF ----------
palette = [(0, 0, 0)]
lookup = {(0, 0, 0): 0}
img = Image.new("P", (COLS * T, ROWS * T), 0)
px = img.load()
for idx, t in tiles.items():
    ox, oy = (idx % COLS) * T, (idx // COLS) * T
    for j in range(T):
        for i in range(T):
            c = t[j][i]
            if c is None:
                continue
            if c == (0, 0, 0):
                c = (0, 0, 1)  # keep index 0 reserved for transparency
            if c not in lookup:
                lookup[c] = len(palette)
                palette.append(c)
            px[ox + i, oy + j] = lookup[c]
assert len(palette) <= 256, len(palette)
flat = [v for c in palette for v in c]
flat += [0] * (768 - len(flat))
img.putpalette(flat)
out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "assets", "tileset.gif")
img.save(out)
# animated tiles, raw palette indices in tile order: conveyor left, conveyor right, wind
anim = bytearray()
for idx in (50, 51, 54):
    ox, oy = (idx % COLS) * T, (idx // COLS) * T
    anim += bytes(px[ox + i, oy + j] for j in range(T) for i in range(T))
open(os.path.join(os.path.dirname(out), "anim.bin"), "wb").write(anim)
print("wrote", os.path.normpath(out), img.size, len(palette), "colors", len(tiles), "tiles")

# Shared SNES art, owned locally so the game remains standalone.
from snes_glyphs import install as install_snes_glyphs
install_snes_glyphs(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), COLS * ROWS, FONT_BASE)
