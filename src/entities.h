#ifndef GAME_ENTITIES_H
#define GAME_ENTITIES_H
#include <zgdk/entities/animated_physics.h>
#include <zgdk/physics/platformer.h>

/* Only game-owned state surrounds the reusable SDK preset. */
typedef struct {
    animated_physics_entity_t entity;
    union { platformer_state_t platformer; uint8_t user[3]; };
    uint8_t timer;
    uint16_t data;
} game_entity_t;
#ifdef __SDCC
_Static_assert(sizeof(game_entity_t) == 32, "game record layout");
#endif
extern entity_pool_t game_entities;
#endif
