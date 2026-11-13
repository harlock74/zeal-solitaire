#include <stdint.h>

#include <zvb_gfx.h>
#include <zvb_sprite.h>
#include <zgdk.h>

#include "main.h"
#include "app_state.h"
#include "assets.h"
#include "render.h"

/*
 * ZGDK's arena guard leaves one spare slot. A dragged king run uses 12
 * covered rows plus one full 3x4 card: 16 rows * 3 sprites.
 */
#define DRAG_STACK_SPRITES (CARD_STACK_SPRITE_MAX_ROWS * CARD_TILE_W)
#define SPRITE_ARENA_CAPACITY (2 + DRAG_STACK_SPRITES + 1)
#define CURSOR_HOTSPOT_X 9
#define CURSOR_HOTSPOT_Y 4
#define TILE_PIXELS 16

static gfx_sprite sprite_arena[SPRITE_ARENA_CAPACITY];
static gfx_sprite* cursor_sprite;
static gfx_sprite* selected_marker_sprite;
static gfx_sprite* drag_stack_sprites[DRAG_STACK_SPRITES];
static uint8_t drag_stack_sprite_count;
/* Includes slots that may still be visible after a partial upload. */
static uint8_t upload_extent;

static void hide_sprite(gfx_sprite* sprite)
{
    sprite->x = 0;
    sprite->y = 0;
    sprite->tile = EMPTY_TILE;
}

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

    hud_tilemap_nput_s(&vctx, text, len, x0, y);
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
    gfx_sprite sprite = {
        .y = 0,
        .x = 0,
        .tile = EMPTY_TILE,
        .flags = SPRITE_NONE,
        .options = SPRITE_OPTION_NONE,
    };

    gfx_error err = sprites_clear(&vctx);
    if (err != GFX_SUCCESS) {
        return err;
    }
    drag_stack_sprite_count = 0;
    upload_extent = 0;

    if (sprites_register_arena(sprite_arena, SPRITE_ARENA_CAPACITY) != GFX_SUCCESS) {
        return GFX_FAILURE;
    }

    cursor_sprite = sprites_register(sprite);
    selected_marker_sprite = sprites_register(sprite);
    if (cursor_sprite == NULL || selected_marker_sprite == NULL) {
        return GFX_FAILURE;
    }

    for (uint8_t i = 0; i < DRAG_STACK_SPRITES; i++) {
        drag_stack_sprites[i] = sprites_register(sprite);
        if (drag_stack_sprites[i] == NULL) {
            return GFX_FAILURE;
        }
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

    if (!visible) {
        hide_sprite(selected_marker_sprite);
        return;
    }

    selected_marker_sprite->tile = HAND_CLOSED_SELECTOR_TILE;
    selected_marker_sprite->x = sprite_pos_from_tile(tile_x);
    selected_marker_sprite->y = sprite_pos_from_tile(tile_y);
}

void render_set_drag_stack(
    uint16_t pixel_x,
    uint16_t pixel_y,
    uint8_t rows,
    const uint8_t grid[CARD_STACK_SPRITE_MAX_ROWS][CARD_TILE_W])
{
    uint8_t sprite_index = 0;

    uint8_t count = (uint8_t)(rows * CARD_TILE_W);
    for (uint8_t i = count; i < drag_stack_sprite_count; i++) {
        hide_sprite(drag_stack_sprites[i]);
    }
    drag_stack_sprite_count = count;
    for (uint8_t row = 0; row < rows; row++) {
        for (uint8_t col = 0; col < CARD_TILE_W; col++) {
            gfx_sprite* sprite = drag_stack_sprites[sprite_index];

            sprite->tile = grid[row][col];
            sprite->x = (uint16_t)(pixel_x + ((uint16_t)col * TILE_PIXELS) + TILE_PIXELS);
            sprite->y = (uint16_t)(pixel_y + ((uint16_t)row * TILE_PIXELS) + TILE_PIXELS);
            sprite_index++;
        }
    }
}

void render_move_drag_stack(uint16_t pixel_x, uint16_t pixel_y)
{
    uint8_t sprite_index = 0;
    uint8_t rows = (uint8_t)(drag_stack_sprite_count / CARD_TILE_W);

    for (uint8_t row = 0; row < rows; row++) {
        for (uint8_t col = 0; col < CARD_TILE_W; col++) {
            gfx_sprite* sprite = drag_stack_sprites[sprite_index];

            sprite->x = (uint16_t)(pixel_x + ((uint16_t)col * TILE_PIXELS) + TILE_PIXELS);
            sprite->y = (uint16_t)(pixel_y + ((uint16_t)row * TILE_PIXELS) + TILE_PIXELS);
            sprite_index++;
        }
    }
}

void render_clear_drag_stack(void)
{
    for (uint8_t i = 0; i < drag_stack_sprite_count; i++) {
        hide_sprite(drag_stack_sprites[i]);
    }
    drag_stack_sprite_count = FLAG_OFF;
}

void render_draw_sprites(void)
{
    uint8_t active_count = drag_stack_sprite_count ?
        (uint8_t)(2 + drag_stack_sprite_count) :
        (selected_marker_sprite->tile != EMPTY_TILE ? 2 : 1);

    /* Retire the old tail once before reducing the rendered prefix. Keep the
     * extent on failure, including growth that may have reached hardware.
     */
    if (active_count > upload_extent) {
        upload_extent = active_count;
    }
    if (sprites_render_range(&vctx, 0, upload_extent) != GFX_SUCCESS) {
        return;
    }
    upload_extent = active_count;
}
