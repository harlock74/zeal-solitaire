#pragma once

#include <stdint.h>
#include <zos_errors.h>
#include <zvb_gfx.h>
#include "main.h"

gfx_error load_cards_palette(gfx_context* ctx);
gfx_error load_cards_tileset(gfx_context* ctx);
void assets_build_card_tile_grid(uint8_t grid[CARD_TILE_H][CARD_TILE_W], uint8_t card);
void assets_build_back_tile_grid(uint8_t grid[CARD_TILE_H][CARD_TILE_W]);
uint8_t assets_get_layout_tile(uint8_t x, uint8_t y);
