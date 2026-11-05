#ifndef CONTROLS_H
#define CONTROLS_H
#include <stdint.h>
#include "pad_glyphs.h"

extern uint8_t controls_pad;
/* Call after input_flush() on entering a pre-game menu. */
void controls_menu_begin(void);
/* Call after the menu's normal input_read(). Returns nonzero on display change.
 * New keyboard presses win ties; releases and idle pads do not change selection.
 * Never called in gameplay: the selected display remains fixed for the run. */
uint8_t controls_menu_update(void);
#define CONTROL_PROMPT(keyboard, pad) (controls_pad ? (pad) : (keyboard))
#define PROMPT_START CONTROL_PROMPT("PRESS ENTER", "PRESS " PAD_START)
#endif
