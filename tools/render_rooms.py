#!/usr/bin/env python3
"""Draw the editable rooms in assets/rooms/*.tmx with assets/tileset.gif, the way src/game.c builds them (backdrop, zone
themes, updrafts, bits/drones/start as sprites). For checking room edits without the emulator, and for the README.

Usage: python3 tools/render_rooms.py [out.png] [--room N] [--scale S] [--cols C]
Default: every room in a grid, 4 per row, half size, into screenshots/rooms.png. Needs Pillow.
"""
import os
import sys
from tmx_helpers import load_maps
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
GAME = os.path.join(HERE, '..')
W, H, T = 20, 15, 16

# tile indices, as in src/tiles.h
T_EMPTY, T_BG, T_BOARD_TOP = 0, 1, 16
T_ONEWAY, T_LADDER, T_SPIKES, T_DOOR, T_SOCKET, T_SWITCH, T_GATE, T_GATE_OPEN = 21, 22, 23, 24, 25, 27, 29, 30
T_CPU, T_LADDER_TOP, T_CONV_L, T_CONV_R, T_FAN, T_FAN_OFF, T_WIND = 33, 37, 50, 51, 52, 53, 54
T_BOT, T_BIT, T_DRONE, T_THEME = 40, 44, 46, 112
ZONE_BOARD = [T_BOARD_TOP, T_THEME, T_THEME + 16, T_THEME + 32]
ZONE_BACKDROP = [T_BG, T_THEME + 8, T_THEME + 24, T_THEME + 40]
BACKDROP = [1, 2, 3, 0, 0, 0, 0, 0, 1, 2, 0, 0, 0, 0, 0, 0]
OBJECT_TILES = {'Door': T_DOOR, 'Socket': T_SOCKET, 'Switch': T_SWITCH,
                'GateClosed': T_GATE, 'GateOpen': T_GATE_OPEN, 'FanOn': T_FAN,
                'FanOff': T_FAN_OFF, 'SocketFan': T_FAN_OFF,
                'Player': T_BOT, 'Bit': T_BIT, 'Drone': T_DRONE}


def tile_img(sheet, idx):
    x, y = (idx % 16) * T, (idx // 16) * T
    return sheet.crop((x, y, x + T, y + T))


def render(sheet, template, room):
    zone = template.props["zone"]
    img = Image.new('RGBA', (W * T, H * T))
    bb, bg = ZONE_BOARD[zone], ZONE_BACKDROP[zone]
    cx, cy = 3 + (room * 5) % 13, 3 + (room * 3) % 8
    hy = (room * 53) & 255
    for y in range(H):
        hx = hy
        for x in range(W):
            h = (hx ^ (hx >> 3)) & 255
            t = bg + BACKDROP[h & 15]
            if y in (cy, cy + 1) and x in (cx, cx + 1):
                t = bg + 4 + (x - cx) + ((y - cy) << 1)
            img.paste(tile_img(sheet, t), (x * T, y * T))
            hx = (hx + 37) & 255
        hy = (hy + 91) & 255
    tiles = [g - 1 if g else 0 for g in template.gids]
    objects = template.named_objects(list(OBJECT_TILES))
    for obj in objects:
        if obj['kind'] == 'FanOn':
            x, y = obj['x'], obj['y'] - 1
            while y >= 0 and tiles[y * W + x] == T_EMPTY:
                tiles[y * W + x] = T_WIND
                y -= 1
    for y in range(H):
        for x in range(W):
            if tiles[y * W + x]:
                tile = tile_img(sheet, tiles[y * W + x])
                img.paste(tile, (x * T, y * T), tile)
    for obj in objects:
        tile = tile_img(sheet, OBJECT_TILES[obj['kind']])
        img.paste(tile, (obj['x'] * T, obj['y'] * T), tile)
    return img


def main():
    args = sys.argv[1:]
    out = os.path.join(GAME, 'screenshots', 'rooms.png')
    only, scale, cols = None, 0.5, 4
    i = 0
    while i < len(args):
        if args[i] == '--room':
            only = int(args[i + 1]); i += 1
        elif args[i] == '--scale':
            scale = float(args[i + 1]); i += 1
        elif args[i] == '--cols':
            cols = int(args[i + 1]); i += 1
        else:
            out = args[i]
        i += 1
    sheet = Image.open(os.path.join(GAME, 'assets', 'tileset.gif')).convert('RGBA')
    # palette index 0 is transparent
    src = Image.open(os.path.join(GAME, 'assets', 'tileset.gif'))
    mask = Image.eval(src.convert('L'), lambda v: 255)
    px, mp = src.load(), mask.load()
    for y in range(src.height):
        for x in range(src.width):
            if px[x, y] == 0:
                mp[x, y] = 0
    sheet.putalpha(mask)
    templates = load_maps('rooms/room[0-9][0-9].tmx', 'ROOM_COUNT', (W, H))
    rooms = [render(sheet, template, n) for n, template in enumerate(templates)]
    if only is not None:
        rooms = [rooms[only]]
        cols = 1
    rw, rh = int(W * T * scale), int(H * T * scale)
    gap = 4
    nrows = (len(rooms) + cols - 1) // cols
    sheet_out = Image.new('RGB', (cols * rw + (cols - 1) * gap, nrows * rh + (nrows - 1) * gap), (11, 10, 18))
    for k, im in enumerate(rooms):
        sheet_out.paste(im.resize((rw, rh), Image.NEAREST), ((k % cols) * (rw + gap), (k // cols) * (rh + gap)))
    sheet_out.save(out)
    print('wrote', os.path.normpath(out), sheet_out.size)


main()
