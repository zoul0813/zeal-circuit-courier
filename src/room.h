#ifndef GAME_ROOM_H
#define GAME_ROOM_H

#include <zgdk/physics/body.h>

#define ROOM_WIDTH   20
#define ROOM_HEIGHT  15
#define ROOM_SIZE    (ROOM_WIDTH * ROOM_HEIGHT)

typedef enum {
    ROOM_EXIT_LEFT  = 0,
    ROOM_EXIT_RIGHT = 1,
    ROOM_EXIT_UP    = 2,
    ROOM_EXIT_DOWN  = 3,
    ROOM_EXIT_NONE  = 0xFF,
} room_exit;

/**
 * @brief Which edge has the body's center crossed?
 * @return room_exit or ROOM_EXIT_NONE
 */
room_exit room_exit_for(const body_t *body);

/**
 * @brief Move the body to the opposite edge after taking `exit`
 *
 * Preserves velocity; body_set_pos() clears transient movement state.
 */
void room_wrap_body(body_t *body, room_exit exit);

#endif
