/* Room boundary/wrap behavior and SDK tile loading with the collision cache. */
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "room.h"
#include <zgdk/tilemap/flags.h>
#include <zgdk/physics/tilemap.h>
#include "zgdk/tilemap.h"

TILEFLAGS_DEFINE_CACHE(40);

static int fail_after = -1;
Tilemap *_TILEMAP;
int16_t tilemap_get_xy(uint8_t x, uint8_t y) { (void)x; (void)y; return 0; }
zos_err_t tilemap_register(Tilemap *tilemap, uint8_t *tiles) {
    _TILEMAP = tilemap;
    tilemap->tiles = tiles;
    return ERR_SUCCESS;
}
gfx_error gfx_tilemap_load(gfx_context *ctx, void *tiles, uint8_t size, uint8_t layer, uint8_t x, uint8_t y) {
    (void)ctx; (void)tiles; (void)size; (void)layer; (void)x; (void)y;
    if (fail_after == 0) return GFX_FAILURE;
    if (fail_after > 0) fail_after--;
    return GFX_SUCCESS;
}

/* Failed drawing leaves the previous map/cache active; refresh after success. */
static void check_cache_registration(void) {
    static uint8_t old[ROOM_SIZE], next_tiles[ROOM_SIZE], flags[256];
    Tilemap previous = { .rect = { .w = ROOM_WIDTH, .h = ROOM_HEIGHT } };
    Tilemap next;
    gfx_context ctx = {0};
    zos_err_t err;
    memset(old, 1, sizeof(old));
    memset(next_tiles, 2, sizeof(next_tiles));
    flags[1] = TF_USER1;
    flags[2] = TF_USER2;
    tilemap_register(&previous, old);
    tileflags_register(flags);
    tileflags_refresh();
    fail_after = 3;
    err = tilemap_load(&ctx, &next, next_tiles, ROOM_WIDTH, ROOM_HEIGHT, 0, 0, 0);
    if (err == ERR_SUCCESS) tileflags_refresh();
    assert(err == ERR_FAILURE);
    assert(_TILEMAP == &previous && tileflags_fast(0, 0) == TF_USER1);
    fail_after = -1;
    err = tilemap_load(&ctx, &next, next_tiles, ROOM_WIDTH, ROOM_HEIGHT, 0, 0, 0);
    if (err == ERR_SUCCESS) tileflags_refresh();
    assert(err == ERR_SUCCESS);
    assert(_TILEMAP == &next && tileflags_fast(0, 0) == TF_USER2);
    /* Trigger is neutral contact; damage remains a decision in each game. */
    {
        body_t body;
        flags[2] = TF_TRIGGER;
        body_init(&body, 32, 32, 12, 12);
        body_step(&body);
        assert(body.touched & TF_TRIGGER);
        assert(body.state & BODY_TOUCHING_TRIGGER);
        body_set_touch_mask(0);
        body_step(&body);
        assert(body.touched == 0 && !(body.state & BODY_TOUCHING_TRIGGER));
        body_set_touch_mask(0xff);
    }
}

static void check_wrap(void) {
    static const uint8_t sizes[][2] = { { 12, 30 }, { 16, 16 }, { 11, 17 } };
    uint8_t i, edge;
    const int16_t width = ROOM_WIDTH * 16, height = ROOM_HEIGHT * 16;
    for (i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
        body_t body, before;
        int16_t hw = sizes[i][0] >> 1, hh = sizes[i][1] >> 1;
        /* Exact center boundaries: top/left inclusive, bottom/right exclusive. */
        body_init(&body, -hw, -hh, sizes[i][0], sizes[i][1]);
        assert(room_exit_for(&body) == ROOM_EXIT_NONE);
        body.x--;  /* one fixed-point unit crosses left */
        assert(room_exit_for(&body) == ROOM_EXIT_LEFT);
        body_set_pos(&body, width - hw - 1, height - hh - 1);
        assert(room_exit_for(&body) == ROOM_EXIT_NONE);
        body.x += PHYSICS_FP_ONE;
        assert(room_exit_for(&body) == ROOM_EXIT_RIGHT);
        body_set_pos(&body, 64, -hh - 1);
        assert(room_exit_for(&body) == ROOM_EXIT_UP);
        body_set_pos(&body, 64, height - hh);
        assert(room_exit_for(&body) == ROOM_EXIT_DOWN);
        for (edge = ROOM_EXIT_LEFT; edge <= ROOM_EXIT_DOWN; edge++) {
            int16_t px = edge == ROOM_EXIT_LEFT ? -hw - 1 : edge == ROOM_EXIT_RIGHT ? width - hw : 64;
            int16_t py = edge == ROOM_EXIT_UP ? -hh - 1 : edge == ROOM_EXIT_DOWN ? height - hh : 80;
            body_set_pos(&body, px, py);
            body.vx = -37;
            body.vy = 53;
            body.state = BODY_PASS_ONEWAY | BODY_ON_GROUND;
            body.touched = TF_TRIGGER;
            assert(room_exit_for(&body) == edge);
            room_wrap_body(&body, (room_exit)edge);
            assert(room_exit_for(&body) == ROOM_EXIT_NONE);
            assert(body.vx == -37 && body.vy == 53);
            assert(body.state == 0 && body.touched == 0);
            assert(body.w == sizes[i][0] && body.h == sizes[i][1]);
            assert(body.x == PHYSICS_FP(edge == ROOM_EXIT_LEFT ? width - hw - 1 : edge == ROOM_EXIT_RIGHT ? -hw : px));
            assert(body.y == PHYSICS_FP(edge == ROOM_EXIT_UP ? height - hh - 1 : edge == ROOM_EXIT_DOWN ? -hh : py));
        }
        before = body;
        room_wrap_body(&body, ROOM_EXIT_NONE);
        assert(!memcmp(&body, &before, sizeof(body)));
    }
}

int main(void) {
    check_wrap();
    check_cache_registration();
    puts("room: wrapping and tilemap/cache integration passed");
    return 0;
}
