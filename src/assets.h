#ifndef CIRCUIT_COURIER_ASSETS_H
#define CIRCUIT_COURIER_ASSETS_H
#include <stdint.h>
#include <zos_errors.h>
#include <zvb_gfx.h>
#include <zar.h>

/* Open before loading assets; close on exit. */
extern zar_file_t pack;

/* From the asset archive, which must be open (zar_file_open(PACK_FILE, &pack), PACK_FILE from cmake/assets.cmake).
 * Call both before the first audio_music(): they borrow its pattern buffers. */
zos_err_t load_palette(gfx_context* ctx);
zos_err_t load_tileset(gfx_context* ctx, gfx_tileset_options* options);

#endif
