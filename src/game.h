#include <stdint.h>
#include <zvb_gfx.h>
#include <zgdk/physics/body.h>
#include <zgdk/tilemap/flags.h>
#include "room.h"
#include "entities.h"

#ifndef GAME_H
#define GAME_H

#define ROOM_COUNT    18
#define ZONE_COUNT    4
#define BIT_MAX       24
#define ROOM_MARK_MAX 24   // doors / gates / sockets / fans tracked per room
#define TIME_MAX      5999 // 99:59

/* entity types */
#define TYPE_BOT    0
#define TYPE_BIT    1
#define TYPE_DRONE  2
#define TYPE_SPARK  3

/* bit_t.state */
#define BIT_FREE     0
#define BIT_CARRIED  1
#define BIT_SOCKETED 2

typedef struct {
    uint8_t room;
    uint8_t state;
    int16_t px, py;   // resting position in the room, tilemap pixels
} bit_t;

/* state kept per room across visits */
typedef struct {
    uint8_t sockets;  // bit mask of filled sockets (scan order)
    uint8_t gates;    // 1 when the room's switch has been flipped
} room_state_t;

/* the parts of the world that a death or a room reset rolls back */
typedef struct {
    bit_t bits[BIT_MAX];
    room_state_t rooms[ROOM_COUNT];
    uint8_t carried;  // bit index or 0xFF
    int16_t px, py;   // where the bot entered the room
} snapshot_t;

extern gfx_context vctx;

/* run stats, for the HUD and the end screen */
extern uint16_t game_seconds;
extern uint16_t game_deaths;

void hud_init(void);
void hud_print(uint8_t x, uint8_t y, const char *text);
/* "2-3 B07/18" in columns 0..9: zone, room in the zone, sockets filled / total */
void hud_draw(uint8_t zone, uint8_t room, uint8_t filled, uint8_t total);
/* "MM:SS" in columns 11..15 */
void hud_time(uint16_t seconds);
/* the zone's name across the HUD, until the next hud_draw() */
void hud_zone(uint8_t zone);
void hud_clear_row(uint8_t y);
/* writes "MM:SS" into text (5 chars, no terminator) */
void format_time(char *text, uint16_t seconds);

void game_init(void);
/* @return 1 when the game is won */
uint8_t game_update(uint16_t keys, uint16_t pressed);
void game_draw(void);

#endif
