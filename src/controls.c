/* Menu-only device selection. Local copy keeps this game standalone. */
#include <zgdk/input.h>
#include <zgdk/ascii.h>
#include "controls.h"

uint8_t controls_pad;
static uint16_t last_keyboard, last_pad;

void controls_menu_begin(void) {
    uint16_t pad = controller_read_pad(SNES_PORT1);
    controls_pad = controller_pad_valid(pad);
    last_pad = controls_pad ? pad : 0;
    last_keyboard = KEYBOARD_keys;
    ascii_map(PAD_GLYPH_FIRST, PAD_GLYPH_COUNT, T_PAD_B);
}

uint8_t controls_menu_update(void) {
    uint16_t keyboard = KEYBOARD_keys;
    uint16_t pad = controller_read_pad(SNES_PORT1);
    uint8_t previous = controls_pad;
    if (!controller_pad_valid(pad)) pad = 0;
    if (pad & ~last_pad) controls_pad = 1;
    if (keyboard & ~last_keyboard) controls_pad = 0;
    last_pad = pad;
    last_keyboard = keyboard;
    return controls_pad != previous;
}
