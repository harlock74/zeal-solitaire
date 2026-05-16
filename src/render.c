#include <stdint.h>

#include <zvb_gfx.h>
#include <zvb_sprite.h>
#include <zgdk.h>

#include "main.h"
#include "app_state.h"
#include "assets.h"
#include "render.h"

enum {
    /*
     * ZGDK's arena guard leaves one spare slot, so capacity is one larger
     * than the two sprites registered below.
     */
    SPRITE_ARENA_CAPACITY = 3,
    CURSOR_HOTSPOT_X = 9,
    CURSOR_HOTSPOT_Y = 4,
    TILE_PIXELS = 16,
};

static gfx_sprite sprite_arena[SPRITE_ARENA_CAPACITY];
static gfx_sprite* cursor_sprite;
static gfx_sprite* selected_marker_sprite;

static uint8_t tile_coord_visible(uint8_t x, uint8_t y)
{
    return (x < SCREEN_TILE_W && y < SCREEN_TILE_H) ? FLAG_ON : FLAG_OFF;
}

static uint16_t sprite_x_from_hotspot(uint16_t hotspot_x)
{
    return (uint16_t)(hotspot_x + TILE_PIXELS - CURSOR_HOTSPOT_X);
}

static uint16_t sprite_y_from_hotspot(uint16_t hotspot_y)
{
    return (uint16_t)(hotspot_y + TILE_PIXELS - CURSOR_HOTSPOT_Y);
}

static uint16_t sprite_pos_from_tile(uint8_t tile)
{
    return (uint16_t)(((uint16_t)tile * TILE_PIXELS) + TILE_PIXELS);
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

gfx_error render_init_sprites(void)
{
    gfx_sprite cursor = {
        .y = 16,
        .x = 16,
        .tile = EMPTY_TILE,
        .flags = SPRITE_NONE,
        .options = SPRITE_OPTION_NONE,
    };
    gfx_sprite selected_marker = {
        .y = 16,
        .x = 16,
        .tile = EMPTY_TILE,
        .flags = SPRITE_NONE,
        .options = SPRITE_OPTION_NONE,
    };

    if (sprites_register_arena(sprite_arena, SPRITE_ARENA_CAPACITY) != GFX_SUCCESS) {
        return GFX_FAILURE;
    }

    cursor_sprite = sprites_register(cursor);
    selected_marker_sprite = sprites_register(selected_marker);
    if (cursor_sprite == NULL || selected_marker_sprite == NULL) {
        return GFX_FAILURE;
    }

    return GFX_SUCCESS;
}

void render_set_cursor_pixel(uint16_t hotspot_x, uint16_t hotspot_y)
{
    if (cursor_sprite == NULL) {
        return;
    }

    cursor_sprite->x = sprite_x_from_hotspot(hotspot_x);
    cursor_sprite->y = sprite_y_from_hotspot(hotspot_y);
}

void render_set_cursor_visible(uint8_t visible)
{
    if (cursor_sprite == NULL) {
        return;
    }

    cursor_sprite->tile = visible ? HAND_SELECTOR_TILE : EMPTY_TILE;
}

void render_set_selected_marker_tile(uint8_t tile_x, uint8_t tile_y, uint8_t visible)
{
    if (selected_marker_sprite == NULL) {
        return;
    }

    selected_marker_sprite->tile = visible ? HAND_CLOSED_SELECTOR_TILE : EMPTY_TILE;
    selected_marker_sprite->x = sprite_pos_from_tile(tile_x);
    selected_marker_sprite->y = sprite_pos_from_tile(tile_y);
}

void render_draw_sprites(void)
{
    sprites_render(&vctx);
}
