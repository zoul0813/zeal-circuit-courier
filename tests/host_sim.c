#include <zgdk/tilemap/flags.h>
/*
 * Host-side simulation of Circuit Courier (cc, no emulator): the real src/game.c, core physics and rooms
 * (generated rooms.ztm/rooms.spn, the file courier.zar is built from) run frame by frame on a replay script; video, sound
 * and the HUD are stubs. Follows main.c's title/play loop, so a script that wins here wins the REPLAY build.
 *
 * Usage: tests/run_sim.sh [script file] [-t from:to] [-m]
 *   prints room changes, sockets, deaths and the win frame;
 *   -t traces the bot every frame in that frame range; -m draws the room as text at the end (B bot, o bits, d drones)
 * The script is the replay format of tools/replay.py (KEYS*FRAMES tokens), default tools/solution.txt.
 */
/* ---- stubs for the parts that touch hardware (ZOS headers before stdio.h: zos_vfs.h declares SEEK_SET as an enum) */
#include <zvb_gfx.h>
#include "zgdk/tilemap.h"
#include "zgdk/sprites.h"
#include "assets.h"
#include "../src/entities.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

Tilemap *_TILEMAP;
gfx_context vctx;
static gfx_sprite *arena_ptr;
static uint8_t arena_used;
static FILE *rooms_fp, *objects_fp;
static long sim_frame;

void *mem_cpy(void *d, const void *s, size_t n) { return memmove(d, s, n); }
void *mem_set(void *p, uint8_t v, size_t n) { return memset(p, v, n); }
gfx_error gfx_tilemap_load(gfx_context *c, void *t, uint8_t n, uint8_t l, uint8_t x, uint8_t y) { return GFX_SUCCESS; }
gfx_error gfx_tileset_load_none(gfx_context *c, uint8_t *t, uint16_t n, uint16_t f, uint8_t p, uint8_t o) { return GFX_SUCCESS; }
zos_err_t tilemap_register(Tilemap *t, uint8_t *tiles) { _TILEMAP = t; t->tiles = tiles; return ERR_SUCCESS; }
int16_t tilemap_set_xy(uint8_t x, uint8_t y, uint8_t v) { _TILEMAP->tiles[y * _TILEMAP->rect.w + x] = v; return v; }
gfx_error tilemap_place_xy(gfx_context *c, uint8_t l, uint8_t t, uint8_t x, uint8_t y) { return GFX_SUCCESS; }
gfx_error sprites_register_arena(gfx_sprite *a, uint8_t n) { arena_ptr = a; arena_used = 0; return GFX_SUCCESS; }
gfx_sprite *sprites_register(gfx_sprite s) { arena_ptr[arena_used] = s; return &arena_ptr[arena_used++]; }
gfx_error sprites_render(gfx_context *c) { return GFX_SUCCESS; }
/* Hardware rendering is not simulated here; SDK preset_check.py tests the real ASM. */
void physics_entities_sync(const entity_pool_t *pool) { (void)pool; }
zos_err_t zar_entry_from_name(zar_file_t *z, const char *name, zar_entry_t *e) {
    e->position = !strcmp(name, "rooms.spn");
    return ERR_SUCCESS;
}
zar_file_t pack;
zos_err_t zar_entry_read_at(zar_file_t *z, zar_entry_t *e, uint16_t off, void *buf, uint16_t *size) {
    FILE *f = e->position ? objects_fp : rooms_fp;
    fseek(f, off, SEEK_SET);
    *size = (uint16_t)fread(buf, 1, *size, f);
    return ERR_SUCCESS;
}
zos_err_t zar_entry_load(zar_file_t *z, const char *name, void *buf, uint16_t *size) { *size = 0; return ERR_SUCCESS; }
void audio_sfx(uint8_t sfx) {}
void hud_draw(uint8_t zone, uint8_t room, uint8_t filled, uint8_t total) {}
void hud_time(uint16_t seconds) {}
void hud_zone(uint8_t zone) { printf("%6ld  zone %d\n", sim_frame, zone + 1); }

#include "../src/game.c"


/* ---- replay script */
static uint16_t script[4096][2];
static int script_len;

static void parse(const char *text) {
    const char *p = text;
    while (*p) {
        uint16_t mask = 0;
        while (*p == ' ' || *p == '\n' || *p == '\t') p++;
        if (!*p) break;
        if (*p == '#') { while (*p && *p != '\n') p++; continue; }   // comment to the end of the line
        for (; *p && *p != '*' && *p != ' ' && *p != '\n'; p++) {
            switch (*p) {
                case 'B': mask |= BUTTON_B; break;
                case 'S': mask |= BUTTON_START; break;
                case 'U': mask |= BUTTON_UP; break;
                case 'D': mask |= BUTTON_DOWN; break;
                case 'L': mask |= BUTTON_LEFT; break;
                case 'R': mask |= BUTTON_RIGHT; break;
                case 'A': mask |= BUTTON_A; break;
                case '_': break;
                default: fprintf(stderr, "bad key %c\n", *p); exit(2);
            }
        }
        script[script_len][0] = 1;
        if (*p == '*') script[script_len][0] = (uint16_t)strtol(p + 1, (char **)&p, 10);
        script[script_len][1] = mask;
        script_len++;
    }
}

static void draw_map(void) {
    char map[ROOM_HEIGHT][ROOM_WIDTH + 1];
    game_entity_t *e;
    uint8_t x, y;
    for (y = 0; y < ROOM_HEIGHT; y++) {
        for (x = 0; x < ROOM_WIDTH; x++) {
            uint8_t t = room_tiles[y * ROOM_WIDTH + x], f = tile_flags[t];
            char c = (f & TF_SOLID) ? '#' : '.';
            if (t == T_WIND) c = '|';
            if (t == T_SOCKET) c = 'S';
            if (t == T_SOCKET_FULL) c = '*';
            if (t == T_LADDER || t == T_LADDER_TOP) c = 'H';
            if (t == T_ONEWAY) c = '=';
            if (t == T_SPIKES) c = '^';
            map[y][x] = c;
        }
        map[y][ROOM_WIDTH] = 0;
    }
    ENTITY_POOL_FOR_EACH(&game_entities, e, entity.physics.visual.type) {
        int cx = (PHYSICS_FP_INT(e->entity.physics.body.x) + (e->entity.physics.body.w >> 1)) >> 4, cy = (PHYSICS_FP_INT(e->entity.physics.body.y) + (e->entity.physics.body.h >> 1)) >> 4;
        if (cx < 0 || cy < 0 || cx >= ROOM_WIDTH || cy >= ROOM_HEIGHT) continue;
        if (e->entity.physics.visual.type == TYPE_BOT) map[cy][cx] = 'B';
        else if (e->entity.physics.visual.type == TYPE_BIT && map[cy][cx] != 'B') map[cy][cx] = 'o';
        else if (e->entity.physics.visual.type == TYPE_DRONE) map[cy][cx] = 'd';
    }
    printf("room %d:\n", cur_room);
    for (y = 0; y < ROOM_HEIGHT; y++) printf("  %2d %s\n", y, map[y]);
}

int main(int argc, char **argv) {
    const char *path = "tools/solution.txt";
    long trace_from = -1, trace_to = -1;
    int map = 0, i, step = 0, left = 0, state = 0;
    uint16_t keys, last = 0, pressed;
    uint8_t prev_room = 0, prev_filled = 0, prev_dying = 0;
    static char text[65536];
    FILE *f;

    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-t") && i + 1 < argc) sscanf(argv[++i], "%ld:%ld", &trace_from, &trace_to);
        else if (!strcmp(argv[i], "-m")) map = 1;
        else path = argv[i];
    }
    f = fopen(path, "r");
    if (!f) { perror(path); return 2; }
    text[fread(text, 1, sizeof(text) - 1, f)] = 0;
    fclose(f);
    parse(text);
    rooms_fp = fopen(MAPS_DIR "/rooms.ztm", "rb");
    objects_fp = fopen(MAPS_DIR "/rooms.spn", "rb");
    if (!rooms_fp || !objects_fp) { perror("generated maps"); return 2; }

    game_init();
    for (sim_frame = 0;; sim_frame++) {
        // replay_read() in main.c
        if (left == 0) {
            if (step >= script_len) break;
            left = script[step][0];
            step++;
        }
        left--;
        keys = script[step - 1][1];
        pressed = keys & ~last;
        last = keys;
        if (state == 0) {
            if (pressed & (BUTTON_START | BUTTON_B)) {
                game_init();
                prev_room = cur_room;
                state = 1;
                printf("%6ld  start\n", sim_frame);
            }
            continue;
        }
        if (state == 2) continue;
        if (game_update(keys, pressed)) {
            printf("%6ld  WIN  time %02d:%02d  deaths %d\n", sim_frame, game_seconds / 60, game_seconds % 60, game_deaths);
            state = 2;
            break;
        }
        if (cur_room != prev_room) {
            printf("%6ld  room %d -> %d  at %d,%d%s\n", sim_frame, prev_room, cur_room, PHYSICS_FP_INT(bot->entity.physics.body.x), PHYSICS_FP_INT(bot->entity.physics.body.y),
                   carried != NONE ? "  carrying" : "");
            prev_room = cur_room;
        }
        if (sockets_filled() != prev_filled) {
            prev_filled = sockets_filled();
            printf("%6ld  socket %d/%d in room %d\n", sim_frame, prev_filled, socket_total, cur_room);
        }
        if (dying && !prev_dying) printf("%6ld  DEATH in room %d at %d,%d\n", sim_frame, cur_room, PHYSICS_FP_INT(bot->entity.physics.body.x), PHYSICS_FP_INT(bot->entity.physics.body.y));
        prev_dying = dying;
        if (sim_frame >= trace_from && sim_frame <= trace_to) {
            body_t *b = &bot->entity.physics.body;
            game_entity_t *o;
            printf("%6ld  x %3d y %3d vx %4d vy %4d st %02x touch %02x%s", sim_frame, PHYSICS_FP_INT(b->x), PHYSICS_FP_INT(b->y), b->vx, b->vy,
                   b->state, b->touched, carried != NONE ? " C" : "");
            ENTITY_POOL_FOR_EACH(&game_entities, o, entity.physics.visual.type) {
                if (o->entity.physics.visual.type == TYPE_DRONE) printf("  d %d,%d%s", PHYSICS_FP_INT(o->entity.physics.body.x), PHYSICS_FP_INT(o->entity.physics.body.y), o->timer ? "s" : "");
                else if (o->entity.physics.visual.type == TYPE_BIT && o != carried_e) printf("  o %d,%d", PHYSICS_FP_INT(o->entity.physics.body.x), PHYSICS_FP_INT(o->entity.physics.body.y));
            }
            printf("\n");
        }
    }
    if (state != 2) printf("%6ld  end of script in room %d at %d,%d%s, sockets %d/%d\n", sim_frame, cur_room, PHYSICS_FP_INT(bot->entity.physics.body.x),
                           PHYSICS_FP_INT(bot->entity.physics.body.y), carried != NONE ? " carrying" : "", sockets_filled(), socket_total);
    if (map) draw_map();
    return state == 2 ? 0 : 1;
}
