/* HUD on layer 1, row 0, using the ZGDK ascii map */
#include <stdint.h>
#include <core.h>
#include <zvb_gfx.h>
/* individual ZGDK headers: <zgdk.h> pulls in non-inline statics (rect.h, music.h) that SDCC copies into every file */
#include "zgdk/types.h"
#include "zgdk/ascii.h"
#include "zgdk/tilemap.h"
#include "tiles.h"
#include "game.h"

#define HUD_COLS 16   // columns 16..19 of row 0 stay free for vertical room exits

static const char *const ZONE_NAMES[ZONE_COUNT] = {
    "ZONE 1  BUS     ",
    "ZONE 2  DMA     ",
    "ZONE 3  COOLING ",
    "CPU CORE        ",
};

void hud_init(void) {
    mem_set(ASCII_MAP, T_HUD, sizeof(ASCII_MAP));
    ascii_map(' ', 1, T_HUD);
    ascii_map('0', 10, T_FONT_DIGITS);
    ascii_map('A', 26, T_FONT_ALPHA);
    ascii_map('/', 1, T_FONT_SLASH);
    ascii_map('!', 1, T_FONT_BANG);
    ascii_map('-', 1, T_FONT_DASH);
    ascii_map(':', 1, T_FONT_COLON);
}

void hud_print(uint8_t x, uint8_t y, const char *text) {
    uint8_t line[ROOM_WIDTH];
    uint8_t n = 0;
    while (text[n] && x + n < ROOM_WIDTH) {
        line[n] = ascii_to(text[n]);
        n++;
    }
    gfx_tilemap_load(&vctx, line, n, LAYER1, x, y);
}

void hud_clear_row(uint8_t y) {
    tilemap_fill(&vctx, LAYER1, T_HUD, 0, y, ROOM_WIDTH, 1);
}

static void put2(char *text, uint8_t v) {
    text[0] = '0' + v / 10;
    text[1] = '0' + v % 10;
}

void format_time(char *text, uint16_t seconds) {
    put2(text, (uint8_t)(seconds / 60));
    text[2] = ':';
    put2(text + 3, (uint8_t)(seconds % 60));
}

void hud_draw(uint8_t zone, uint8_t room, uint8_t filled, uint8_t total) {
    char text[] = "0-0 B00/00 ";
    text[0] = '1' + zone;
    text[2] = '0' + room;
    put2(text + 5, filled);
    put2(text + 8, total);
    hud_print(0, 0, text);
    hud_time(game_seconds);
}

void hud_time(uint16_t seconds) {
    char text[6];
    format_time(text, seconds);
    text[5] = 0;
    hud_print(11, 0, text);
}

void hud_zone(uint8_t zone) {
    hud_print(0, 0, ZONE_NAMES[zone]);
}
