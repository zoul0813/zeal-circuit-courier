/* Circuit Courier: a tiny maintenance bot carries data bits back to their sockets.
 * Built on ZGDK. */
#include <string.h>
#include <stdint.h>
#include <zos_errors.h>
#include <zos_sys.h>
#include <zos_video.h>
/* individual ZGDK headers: <zgdk.h> pulls in non-inline statics (rect.h, music.h) that SDCC copies into every file */
#include "zgdk/types.h"
#include "zgdk/input/button_map.h"
#include <zgdk/input.h>
#include "zgdk/tilemap.h"
#include <zos_sys.h>
#include <zos_vfs.h>
#include "assets.h"
#include "tiles.h"
#include "game.h"
#include "controls.h"
#include "audio.h"
#include <zgdk/sprites.h>

#define STATE_TITLE 0
#define STATE_PLAY  1
#define STATE_WIN   2

gfx_context vctx;

#ifdef REPLAY
/* frame-exact scripted input for testing, see tools/replay.py */
#include "replay.h"
static uint16_t replay_step;
static uint16_t replay_left;

static uint16_t replay_read(void) {
    if (replay_left == 0) {
        if (REPLAY_INPUT[replay_step][0] == 0) return 0;
        replay_left = REPLAY_INPUT[replay_step][0];
        replay_step++;
    }
    replay_left--;
    return REPLAY_INPUT[replay_step - 1][1];
}
#endif

/* no printf: it costs ~2.6 KB, and code + data + music must fit in 48 KB */
static void die(zos_err_t err, const char *what) {
    (void)what;
    ioctl(DEV_STDOUT, CMD_RESET_SCREEN, NULL);
    exit(err);
}

static void wait_frame(void) {
    gfx_wait_vblank(&vctx);
    game_draw();
    gfx_wait_end_vblank(&vctx);
    audio_tick();
}

static void centered(uint8_t y, const char *text) {
    hud_print(10 - (strlen(text) >> 1), y, text);
}

static void banner(const char *line1, const char *line2) {
    uint8_t y;
    for (y = 5; y <= 9; y++) hud_clear_row(y);
    centered(6, line1);
    centered(8, line2);
}

/* best time, kept next to the .bin (the game runs from its own folder, like courier.zar) */
#define SAVE_FILE "courier.sav"
static uint16_t best_seconds;   // 0 = none yet

static void best_load(void) {
    uint16_t size = sizeof(best_seconds);
    zos_dev_t dev = open(SAVE_FILE, O_RDONLY);
    best_seconds = 0;
    if (dev < 0) return;
    if (read(dev, &best_seconds, &size) != ERR_SUCCESS || size != sizeof(best_seconds) || best_seconds > TIME_MAX) best_seconds = 0;
    close(dev);
}

static void best_save(void) {
    uint16_t size = sizeof(best_seconds);
    // no O_TRUNC: the file is always 2 bytes, rewritten in place
    zos_dev_t dev = open(SAVE_FILE, O_WRONLY | O_CREAT);
    if (dev < 0) return;
    write(dev, &best_seconds, &size);
    close(dev);
}

static void title_screen(void) {
    char text[] = "BEST 00:00";
    banner("CIRCUIT COURIER", PROMPT_START);
    centered(11, CONTROL_PROMPT("Z JUMP  X THROW", PAD_B " JUMP  " PAD_A " THROW"));
    if (best_seconds) {
        format_time(text + 5, best_seconds);
        centered(9, text);
    }
}

/* rank on time plus 10 s per death; par is about the solution replay's time (tools/solution.txt) */
#define PAR_SECONDS 300
static const uint16_t RANK_LIMIT[] = { PAR_SECONDS, PAR_SECONDS * 3 / 2, PAR_SECONDS * 2, PAR_SECONDS * 3 };

static void win_screen(void) {
    char time_text[] = "TIME 00:00";
    char deaths_text[] = "CRASHES 000";
    char rank_text[] = "RANK S";
    uint16_t score = game_seconds + game_deaths * 10;
    uint8_t rank = 0, y;
    for (y = 3; y <= 12; y++) hud_clear_row(y);
    centered(4, "SYSTEM ONLINE!");
    format_time(time_text + 5, game_seconds);
    centered(6, time_text);
    deaths_text[8] = '0' + game_deaths / 100;
    deaths_text[9] = '0' + (game_deaths / 10) % 10;
    deaths_text[10] = '0' + game_deaths % 10;
    centered(7, deaths_text);
    while (rank < 4 && score > RANK_LIMIT[rank]) rank++;
    rank_text[5] = "SABCD"[rank];
    centered(9, rank_text);
    if (!best_seconds || game_seconds < best_seconds) {
        best_seconds = game_seconds;
        best_save();
        centered(10, "NEW BEST TIME!");
    }
    centered(12, PROMPT_START);
}

int main(void) {
    zos_err_t err;
    uint16_t keys, last = 0, pressed;
    uint8_t state = STATE_TITLE;

    err = input_init(1);
    if (err) die(err, "input");

    gfx_enable_screen(0);
    err = gfx_initialize(ZVB_CTRL_VID_MODE_GFX_320_8BIT, &vctx);
    if (err) die(err, "gfx");
    err = sprites_clear(&vctx);   // a previous program may have left sprites in the table
    if (err) die(err, "sprites");
    err = zar_file_open(PACK_FILE, &pack);   // courier.zar (courier_replay.zar for REPLAY), next to the .bin
    if (err) die(err, PACK_FILE);
    err = load_palette(&vctx);
    if (err) die(err, "palette");
    gfx_tileset_options options = { 0 };   // load_tileset() selects the LZ loader and advances from_byte
    err = load_tileset(&vctx, &options);
    if (err) die(err, "tileset");
    tilemap_fill(&vctx, LAYER0, T_BG, 0, 0, 80, 40);
    tilemap_fill(&vctx, LAYER1, T_EMPTY, 0, 0, 80, 40);
    hud_init();
    audio_init();

    best_load();
    game_init();
    input_flush();
    controls_menu_begin();
    title_screen();
    gfx_enable_screen(1);
    audio_music(MUSIC_BGM);

    while (1) {
#ifdef REPLAY
        keys = replay_read();
#else
        keys = input_read();
#endif
        pressed = keys & ~last;
        last = keys;
        if (keys & BUTTON_SELECT) break;

        switch (state) {
            case STATE_TITLE:
                if (controls_menu_update()) title_screen();
                if (pressed & (BUTTON_START | BUTTON_B)) {
                    game_init();
                    audio_sfx(SFX_START);
                    state = STATE_PLAY;
                }
                break;
            case STATE_PLAY:
                if (game_update(keys, pressed)) {
                    win_screen();
                    audio_music(MUSIC_WIN);
                    state = STATE_WIN;
                }
                break;
            case STATE_WIN:
                if (pressed & BUTTON_START) {
                    game_init();
                    input_flush();
                    controls_menu_begin();
                    title_screen();
                    audio_music(MUSIC_BGM);
                    state = STATE_TITLE;
                }
                break;
        }
        wait_frame();
    }

    audio_deinit();
    zar_file_close(&pack);
    (void)sprites_clear(&vctx);   // best-effort cleanup before returning to ZOS
    ioctl(DEV_STDOUT, CMD_RESET_SCREEN, NULL);
    return 0;
}
