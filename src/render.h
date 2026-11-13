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
gfx_error render_init_sprites(void);
void render_set_cursor_pixel(uint16_t hotspot_x, uint16_t hotspot_y);
void render_set_cursor_visible(uint8_t visible);
void render_set_selected_marker_tile(uint8_t tile_x, uint8_t tile_y, uint8_t visible);
void render_set_drag_stack(
    uint16_t pixel_x,
    uint16_t pixel_y,
    uint8_t rows,
    const uint8_t grid[CARD_STACK_SPRITE_MAX_ROWS][CARD_TILE_W]);
void render_move_drag_stack(uint16_t pixel_x, uint16_t pixel_y);
void render_clear_drag_stack(void);
void render_draw_sprites(void);
