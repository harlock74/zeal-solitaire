#include <stdint.h>

#include <zvb_gfx.h>
#include <zgdk.h>

#include "main.h"
#include "app_state.h"
#include "assets.h"
#include "render.h"

static uint8_t tile_coord_visible(uint8_t x, uint8_t y)
{
    return (x < SCREEN_TILE_W && y < SCREEN_TILE_H) ? FLAG_ON : FLAG_OFF;
}

void render_table(void)
{
    /*
     * Milestone 1 target:
     * draw the static Solitaire tilemap exactly as exported from Tiled.
     */
    for (uint8_t y = 0; y < SCREEN_TILE_H; y++) {
        for (uint8_t x = 0; x < SCREEN_TILE_W; x++) {
            gfx_tilemap_place(&vctx, assets_get_layout_tile(x, y), LAYER_BG, x, y);
            gfx_tilemap_place(&vctx, EMPTY_TILE, LAYER_UI, x, y);
        }
    }
}

void render_place_card_grid(uint8_t x0, uint8_t y0, const uint8_t grid[CARD_TILE_H][CARD_TILE_W])
{
    for (uint8_t row = 0; row < CARD_TILE_H; row++) {
        for (uint8_t col = 0; col < CARD_TILE_W; col++) {
            uint8_t x = (uint8_t)(x0 + col);
            uint8_t y = (uint8_t)(y0 + row);

            if (tile_coord_visible(x, y)) {
                gfx_tilemap_place(&vctx, grid[row][col], LAYER_BG, x, y);
            }
        }
    }
}

void render_restore_tile_area(uint8_t x0, uint8_t y0, uint8_t width, uint8_t height)
{
    for (uint8_t row = 0; row < height; row++) {
        for (uint8_t col = 0; col < width; col++) {
            uint8_t x = (uint8_t)(x0 + col);
            uint8_t y = (uint8_t)(y0 + row);

            if (tile_coord_visible(x, y)) {
                gfx_tilemap_place(&vctx, assets_get_layout_tile(x, y), LAYER_BG, x, y);
            }
        }
    }
}

void render_restore_card_area(uint8_t x0, uint8_t y0)
{
    render_restore_tile_area(x0, y0, CARD_TILE_W, CARD_TILE_H);
}

void render_clear_ui_layer(void)
{
    for (uint8_t y = 0; y < SCREEN_TILE_H; y++) {
        for (uint8_t x = 0; x < SCREEN_TILE_W; x++) {
            gfx_tilemap_place(&vctx, EMPTY_TILE, LAYER_UI, x, y);
        }
    }
}

void render_draw_text(uint8_t x0, uint8_t y, const char* text)
{
    uint8_t len = 0;
    const char* p = text;

    while (*p != '\0' && (uint8_t)(x0 + len) < SCREEN_TILE_W) {
        len++;
        p++;
    }

    nprint_string(&vctx, text, len, x0, y);
}

void render_draw_text_centered(uint8_t y, const char* text)
{
    uint8_t len = 0;
    const char* p = text;

    while (*p != '\0') {
        len++;
        p++;
    }

    render_draw_text((len < SCREEN_TILE_W) ? (uint8_t)((SCREEN_TILE_W - len) / 2U) : 0, y, text);
}

void render_draw_hand_selector(uint8_t x, uint8_t y)
{
    if (!tile_coord_visible(x, y)) {
        return;
    }

    /*
     * Hand selector is drawn on UI layer so it can move independently
     * from static background cards/slots.
     */
    gfx_tilemap_place(&vctx, HAND_SELECTOR_TILE, LAYER_UI, x, y);
}

void render_draw_closed_hand_selector(uint8_t x, uint8_t y)
{
    if (!tile_coord_visible(x, y)) {
        return;
    }

    /*
     * The closed hand marks the source card/run currently picked up,
     * while the normal hand remains free to show the live cursor.
     */
    gfx_tilemap_place(&vctx, HAND_CLOSED_SELECTOR_TILE, LAYER_UI, x, y);
}

void render_clear_hand_selector(uint8_t x, uint8_t y)
{
    if (!tile_coord_visible(x, y)) {
        return;
    }

    /*
     * Clear just the previous selector tile position on the UI layer.
     */
    gfx_tilemap_place(&vctx, EMPTY_TILE, LAYER_UI, x, y);
}
