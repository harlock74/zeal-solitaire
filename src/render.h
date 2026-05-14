#pragma once

#include <stdint.h>
#include "main.h"

void render_table(void);
void render_place_card_grid(uint8_t x0, uint8_t y0, const uint8_t grid[CARD_TILE_H][CARD_TILE_W]);
void render_restore_tile_area(uint8_t x0, uint8_t y0, uint8_t width, uint8_t height);
void render_restore_card_area(uint8_t x0, uint8_t y0);
void render_clear_ui_layer(void);
void render_draw_text(uint8_t x0, uint8_t y, const char* text);
void render_draw_text_centered(uint8_t y, const char* text);
void render_draw_hand_selector(uint8_t x, uint8_t y);
void render_draw_closed_hand_selector(uint8_t x, uint8_t y);
void render_clear_hand_selector(uint8_t x, uint8_t y);
