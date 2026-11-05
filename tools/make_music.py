#!/usr/bin/env python3
"""Compose Circuit Courier's music as Zeal Music Tracker (.zmt) files.

Writes assets/bgm.zmt (looping theme) and assets/win.zmt (one-shot jingle). Both open and
play in ZMT (`exec zmt.bin H:/bgm.zmt`) for editing; the game embeds them and plays them
with ZGDK's tracker (zmt_rom_load / zmt_tick).

Voices: 1 lead (square), 2 bass (triangle), 3 drums (noise). Voice 4 is left empty so the
game can use it for sound effects.

Step strings: one token per 16th-note step, 32 steps per pattern.
  C4, F#5, ...  note on      -  hold      .  note off
Drum strings use K (kick), S (snare), h (hat), . off, - hold.
"""
import os, struct

NOTE_OFF = 108
SEMI = {"C": 0, "C#": 1, "D": 2, "D#": 3, "E": 4, "F": 5, "F#": 6, "G": 7, "G#": 8, "A": 9, "A#": 10, "B": 11}
WAVE_SQUARE, WAVE_TRIANGLE, WAVE_SAW, WAVE_NOISE = 0, 1, 2, 3
FX_VOL = {25: 0xF2, 50: 0xF4, 75: 0xF6, 100: 0xF8}
DRUMS = {"K": 20, "S": 62, "h": 100}   # noise pitch per hit


def note(tok):
    return SEMI[tok[:-1]] + 12 * int(tok[-1])


def parse(text, drum=False):
    toks = text.replace("|", " ").split()
    assert len(toks) == 32, (len(toks), text)
    steps = []
    for t in toks:
        if t == "-":
            steps.append(None)
        elif t == ".":
            steps.append(NOTE_OFF)
        else:
            steps.append(DRUMS[t] if drum else note(t))
    return steps


def voice(notes, wave, vol):
    """32 note cells -> {step: [note, wave, fx1, fx2]} with the wave + volume on step 0"""
    cells = {}
    for i, n in enumerate(notes):
        if n is not None:
            cells[i] = [n, None, None, None]
    first = cells.setdefault(0, [None, None, None, None])
    first[1] = wave
    first[2] = FX_VOL[vol]
    return cells


def stacc(text):
    """8th-note bass: 'A2 A2 A3 A2 ...' (16 per pattern) -> note on even steps, off on odd"""
    toks = text.replace("|", " ").split()
    assert len(toks) == 16, (len(toks), text)
    out = []
    for t in toks:
        out += [t, "."] if t != "-" else ["-", "-"]
    return " ".join(out)


def pattern_bytes(voices):
    out = bytearray()
    bitmap = 0
    for v, cells in enumerate(voices):
        if cells:
            bitmap |= 1 << v
    out.append(bitmap)
    for cells in voices:
        if not cells:
            continue
        header = 0
        for s in cells:
            header |= 1 << s
        out += struct.pack("<I", header)
        for s in sorted(cells):
            c = cells[s]
            h = 0
            for k in range(4):
                if c[k] is not None:
                    h |= 1 << k
            out.append(h)
            out += bytes(x for x in c if x is not None)
    return bytes(out)


def track(title, tempo, patterns, arrangement):
    assert tempo % 4 == 0 and 4 <= tempo <= 128
    out = bytearray(b"ZMT\x00")
    out += title.encode().ljust(12, b"\x00")[:12]
    out += bytes([tempo, len(patterns)])
    bm = 0
    for i in range(len(arrangement)):
        bm |= 1 << i
    out += struct.pack("<Q", bm)
    for pat, fx in arrangement:
        out += bytes([pat, fx])
    for p in patterns:
        out += pattern_bytes(p)
    return bytes(out)


# ---------------------------------------------------------------- theme: "Bus Run" (A minor)
DRUM_A = "K . h . S . h . K . K . S . h h | K . h . S . h . K . K . S . S S"
DRUM_B = "K . h . S . h . K . K . S . h h | K . h . S . h . K . h . S S S S"
BASS = {
    "Am_F": stacc("A2 A2 A3 A2 A2 A3 A2 G2 | F2 F2 F3 F2 F2 F3 F2 E2"),
    "C_G":  stacc("C3 C3 C4 C3 C3 C4 C3 B2 | G2 G2 G3 G2 G2 G3 G2 B2"),
    "Am_E": stacc("A2 A2 A3 A2 A2 A3 A2 G2 | E2 E2 E3 E2 E2 E3 G#2 B2"),
}
LEAD = {
    "rest": ". - - - - - - - - - - - - - - - | - - - - - - - - - - - - - - - -",
    "A1":   "A4 - C5 - E5 - . A5 G5 - E5 - D5 - C5 . | F4 - A4 - C5 - . F5 E5 - C5 - A4 - C5 .",
    "B1":   "E5 - G5 - C6 - . G5 F5 - E5 - D5 - C5 . | D5 - G4 - B4 - D5 - G5 - - - F5 - D5 .",
    "A2":   "A5 - - G5 E5 - - D5 C5 - D5 - E5 - . . | B4 - - D5 E5 - G#4 - B4 - E5 - - - . .",
    "arp":  "A5 . E5 . C5 . E5 . A5 . E5 . C5 . B4 . | A5 . F5 . C5 . F5 . A5 . F5 . C6 - . .",
}


def pat(lead, bass, drums, lead_vol=75):
    return [
        voice(parse(LEAD[lead]), WAVE_SQUARE, lead_vol),
        voice(parse(BASS[bass]), WAVE_TRIANGLE, 100),
        voice(parse(drums, drum=True), WAVE_NOISE, 25),
        {},
    ]


bgm_patterns = [
    pat("rest", "Am_F", DRUM_A),   # 0 groove
    pat("rest", "C_G", DRUM_B),    # 1 groove
    pat("A1", "Am_F", DRUM_A),     # 2 theme
    pat("B1", "C_G", DRUM_B),      # 3 answer
    pat("A2", "Am_E", DRUM_B),     # 4 turnaround
    pat("arp", "Am_F", DRUM_A),    # 5 arpeggio break
]
NO_FX = 0xFF
bgm_arr = [(p, NO_FX) for p in (0, 1, 2, 3, 2, 4, 5, 1, 5, 4)]

# ---------------------------------------------------------------- win jingle (C major, one shot)
win_patterns = [[
    voice(parse("C5 - E5 - G5 - C6 - - - G5 - C6 - - - | - - - - - - - - . - - - - - - -"), WAVE_SQUARE, 75),
    voice(parse("C3 - - - E3 - - - G3 - - - C4 - - - | - - - - - - - - . - - - - - - -"), WAVE_TRIANGLE, 100),
    voice(parse("K . h . K . h . S . . . S S S S | S . - - - - - - - - - - - - - -", drum=True), WAVE_NOISE, 25),
    {},
]]
win_arr = [(0, NO_FX)]

here = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "assets")
for name, data in (("bgm.zmt", track("Bus Run", 28, bgm_patterns, bgm_arr)),
                   ("win.zmt", track("Online", 20, win_patterns, win_arr))):
    with open(os.path.join(here, name), "wb") as f:
        f.write(data)
    print("wrote", name, len(data), "bytes")
