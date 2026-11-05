/* Host-only collision reference for native gameplay simulations.
 * ZGDK's production collision probes are Z80 assembly. This model uses public
 * SDK flag lookups and preserves the existing discrete movement/contact rules.
 * Validate the actual target implementation separately with ZGDK's Z80 suite.
 * This file is never part of a game or SDK library build.
 */
#ifdef __SDCC
#error Host collision reference must not be compiled into a target game
#endif
#include <zgdk/physics/tilemap.h>
#include <zgdk/tilemap.h>

static uint8_t touch_mask = 0xff;

static uint8_t probe_row(int16_t y, int16_t left, int16_t right) {
    uint8_t flags = 0, outside = tileflags_at(-1, 0);
    int16_t x = left >> 4, last = right >> 4;
    if (y < 0 || (y >> 4) >= _TILEMAP->rect.h || (y >> 4) >= tileflags_row_capacity)
        return outside;
    if (left < 0) { flags = outside; x = 0; }
    if (last >= _TILEMAP->rect.w) { flags |= outside; last = _TILEMAP->rect.w - 1; }
    for (; x <= last; x++) flags |= tileflags_at_tile((uint8_t)x, (uint8_t)(y >> 4));
    return flags;
}

static uint8_t probe_column(int16_t x, int16_t top, int16_t bottom) {
    uint8_t flags = 0, outside = tileflags_at(-1, 0);
    int16_t y = top >> 4, last = bottom >> 4;
    int16_t height = _TILEMAP->rect.h < tileflags_row_capacity ? _TILEMAP->rect.h : tileflags_row_capacity;
    if (x < 0 || (x >> 4) >= _TILEMAP->rect.w) return outside;
    if (top < 0) { flags = outside; y = 0; }
    if (last >= height) { flags |= outside; last = height - 1; }
    for (; y <= last; y++) flags |= tileflags_at_tile((uint8_t)(x >> 4), (uint8_t)y);
    return flags;
}

void body_set_touch_mask(uint8_t mask) { touch_mask = mask; }

void body_step(body_t *body) {
    int16_t left, top, right, bottom, old_bottom, cx, cy;
    uint8_t flags, center;
    body->state &= BODY_PASS_ONEWAY;
    body->touched = 0;
    if (_TILEMAP == NULL || _tf_flags == NULL) {
        body_integrate(body);
        return;
    }
    if (body->vx) {
        body->x += body->vx;
        left = body_px(body); top = body_py(body);
        right = left + body->w - 1; bottom = top + body->h - 1;
        if (body->vx > 0 && (probe_column(right, top, bottom) & TF_SOLID)) {
            body->x = PHYSICS_FP((right & ~15) - body->w);
            body->vx = 0;
            body->state |= BODY_HIT_WALL;
        } else if (body->vx < 0 && (probe_column(left, top, bottom) & TF_SOLID)) {
            body->x = PHYSICS_FP((left & ~15) + 16);
            body->vx = 0;
            body->state |= BODY_HIT_WALL;
        }
    }
    old_bottom = body_py(body) + body->h - 1;
    if (body->vy) {
        body->y += body->vy;
        left = body_px(body); top = body_py(body);
        right = left + body->w - 1; bottom = top + body->h - 1;
        if (body->vy > 0) {
            flags = probe_row(bottom, left, right);
            if ((flags & TF_SOLID) || ((flags & TF_ONEWAY) && !(body->state & BODY_PASS_ONEWAY)
                && (old_bottom >> 4) < (bottom >> 4))) {
                body->y = PHYSICS_FP((bottom & ~15) - body->h);
                body->vy = 0;
                body->state |= BODY_ON_GROUND;
            }
        } else if (probe_row(top, left, right) & TF_SOLID) {
            body->y = PHYSICS_FP((top & ~15) + 16);
            body->vy = 0;
            body->state |= BODY_HIT_CEIL;
        }
    }
    bottom = body_py(body) + body->h;
    if (body->vy >= 0 && !(body->state & BODY_ON_GROUND) && !(bottom & 15)) {
        flags = probe_row(bottom, body_px(body), body_px(body) + body->w - 1);
        if ((flags & TF_SOLID) || ((flags & TF_ONEWAY) && !(body->state & BODY_PASS_ONEWAY))) {
            body->y = PHYSICS_FP(bottom - body->h);
            body->vy = 0;
            body->state |= BODY_ON_GROUND;
        }
    }
    if (touch_mask) {
        left = body_px(body); top = body_py(body);
        right = left + body->w - 1; bottom = top + body->h - 1;
        cx = left + (body->w >> 1); cy = top + (body->h >> 1);
        center = probe_row(cy, cx, cx);
        body->touched = (center | probe_row(cy, left + 2, right - 2)
            | probe_column(cx, top + 2, bottom - 2)) & touch_mask;
        if (body->touched & TF_TRIGGER) body->state |= BODY_TOUCHING_TRIGGER;
        if (center & touch_mask & TF_LADDER) body->state |= BODY_ON_LADDER;
    }
}
