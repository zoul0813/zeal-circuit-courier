/* Music (ZGDK tracker playing .zmt files from courier.zar) and sound effects (zgdk/sound/sfx.h on voice 4).
 * The tracks leave voice 4 empty (see tools/make_music.py), so effects never cut the music. */
#include <stdint.h>
#include <zvb_sound.h>
/* individual ZGDK headers: <zgdk.h> pulls in non-inline statics (rect.h, music.h) that SDCC copies into every file */
#include "zgdk/sound/sounds.h"
#include "zgdk/sound/tracker.h"
#include <zgdk/sound/sfx.h>
#include "audio.h"
#include "assets.h"

/* Tracks in the asset archive. One is read into zmt_buf when it starts and parsed into the patterns by
 * zmt_rom_load() (ZGDK's device loader is private; a zmt_dev_load(track, dev) would skip zmt_buf). */
#define ZMT_MAX 1152   // the largest track, bgm.zmt, is 1104 bytes
static uint8_t zmt_buf[ZMT_MAX];

#define PATTERN_MAX 6

static track_t track;
static pattern_t patterns[PATTERN_MAX];
static uint8_t playing;
static uint8_t last_step;

/* ---------------------------------------------------------------- effects
 * Played by ZGDK's sequencer (zgdk/sound/sfx.h) on voice 4. Each segment sweeps
 * linearly from `from` to `to` Hz over `frames`. */
static const sfx_seg FX_JUMP[]   = { SFX_SEG(WAV_SQUARE, 260, 620, 6), SFX_END };
static const sfx_seg FX_JUMP2[]  = { SFX_SEG(WAV_SQUARE, 520, 1100, 6), SFX_END };
static const sfx_seg FX_PICKUP[] = { SFX_SEG(WAV_SQUARE, 880, 880, 3), SFX_SEG(WAV_SQUARE, 1320, 1320, 5), SFX_END };
static const sfx_seg FX_THROW[]  = { SFX_SEG(WAV_SAWTOOTH, 900, 300, 8), SFX_END };
static const sfx_seg FX_SOCKET[] = {
    SFX_SEG(WAV_SQUARE, 523, 523, 4), SFX_SEG(WAV_SQUARE, 659, 659, 4),
    SFX_SEG(WAV_SQUARE, 784, 784, 4), SFX_SEG(WAV_SQUARE, 1047, 1047, 8), SFX_END };
/* socket chime, then the door grinding open */
static const sfx_seg FX_DOOR[]   = {
    SFX_SEG(WAV_SQUARE, 523, 523, 4), SFX_SEG(WAV_SQUARE, 659, 659, 4),
    SFX_SEG(WAV_SQUARE, 784, 784, 4), SFX_SEG(WAV_SQUARE, 1047, 1047, 6),
    SFX_SEG(WAV_SAWTOOTH, 140, 70, 24), SFX_END };
static const sfx_seg FX_SWITCH[] = { SFX_SEG(WAV_SQUARE, 1400, 1400, 2), SFX_SEG(WAV_SQUARE, 700, 700, 4), SFX_END };
static const sfx_seg FX_STUN[]   = { SFX_SEG(WAV_SAWTOOTH, 600, 150, 12), SFX_END };
static const sfx_seg FX_DEATH[]  = { SFX_SEG(WAV_NOISE, 2000, 400, 10), SFX_SEG(WAV_SQUARE, 500, 80, 24), SFX_END };
static const sfx_seg FX_LOST[]   = { SFX_SEG(WAV_TRIANGLE, 400, 200, 10), SFX_END };
static const sfx_seg FX_START[]  = { SFX_SEG(WAV_SQUARE, 392, 392, 5), SFX_SEG(WAV_SQUARE, 784, 784, 10), SFX_END };

static const sfx_seg *const EFFECTS[] = {
    FX_JUMP, FX_JUMP2, FX_PICKUP, FX_THROW, FX_SOCKET, FX_DOOR,
    FX_SWITCH, FX_STUN, FX_DEATH, FX_LOST, FX_START,
};

/* a jump or pickup can't cut the socket / door / death sounds */
static const uint8_t PRIORITY[] = {
    1, 1, 1, 1, 2, 3,   // jump, jump2, pickup, throw, socket, door
    1, 1, 3, 1, 2,      // switch, stun, death, lost, start
};

void audio_sfx(uint8_t id) {
    sfx_play_prio(EFFECTS[id], PRIORITY[id]);
}

/* ---------------------------------------------------------------- music */
static void load(const char *name) {
    uint8_t i;
    uint16_t size = sizeof(zmt_buf);
    for (i = 0; i < PATTERN_MAX; i++) track.patterns[i] = &patterns[i];
    if (zar_entry_load(&pack, name, zmt_buf, &size) != ERR_SUCCESS ||
        zmt_rom_load(&track, zmt_buf, size) != ERR_SUCCESS) {
        playing = MUSIC_NONE;
        return;
    }
    zmt_track_reset(&track, 1);
}

/* The pattern buffers (4.6 KB) hold nothing until the first track loads, so the tileset streams
 * through them at startup */
void *audio_scratch(uint16_t *size) {
    *size = sizeof(patterns);
    return patterns;
}

void audio_init(void) {
    sound_init_hardware();
    zmt_reset(VOL_75);
    sfx_init(VOICE3, VOL_75);
    playing = MUSIC_NONE;
}

void audio_deinit(void) {
    zmt_sound_off();
    sound_deinit();
}

void audio_music(uint8_t music) {
    if (music == playing) return;
    zmt_sound_off();
    sfx_restore();   // zmt_sound_off() mutes every voice
    playing = music;
    last_step = 0;
    if (music == MUSIC_BGM) load("bgm.zmt");
    else if (music == MUSIC_WIN) load("win.zmt");
}

void audio_tick(void) {
    uint8_t step;
    if (playing != MUSIC_NONE) {
        step = zmt_tick(&track, 1);
        // a one-shot track stops when its last pattern wraps around
        if (playing == MUSIC_WIN && step < last_step) {
            zmt_sound_off();
            sfx_restore();
            playing = MUSIC_NONE;
        }
        last_step = step;
    }
    sfx_tick();
}
