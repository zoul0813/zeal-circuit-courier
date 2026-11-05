#include "room.h"

room_exit room_exit_for(const body_t *body) {
    int16_t cx = PHYSICS_FP_INT(body->x) + (body->w >> 1);
    int16_t cy = PHYSICS_FP_INT(body->y) + (body->h >> 1);
    if (cx < 0) return ROOM_EXIT_LEFT;
    if (cx >= ROOM_WIDTH * 16) return ROOM_EXIT_RIGHT;
    if (cy < 0) return ROOM_EXIT_UP;
    if (cy >= ROOM_HEIGHT * 16) return ROOM_EXIT_DOWN;
    return ROOM_EXIT_NONE;
}

void room_wrap_body(body_t *body, room_exit exit) {
    int16_t px = PHYSICS_FP_INT(body->x);
    int16_t py = PHYSICS_FP_INT(body->y);
    switch (exit) {
        case ROOM_EXIT_LEFT:  px = (ROOM_WIDTH * 16) - (body->w >> 1) - 1; break;
        case ROOM_EXIT_RIGHT: px = -(int16_t)(body->w >> 1); break;
        case ROOM_EXIT_UP:    py = (ROOM_HEIGHT * 16) - (body->h >> 1) - 1; break;
        case ROOM_EXIT_DOWN:  py = -(int16_t)(body->h >> 1); break;
        default: return;
    }
    body_set_pos(body, px, py);
}
