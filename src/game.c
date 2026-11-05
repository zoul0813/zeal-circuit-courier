#include <zgdk/tilemap/flags.h>
#include <zgdk/physics/platformer.h>
/* Circuit Courier gameplay: bot, data bits, sockets/doors, switches/gates, conveyors, fans, drones, flip-screen rooms */
#include <stdint.h>
#include <core.h>
#include <zvb_gfx.h>
/* individual ZGDK headers: <zgdk.h> pulls in non-inline statics (rect.h, music.h) that SDCC copies into every file */
#include "zgdk/types.h"
#include "zgdk/input/button_map.h"
#include "zgdk/sprites.h"
#include "zgdk/tilemap.h"
#include <zgdk/physics/body.h>
#include <zgdk/tilemap/flags.h>
#include "room.h"
#include "tiles.h"
#include "game.h"
#include "entities.h"
#include "audio.h"
#include "assets.h"
#include "map_data.h"

TILEFLAGS_DEFINE_CACHE(15);

#define ENTITY_COUNT 16
#define NONE         0xFF
#define ROOM_NONE    0xFF  // no neighboring room in the Courier archive record

#define BOT_W   10
#define BOT_H   14
#define BIT_W   8
#define BIT_H   8

#define THROW_COOLDOWN 20
#define DEATH_FRAMES   40
#define STUN_FRAMES    180
#define SPARK_FRAMES   24

/* 12.4 fixed point, per frame at 60 Hz */
static const platformer_config_t BOT_FREE = {
    .gravity = 5, .max_fall = 96, .jump_vel = -96, .jump_cut = -32,
    .accel = 4, .decel = 6, .max_run = 32, .coyote = 5, .buffer = 6, .air_jumps = 1,
};
/* carrying a bit costs the double jump */
static const platformer_config_t BOT_CARRY = {
    .gravity = 5, .max_fall = 96, .jump_vel = -96, .jump_cut = -32,
    .accel = 4, .decel = 6, .max_run = 28, .coyote = 5, .buffer = 6, .air_jumps = 0,
};
#define BIT_GRAVITY   5
#define BIT_MAX_FALL  96
#define THROW_VX      64
#define THROW_VY      -40
#define THROW_UP_VX   0    // straight up; the bot's own run speed adds drift
#define THROW_UP_VY   -120
#define DRONE_SPEED   12
#define CLIMB_SPEED   20
#define CONVEY_SPEED  12   // 0.75 px per frame, for the bot and resting bits
#define WIND_LIFT     9    // an updraft beats gravity (5) by 4 per frame
#define WIND_MAX      48   // up to 3 px per frame
#define ZONE_FLASH    150  // frames the zone name stays in the HUD

static const uint8_t FRAMES_IDLE[]  = { T_BOT_IDLE };
static const uint8_t FRAMES_RUN[]   = { T_BOT_IDLE, T_BOT_RUN };
static const uint8_t FRAMES_JUMP[]  = { T_BOT_JUMP };
static const uint8_t FRAMES_CARRY[] = { T_BOT_CARRY };
static const uint8_t FRAMES_BIT[]   = { T_BIT, T_BIT + 1 };
static const uint8_t FRAMES_DRONE[] = { T_DRONE, T_DRONE + 1 };
static const uint8_t FRAMES_SPARK[] = { T_SPARK, T_SPARK + 1 };
static const anim_def ANIM_IDLE  = { FRAMES_IDLE, 1, 1, ANIM_LOOP };
static const anim_def ANIM_RUN   = { FRAMES_RUN, 2, 8, ANIM_LOOP };
static const anim_def ANIM_JUMP  = { FRAMES_JUMP, 1, 1, ANIM_LOOP };
static const anim_def ANIM_CARRY = { FRAMES_CARRY, 1, 1, ANIM_LOOP };
static const anim_def ANIM_BIT   = { FRAMES_BIT, 2, 16, ANIM_LOOP };
static const anim_def ANIM_DRONE = { FRAMES_DRONE, 2, 4, ANIM_LOOP };
static const anim_def ANIM_SPARK = { FRAMES_SPARK, 2, 4, ANIM_LOOP };

entity_pool_t game_entities;
static gfx_sprite sprite_arena[ENTITY_COUNT];
static game_entity_t entities[ENTITY_COUNT];
static uint8_t room_tiles[ROOM_SIZE];
static Tilemap tilemap;
static uint8_t tile_flags[256];

/* world state */
static bit_t bits[BIT_MAX];
static uint8_t bit_count;
static room_state_t rooms[ROOM_COUNT];
static uint8_t socket_total;
static snapshot_t snap;

/* current room */
static uint8_t cur_room;
static uint8_t room_zone[ROOM_COUNT];
static uint8_t room_num[ROOM_COUNT];   // 1-based within its zone
static uint8_t zone_seen;              // the HUD named every zone up to this one
static uint8_t hud_flash;              // frames left of the zone name in the HUD
static uint8_t board_base;             // the room's theme: board tiles, then the backdrop's
static uint8_t backdrop_base;

/* run stats */
uint16_t game_seconds;
uint16_t game_deaths;
static uint8_t game_ticks;

/* theme per zone: board tiles (T_BOARD_TOP..T_BOARD + 2) and backdrop tiles (T_BG..T_BG_CHIP + 3) */
#define THEME_TILE(t, n) ((uint8_t)(T_THEME + ((t) - 1) * 16 + (n)))
static const uint8_t ZONE_BOARD[ZONE_COUNT] = { T_BOARD_TOP, THEME_TILE(1, 0), THEME_TILE(2, 0), THEME_TILE(3, 0) };
static const uint8_t ZONE_BACKDROP[ZONE_COUNT] = { T_BG, THEME_TILE(1, 8), THEME_TILE(2, 8), THEME_TILE(3, 8) };

/* animated tiles: their pixels are rotated in RAM and uploaded again (assets/anim.bin, tools/make_tileset.py) */
#define ANIM_CONV 1
#define ANIM_WIND 2
static uint8_t anim_px[3][256];   // conveyor left, conveyor right, wind
static uint8_t room_anim;         // ANIM_* in the current room
static uint8_t anim_clock;
static uint8_t anim_upload;       // 1 + which tile game_draw() uploads, 0 for none

/* Native ZTM terrain remains immutable while dynamic puzzle tiles change. */
typedef struct {
    uint8_t tiles[ROOM_SIZE];
    map_objects_t meta;
} room_record_t;
static room_record_t record;
static zar_entry_t rooms_file, objects_file;
static uint8_t maps_ready;

static void read_room(uint8_t id) {
    uint16_t size = ROOM_SIZE;
    if (!maps_ready || zar_entry_read_at(&pack, &rooms_file, (uint16_t)id * ROOM_SIZE, record.tiles, &size) != ERR_SUCCESS || size != ROOM_SIZE)
        mem_set(record.tiles, T_EMPTY, ROOM_SIZE);
    size = sizeof(record.meta);
    if (!maps_ready || zar_entry_read_at(&pack, &objects_file, (uint16_t)id * sizeof(record.meta), &record.meta, &size) != ERR_SUCCESS ||
        size != sizeof(record.meta) || record.meta.count > sizeof(record.meta.objects) / sizeof(record.meta.objects[0])) {
        mem_set(&record.meta, 0, sizeof(record.meta));
        mem_set(record.meta.exits, ROOM_NONE, sizeof(record.meta.exits));
    }
}
typedef struct { uint8_t x, y, kind; } mark_t;
static mark_t marks[ROOM_MARK_MAX];
static uint8_t mark_count;
static uint8_t room_sockets;   // sockets in the current room
static uint8_t socket_seq;     // numbers sockets in scan order while loading

/* bot */
static game_entity_t *bot;
static body_t bot_body;
static platformer_state_t bot_controller;
static uint8_t carried;        // index into bits[] or NONE
static game_entity_t *carried_e;
static int8_t facing;
static uint8_t cooldown;
static uint8_t dying;

static void set_tile(uint8_t x, uint8_t y, uint8_t tile) {
    tilemap_set_xy(x, y, tile);
    tilemap_place_xy(&vctx, LAYER1, tile, x, y);
}

static uint8_t popcount(uint8_t v) {
    uint8_t n = 0;
    while (v) { n += v & 1; v >>= 1; }
    return n;
}

static uint8_t tile_at(int16_t px, int16_t py) {
    if (px < 0 || py < 0 || px >= ROOM_WIDTH * 16 || py >= ROOM_HEIGHT * 16) return T_EMPTY;
    return room_tiles[(uint8_t)(py >> 4) * ROOM_WIDTH + (uint8_t)(px >> 4)];
}

/* '<' cells are T_CONV_L, '>' cells T_CONV_R; the switch reverses them all by swapping the two tiles' pixels
 * (anim_px) instead of rewriting every cell */
static int8_t belt_push(uint8_t t) {
    if (t != T_CONV_L && t != T_CONV_R) return 0;
    return ((t == T_CONV_R) ^ rooms[cur_room].gates) ? CONVEY_SPEED : -CONVEY_SPEED;
}

/* belt push for a body resting on a conveyor: the tile under its center (the bot), or under either bottom
 * corner (`corners`, bits: a bit rides all the way off the belt's end instead of stopping on the edge) */
static int8_t conveyor_push(const body_t *b, uint8_t corners) {
    int16_t left, bottom;
    int8_t push;
    if (!(b->state & BODY_ON_GROUND)) return 0;
    left = PHYSICS_FP_INT(b->x);
    bottom = PHYSICS_FP_INT(b->y) + b->h;
    if (!corners) return belt_push(tile_at(left + (b->w >> 1), bottom));
    push = belt_push(tile_at(left, bottom));
    return push ? push : belt_push(tile_at(left + b->w - 1, bottom));
}

/* an updraft lifts a body that touches one (up to WIND_MAX, a faster jump keeps its speed) */
static void wind_lift(body_t *b) {
    if (!(b->touched & TF_USER4) || b->vy <= -WIND_MAX) return;
    b->vy -= WIND_LIFT;
    if (b->vy < -WIND_MAX) b->vy = -WIND_MAX;
}

static uint8_t sockets_filled(void) {
    uint8_t i, n = 0;
    for (i = 0; i < ROOM_COUNT; i++) n += popcount(rooms[i].sockets);
    return n;
}

static uint8_t doors_open(void) {
    return room_sockets && popcount(rooms[cur_room].sockets) == room_sockets;
}

static void add_mark(uint8_t x, uint8_t y, uint8_t kind) {
    if (mark_count >= ROOM_MARK_MAX) return;
    marks[mark_count].x = x;
    marks[mark_count].y = y;
    marks[mark_count].kind = kind;
    mark_count++;
}

static uint8_t gate_tile(uint8_t kind) {
    // MAP_GATECLOSED starts closed, MAP_GATEOPEN starts open, the switch swaps both
    uint8_t closed = (kind == MAP_GATECLOSED) ^ rooms[cur_room].gates;
    return closed ? T_GATE : T_GATE_OPEN;
}

/* MAP_FANON starts on, MAP_FANOFF starts off, the switch swaps both; MAP_SOCKETFAN runs once any socket in the room is filled */
static uint8_t fan_on(uint8_t kind) {
    if (kind == MAP_SOCKETFAN) return rooms[cur_room].sockets != 0;
    return (kind == MAP_FANON) ^ rooms[cur_room].gates;
}

/* the updraft fills the empty cells (bits and drones start in empty cells) straight above a fan; `live` redraws them, else only the tile buffer
 * is written (while build_room() fills it) */
static void draw_wind(uint8_t x, uint8_t y, uint8_t on, uint8_t live) {
    uint8_t t = on ? T_WIND : T_EMPTY;
    while (y-- > 0 && record.tiles[y * ROOM_WIDTH + x] == T_EMPTY) {
        if (live) set_tile(x, y, t);
        else room_tiles[y * ROOM_WIDTH + x] = t;
    }
}

static game_entity_t *spawn_drone(uint8_t tx, uint8_t ty) {
    game_entity_t *e = physics_entities_spawn(&game_entities, TYPE_DRONE, tx * 16 + 2, ty * 16 + 1, 12, 10);
    if (e == NULL) return NULL;
    e->entity.physics.visual.ox = -2;
    e->entity.physics.visual.oy = -1;
    e->data = (tx & 1);   // start direction varies
    animated_physics_entity_play(&e->entity, &ANIM_DRONE);
    return e;
}

static uint8_t apply_object(uint8_t kind, uint8_t tx, uint8_t ty) {
    uint8_t idx;
    switch (kind) {
        case MAP_DOOR:
            add_mark(tx, ty, MAP_DOOR);
            return doors_open() ? T_EMPTY : T_DOOR;
        case MAP_SOCKET:
            idx = socket_seq++;
            add_mark(tx, ty, idx);
            return (rooms[cur_room].sockets & (1 << idx)) ? T_SOCKET_FULL : T_SOCKET;
        case MAP_SWITCH:
            add_mark(tx, ty, MAP_SWITCH);
            return rooms[cur_room].gates ? T_SWITCH_ON : T_SWITCH;
        case MAP_GATECLOSED:
        case MAP_GATEOPEN:
            add_mark(tx, ty, kind);
            return gate_tile(kind);
        case MAP_FANON:
        case MAP_FANOFF:
        case MAP_SOCKETFAN:
            add_mark(tx, ty, kind);
            room_anim |= ANIM_WIND;
            idx = fan_on(kind);
            draw_wind(tx, ty, idx, 0);   // the rows above are built already
            return idx ? T_FAN : T_FAN_OFF;
        case MAP_DRONE:
            spawn_drone(tx, ty);
            return T_EMPTY;
        default:   // bits and the player are handled by the world scan
            return T_EMPTY;
    }
}

static void build_room(void) {
    uint16_t i;
    mem_cpy(room_tiles, record.tiles, ROOM_SIZE);
    for (i = 0; i < ROOM_SIZE; i++) {
        uint8_t t = room_tiles[i];
        if (t == T_WIND) room_anim |= ANIM_WIND;
        if (t == T_CONV_L || t == T_CONV_R) room_anim |= ANIM_CONV;
    }
    for (i = 0; i < record.meta.count; i++) {
        const map_object_t *o = &record.meta.objects[i];
        if (o->x >= ROOM_WIDTH || o->y >= ROOM_HEIGHT || o->kind < MAP_DOOR || o->kind > MAP_DRONE) continue;
        room_tiles[(uint16_t)o->y * ROOM_WIDTH + o->x] = apply_object(o->kind, o->x, o->y);
    }
    if (tilemap_load(&vctx, &tilemap, room_tiles, ROOM_WIDTH, ROOM_HEIGHT, LAYER1, 0, 0) == ERR_SUCCESS)
        tileflags_refresh();
}

/* backdrop tile by (h & 15): 0/8 horizontal trace, 1/9 vertical trace, 2 via, the rest plain; offsets from the
 * theme's T_BG */
#define BG_OF(t) ((t) - T_BG)
static const uint8_t BACKDROP[16] = {
    BG_OF(T_BG_H), BG_OF(T_BG_V), BG_OF(T_BG_X), 0, 0, 0, 0, 0,
    BG_OF(T_BG_H), BG_OF(T_BG_V), 0,             0, 0, 0, 0, 0,
};

static void draw_backdrop(void) {
    uint8_t line[ROOM_WIDTH];
    uint8_t x, y, h, hx;
    uint8_t hy = (uint8_t)(cur_room * 53);
    uint8_t cx = 3 + (cur_room * 5) % 13, cy = 3 + (cur_room * 3) % 8;
    // h = x * 37 + y * 91 + room * 53 (mod 256), stepped instead of multiplied: this runs per tile on room change
    for (y = 0; y < ROOM_HEIGHT; y++, hy += 91) {
        for (x = 0, hx = hy; x < ROOM_WIDTH; x++, hx += 37) {
            h = hx ^ (hx >> 3);
            line[x] = backdrop_base + BACKDROP[h & 15];
        }
        if (y == cy || y == cy + 1) {
            line[cx] = backdrop_base + BG_OF(T_BG_CHIP) + ((y - cy) << 1);
            line[cx + 1] = backdrop_base + BG_OF(T_BG_CHIP) + 1 + ((y - cy) << 1);
        }
        gfx_tilemap_load(&vctx, line, ROOM_WIDTH, LAYER0, 0, y);
    }
}

static game_entity_t *spawn_bit_entity(uint8_t i) {
    game_entity_t *e = physics_entities_spawn(&game_entities, TYPE_BIT, bits[i].px + 4, bits[i].py + 4, BIT_W, BIT_H);
    if (e == NULL) return NULL;
    e->entity.physics.visual.ox = -4;
    e->entity.physics.visual.oy = -4;
    e->data = i;
    animated_physics_entity_play(&e->entity, &ANIM_BIT);
    return e;
}

static void spawn_spark(int16_t px, int16_t py) {
    game_entity_t *e = physics_entities_spawn(&game_entities, TYPE_SPARK, px, py, 16, 16);
    if (e == NULL) return;
    e->timer = SPARK_FRAMES;
    animated_physics_entity_play(&e->entity, &ANIM_SPARK);
}

static void hud_update(void) {
    if (hud_flash) return;   // the zone name is showing
    hud_draw(room_zone[cur_room], room_num[cur_room], sockets_filled(), socket_total);
}

/* reverse the belts: T_CONV_L and T_CONV_R trade pixels (anim_step() uploads them within 4 frames) */
static uint8_t belts_flipped;   // anim_px[0] holds the right-running pixels

static void belts_swap(void) {
    uint8_t tmp[16], i;
    for (i = 0; i < 16; i++) {
        mem_cpy(tmp, anim_px[0] + i * 16, 16);
        mem_cpy(anim_px[0] + i * 16, anim_px[1] + i * 16, 16);
        mem_cpy(anim_px[1] + i * 16, tmp, 16);
    }
    belts_flipped ^= 1;
}

static void load_room(uint8_t id) {
    uint8_t i, zone;
    cur_room = id;
    read_room(id);
    mark_count = 0;
    room_sockets = 0;
    room_anim = 0;
    zone = room_zone[id];
    board_base = ZONE_BOARD[zone];
    backdrop_base = ZONE_BACKDROP[zone];
    entity_pool_clear(&game_entities);

    // Count sockets before doors/fans use their state.
    for (i = 0; i < record.meta.count; i++)
        if (record.meta.objects[i].kind == MAP_SOCKET) room_sockets++;

    draw_backdrop();
    socket_seq = 0;
    build_room();
    if ((room_anim & ANIM_CONV) && belts_flipped != rooms[id].gates) belts_swap();
    if (zone > zone_seen) {
        // a new zone: name it in the HUD for a while
        zone_seen = zone;
        hud_flash = ZONE_FLASH;
        hud_zone(zone);
        audio_sfx(SFX_START);
    }
    hud_update();

    bot = physics_entities_spawn(&game_entities, TYPE_BOT, 0, 0, BOT_W, BOT_H);
    bot->entity.physics.body = bot_body;
    bot->platformer = bot_controller;
    bot->entity.physics.visual.ox = -3;
    bot->entity.physics.visual.oy = -2;
    animated_physics_entity_play(&bot->entity, &ANIM_IDLE);

    carried_e = NULL;
    for (i = 0; i < bit_count; i++) {
        game_entity_t *e;
        if (bits[i].room != id || bits[i].state == BIT_SOCKETED) continue;
        e = spawn_bit_entity(i);
        if (i == carried) carried_e = e;
    }
}

static void take_snapshot(void) {
    mem_cpy(snap.bits, bits, sizeof(bits));
    mem_cpy(snap.rooms, rooms, sizeof(rooms));
    snap.carried = carried;
    snap.px = PHYSICS_FP_INT(bot_body.x);
    snap.py = PHYSICS_FP_INT(bot_body.y);
}

static void restore_snapshot(void) {
    mem_cpy(bits, snap.bits, sizeof(bits));
    mem_cpy(rooms, snap.rooms, sizeof(rooms));
    carried = snap.carried;
    platformer_set_pos(&bot_body, &bot_controller, snap.px, snap.py);
    bot_body.vx = 0;
    bot_body.vy = 0;
    bot_body.state = 0;
    load_room(cur_room);
    // entered on a ladder: keep hold of it instead of dropping down the shaft
    platformer_ladder(&bot->entity.physics.body, 0, 1, DIRECTION_NONE, 0);
}

/* advance the animated tiles: rotate one tile's pixels, game_draw() uploads it in vblank */
static void anim_step(void) {
    uint8_t which, y, *row;
    uint8_t tmp[32];
    anim_clock = (anim_clock + 1) & 3;
    anim_upload = 0;
    if (anim_clock == 3) return;
    which = anim_clock;  // 0, 1 conveyors, 2 wind
    if (which < 2 ? !(room_anim & ANIM_CONV) : !(room_anim & ANIM_WIND)) return;
    row = anim_px[which];
    if (which < 2) {
        // belt rows 0..6 move 3 px every 4 frames (45 px/s, the belt speed); the pattern repeats every 8 px
        for (y = 0; y < 7; y++, row += 16) {
            mem_cpy(tmp, row, 16);
            mem_cpy(tmp + 16, row, 16);
            mem_cpy(row, tmp + ((which ^ belts_flipped) ? 13 : 3), 16);
        }
    } else {
        // streaks rise 2 rows every 4 frames
        mem_cpy(tmp, row, 32);
        mem_cpy(row, row + 32, 256 - 32);
        mem_cpy(row + 256 - 32, tmp, 32);
    }
    anim_upload = which + 1;
}

static void enter_room(uint8_t id) {
    bot_body = bot->entity.physics.body;
    bot_controller = bot->platformer;
    if (carried != NONE) bits[carried].room = id;
    load_room(id);
    take_snapshot();
}

void game_init(void) {
    uint8_t r, x, y, i;

    mem_set(tile_flags, 0, sizeof(tile_flags));
    for (r = 0; r < ZONE_COUNT; r++) {
        for (x = 0; x < 5; x++) tile_flags[ZONE_BOARD[r] + x] = TF_SOLID;
    }
    tile_flags[T_CONV_L] = TF_SOLID;
    tile_flags[T_CONV_R] = TF_SOLID;
    tile_flags[T_FAN] = TF_SOLID;
    tile_flags[T_FAN_OFF] = TF_SOLID;
    tile_flags[T_WIND] = TF_USER4;
    tile_flags[T_ONEWAY] = TF_ONEWAY;
    tile_flags[T_LADDER] = TF_LADDER;
    tile_flags[T_LADDER_TOP] = TF_LADDER | TF_ONEWAY;
    tile_flags[T_SPIKES] = TF_TRIGGER;
    tile_flags[T_DOOR] = TF_SOLID;
    tile_flags[T_SOCKET] = TF_USER1;
    tile_flags[T_SWITCH] = TF_SOLID | TF_USER2;
    tile_flags[T_SWITCH_ON] = TF_SOLID | TF_USER2;
    tile_flags[T_GATE] = TF_SOLID;
    for (x = 0; x < 4; x++) tile_flags[T_CPU + x] = TF_USER3;
    tileflags_register(tile_flags);
    // open edges lead to the next room
    tileflags_set_outside(TF_NONE);

    sprites_register_arena(sprite_arena, ENTITY_COUNT);
    sprite_entities_init(&game_entities, entities, ENTITY_COUNT, sizeof(*entities));

    {
        uint16_t size = sizeof(anim_px);
        if (zar_entry_load(&pack, "anim.bin", anim_px, &size) != ERR_SUCCESS) room_anim = 0;
    }
    belts_flipped = 0;
    anim_upload = 0;

    // scan the world for bits, sockets, zones and the start position
    bit_count = 0;
    socket_total = 0;
    mem_set(rooms, 0, sizeof(rooms));
    platformer_init(&bot_body, &bot_controller, 32, 192, BOT_W, BOT_H);
    maps_ready = zar_entry_from_name(&pack, "rooms.ztm", &rooms_file) == ERR_SUCCESS &&
                 zar_entry_from_name(&pack, "rooms.spn", &objects_file) == ERR_SUCCESS;
    y = 0;   // rooms so far in the zone
    for (r = 0; r < ROOM_COUNT; r++) {
        read_room(r);
        if (record.meta.zone >= ZONE_COUNT) record.meta.zone = 0;
        if (r && record.meta.zone != room_zone[r - 1]) y = 0;
        room_zone[r] = record.meta.zone;
        room_num[r] = ++y;
        for (i = 0; i < record.meta.count; i++) {
            const map_object_t *o = &record.meta.objects[i];
            if (o->x >= ROOM_WIDTH || o->y >= ROOM_HEIGHT) continue;
            if (o->kind == MAP_SOCKET) socket_total++;
            else if (o->kind == MAP_PLAYER) platformer_set_pos(&bot_body, &bot_controller, o->x * 16 + 3, o->y * 16 + 2);
            else if (o->kind == MAP_BIT && bit_count < BIT_MAX) {
                bits[bit_count].room = r;
                bits[bit_count].state = BIT_FREE;
                bits[bit_count].px = o->x * 16;
                bits[bit_count].py = o->y * 16;
                bit_count++;
            }
        }
    }
    carried = NONE;
    facing = DIRECTION_RIGHT;
    cooldown = 0;
    dying = 0;
    game_seconds = 0;
    game_ticks = 0;
    game_deaths = 0;
    hud_flash = 0;
    zone_seen = room_zone[0];
    load_room(0);
    take_snapshot();
}

/* a socket was just filled: the first one starts the room's MAP_SOCKETFAN fans, the last one opens its doors */
static void power_room(uint8_t first) {
    uint8_t i, open = doors_open();
    if (open) audio_sfx(SFX_DOOR);
    for (i = 0; i < mark_count; i++) {
        if (open && marks[i].kind == MAP_DOOR) {
            set_tile(marks[i].x, marks[i].y, T_EMPTY);
            spawn_spark(marks[i].x * 16, marks[i].y * 16);
        } else if (first && marks[i].kind == MAP_SOCKETFAN) {
            set_tile(marks[i].x, marks[i].y, T_FAN);
            draw_wind(marks[i].x, marks[i].y, 1, 1);
        }
    }
}

/* put bit `i` (entity `e`) into the socket at tile tx, ty */
static void insert_bit(uint8_t i, game_entity_t *e, uint8_t tx, uint8_t ty) {
    uint8_t m, first;
    for (m = 0; m < mark_count; m++) {
        if (marks[m].x == tx && marks[m].y == ty && marks[m].kind < ROOM_MARK_MAX) break;
    }
    if (m == mark_count) return;
    first = !rooms[cur_room].sockets;
    rooms[cur_room].sockets |= 1 << marks[m].kind;
    bits[i].state = BIT_SOCKETED;
    if (carried == i) {
        carried = NONE;
        carried_e = NULL;
    }
    entity_pool_free(&game_entities, e);
    set_tile(tx, ty, T_SOCKET_FULL);
    spawn_spark(tx * 16, ty * 16);
    audio_sfx(SFX_SOCKET);   // the door sound replaces it when this powers the room
    power_room(first);
    hud_update();
    // checkpoint: a death after this keeps the socketed bit
    mem_cpy(snap.bits, bits, sizeof(bits));
    mem_cpy(snap.rooms, rooms, sizeof(rooms));
    snap.carried = carried;
}

static void flip_switch(void) {
    uint8_t i;
    rooms[cur_room].gates ^= 1;
    audio_sfx(SFX_SWITCH);
    if (room_anim & ANIM_CONV) belts_swap();
    for (i = 0; i < mark_count; i++) {
        uint8_t k = marks[i].kind;
        if (k == MAP_SWITCH) set_tile(marks[i].x, marks[i].y, rooms[cur_room].gates ? T_SWITCH_ON : T_SWITCH);
        else if (k == MAP_GATECLOSED || k == MAP_GATEOPEN) set_tile(marks[i].x, marks[i].y, gate_tile(k));
        else if (k == MAP_FANON || k == MAP_FANOFF) {
            set_tile(marks[i].x, marks[i].y, fan_on(k) ? T_FAN : T_FAN_OFF);
            draw_wind(marks[i].x, marks[i].y, fan_on(k), 1);
        }
    }
}

static void kill_bot(void) {
    if (dying) return;
    dying = DEATH_FRAMES;
    if (game_deaths < 999) game_deaths++;
    audio_sfx(SFX_DEATH);
    bot->entity.physics.visual.hidden = 1;
    spawn_spark(PHYSICS_FP_INT(bot->entity.physics.body.x) - 3, PHYSICS_FP_INT(bot->entity.physics.body.y) - 2);
}

/* Ladders: grab/climb/release is the core's platformer_ladder(); the game only centers the
 * bot on the ladder while it climbs so its head clears narrow shafts. */
static void update_ladder(body_t *b, uint16_t keys, int8_t dir) {
    int16_t cx;
    if (!platformer_ladder(b, keys & BUTTON_UP, keys & BUTTON_DOWN, dir, CLIMB_SPEED)) return;
    if (dir != DIRECTION_NONE) return;
    cx = PHYSICS_FP_INT(b->x) + (BOT_W >> 1);
    b->x = PHYSICS_FP(((cx >> 4) << 4) + 8 - (BOT_W >> 1));
}

static void update_bot(uint16_t keys, uint16_t pressed) {
    body_t *b = &bot->entity.physics.body;
    int8_t dir = DIRECTION_NONE;
    const platformer_config_t *cfg = (carried != NONE) ? &BOT_CARRY : &BOT_FREE;

    if (keys & BUTTON_LEFT) dir = DIRECTION_LEFT;
    if (keys & BUTTON_RIGHT) dir = DIRECTION_RIGHT;
    if (dir) facing = dir;

    update_ladder(b, keys, dir);

    // in an updraft the bot can always jump (once), so it can hop off the top of the column, even carrying
    if (b->touched & TF_USER4) bot->platformer.coyote = cfg->coyote;
    {
        uint8_t grounded = (b->state & (BODY_ON_GROUND | BODY_PASS_ONEWAY)) || bot->platformer.coyote;
        int8_t push = conveyor_push(b, 0);
        if (platformer_update(b, cfg, &bot->platformer, dir, keys & BUTTON_B, pressed & BUTTON_B)) audio_sfx(grounded ? SFX_JUMP : SFX_JUMP2);
        wind_lift(b);
        // the belt adds to the bot's own speed for this move only
        b->vx += push;
        platformer_move(b, cfg);
        if (push && !(b->state & BODY_HIT_WALL)) b->vx -= push;
    }

    bot->entity.physics.visual.flip = (facing < 0) ? SPRITE_FLIP_X : 0;
    if (carried != NONE) animated_physics_entity_play(&bot->entity, &ANIM_CARRY);
    else if (!(b->state & BODY_ON_GROUND) && !(b->state & BODY_PASS_ONEWAY)) animated_physics_entity_play(&bot->entity, &ANIM_JUMP);
    else if (b->vx) animated_physics_entity_play(&bot->entity, &ANIM_RUN);
    else animated_physics_entity_play(&bot->entity, &ANIM_IDLE);

    if (b->state & BODY_TOUCHING_TRIGGER) {
        kill_bot();
        return;
    }

    // carried bit rides above the bot
    if (carried_e) {
        carried_e->entity.physics.body.x = b->x + PHYSICS_FP(1);
        carried_e->entity.physics.body.y = b->y - PHYSICS_FP(9);
        carried_e->entity.physics.body.vx = 0;
        carried_e->entity.physics.body.vy = 0;
    }

    if (cooldown) cooldown--;
    if ((pressed & BUTTON_A) && carried_e) {
        game_entity_t *e = carried_e;
        bits[carried].state = BIT_FREE;
        carried = NONE;
        carried_e = NULL;
        cooldown = THROW_COOLDOWN;
        if (keys & BUTTON_DOWN) {
            // set it down at the bot's feet
            e->entity.physics.body.y = b->y + PHYSICS_FP(BOT_H - BIT_H);
        } else if (keys & BUTTON_UP) {
            e->entity.physics.body.vx = facing * THROW_UP_VX + (b->vx >> 1);
            e->entity.physics.body.vy = THROW_UP_VY;
        } else {
            e->entity.physics.body.vx = facing * THROW_VX + (b->vx >> 1);
            e->entity.physics.body.vy = THROW_VY;
        }
        e->timer = 1;   // in flight
        if (!(keys & BUTTON_DOWN)) audio_sfx(SFX_THROW);
    }

    // walking into a socket with a bit plugs it in
    if (carried_e && (b->touched & TF_USER1)) {
        int16_t cx = PHYSICS_FP_INT(b->x) + (BOT_W >> 1);
        int16_t cy = PHYSICS_FP_INT(b->y) + (BOT_H >> 1);
        if (tileflags_at(cx, cy) & TF_USER1) insert_bit(carried, carried_e, cx >> 4, cy >> 4);
    }
}

static void update_bit(game_entity_t *e) {
    body_t *b = &e->entity.physics.body;
    uint8_t i = (uint8_t)e->data;
    int16_t ovx, ovy, left, top, cx, cy;

    if (i == carried) return;

    // pick up a bit by touching it
    if (carried == NONE && !cooldown && !dying && body_overlap(b, &bot->entity.physics.body)) {
        carried = i;
        carried_e = e;
        bits[i].state = BIT_CARRIED;
        e->timer = 0;
        audio_sfx(SFX_PICKUP);
        return;
    }

    // a resting bit rides a belt
    if (!e->timer) b->vx = conveyor_push(b, 1);
    wind_lift(b);
    ovx = b->vx;
    ovy = b->vy;
    b->vy += BIT_GRAVITY;
    if (b->vy > BIT_MAX_FALL) b->vy = BIT_MAX_FALL;
    body_step(b);

    left = PHYSICS_FP_INT(b->x);
    top = PHYSICS_FP_INT(b->y);
    cx = left + (BIT_W >> 1);
    cy = top + (BIT_H >> 1);

    if (b->state & BODY_HIT_WALL) {
        int16_t px = ovx > 0 ? left + BIT_W : left - 1;
        if (e->timer && (tileflags_at(px, cy) & TF_USER2)) flip_switch();
        b->vx = -(ovx >> 1);
    }
    if (b->state & BODY_HIT_CEIL) {
        if (e->timer && (tileflags_at(cx, top - 1) & TF_USER2)) flip_switch();
        b->vy = 0;
    }
    if (b->state & BODY_ON_GROUND) {
        if (e->timer) b->vx = 0;
        e->timer = 0;
    }
    (void)ovy;

    // a thrown or dropped bit that lands in a socket plugs in
    if (tileflags_at(cx, cy) & TF_USER1) {
        insert_bit(i, e, cx >> 4, cy >> 4);
        return;
    }

    // lost on spikes or out of the room: put it back where it was when the room was entered
    if ((b->touched & TF_TRIGGER) || cx < 0 || cx >= ROOM_WIDTH * 16 || cy >= ROOM_HEIGHT * 16) {
        if (snap.bits[i].room == cur_room) {
            bits[i].px = snap.bits[i].px;
            bits[i].py = snap.bits[i].py;
        }
        body_set_pos(b, bits[i].px + 4, bits[i].py + 4);
        b->vx = 0;
        b->vy = 0;
        e->timer = 0;
        spawn_spark(bits[i].px, bits[i].py);
        audio_sfx(SFX_LOST);
        return;
    }

    bits[i].px = left - 4;
    bits[i].py = top - 4;
}

static void update_drone(game_entity_t *e) {
    body_t *b = &e->entity.physics.body;
    game_entity_t *o;
    if (e->timer) {
        // stunned: frozen and harmless, blinking
        e->timer--;
        e->entity.physics.visual.hidden = (e->timer & 4) ? 1 : 0;
        return;
    }
    e->entity.physics.visual.hidden = 0;
    b->vx = e->data ? -DRONE_SPEED : DRONE_SPEED;
    b->vy = 0;
    body_step(b);
    if ((b->state & BODY_HIT_WALL) || PHYSICS_FP_INT(b->x) < 2 || PHYSICS_FP_INT(b->x) > ROOM_WIDTH * 16 - 14) e->data ^= 1;
    e->entity.physics.visual.flip = e->data ? SPRITE_FLIP_X : 0;

    // a flying bit stuns it
    ENTITY_POOL_FOR_EACH(&game_entities, o, entity.physics.visual.type) {
        if (o->entity.physics.visual.type == TYPE_BIT && o->timer && body_overlap(&o->entity.physics.body, b)) {
            e->timer = STUN_FRAMES;
            audio_sfx(SFX_STUN);
            o->entity.physics.body.vx = -(o->entity.physics.body.vx >> 1);
            spawn_spark(PHYSICS_FP_INT(b->x), PHYSICS_FP_INT(b->y));
            return;
        }
    }
    if (!dying && body_overlap(b, &bot->entity.physics.body)) kill_bot();
}

/* @return 1 when the bot reaches the CPU */
static uint8_t update_world(uint16_t keys, uint16_t pressed) {
    game_entity_t *e;
    room_exit exit;

    if (dying) {
        if (--dying == 0) {
            restore_snapshot();
            return 0;
        }
    } else {
        // START resets the room (a way out of a stuck puzzle)
        if (pressed & BUTTON_START) {
            restore_snapshot();
            return 0;
        }
        update_bot(keys, pressed);

        exit = room_exit_for(&bot->entity.physics.body);
        if (exit != ROOM_EXIT_NONE) {
            uint8_t next = record.meta.exits[exit];   // record holds cur_room
            if (next == ROOM_NONE) {
                kill_bot();
            } else {
                room_wrap_body(&bot->entity.physics.body, exit);
                platformer_reset(&bot->platformer);
                enter_room(next);
                return 0;
            }
        }

        if (bot->entity.physics.body.touched & TF_USER3) return 1;
    }

    ENTITY_POOL_FOR_EACH(&game_entities, e, entity.physics.visual.type) {
        switch (e->entity.physics.visual.type) {
            case TYPE_BIT:   update_bit(e); break;
            case TYPE_DRONE: update_drone(e); break;
            case TYPE_SPARK:
                if (--e->timer == 0) {
                    entity_pool_free(&game_entities, e);
                    continue;
                }
                break;
        }
        if (e->entity.physics.visual.type != ENTITY_FREE) e->entity.physics.visual.sprite->tile = anim_tick(&e->entity.anim);
    }
    return 0;
}

uint8_t game_update(uint16_t keys, uint16_t pressed) {
    uint8_t won;
    if (++game_ticks == 60) {
        game_ticks = 0;
        if (game_seconds < TIME_MAX) game_seconds++;
        if (!hud_flash) hud_time(game_seconds);
    }
    if (hud_flash && --hud_flash == 0) hud_update();
    anim_step();
    won = update_world(keys, pressed);
    // always sync, so a room change never shows the old room's sprites for a frame
    physics_entities_sync(&game_entities);
    return won;
}

void game_draw(void) {
    sprites_render(&vctx);
    if (anim_upload) {
        static const uint8_t ANIM_TILES[3] = { T_CONV_L, T_CONV_R, T_WIND };
        uint8_t i = anim_upload - 1;
        gfx_tileset_load_none(&vctx, anim_px[i], 256, (uint16_t)ANIM_TILES[i] << 8, 0, 0);
        anim_upload = 0;
    }
}
