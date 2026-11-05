#include <stdint.h>

#ifndef AUDIO_H
#define AUDIO_H

/* music: ZMT tracks on voices 1-3 */
#define MUSIC_NONE  0
#define MUSIC_BGM   1
#define MUSIC_WIN   2   // one shot

/* sound effects: voice 4 */
#define SFX_JUMP     0
#define SFX_JUMP2    1
#define SFX_PICKUP   2
#define SFX_THROW    3
#define SFX_SOCKET   4
#define SFX_DOOR     5
#define SFX_SWITCH   6
#define SFX_STUN     7
#define SFX_DEATH    8
#define SFX_LOST     9
#define SFX_START    10

void audio_init(void);
void audio_deinit(void);
void audio_music(uint8_t music);
void audio_sfx(uint8_t sfx);
/* call once per frame */
void audio_tick(void);
/* RAM that is free until the first audio_music(): the tileset's load buffer */
void *audio_scratch(uint16_t *size);

#endif
