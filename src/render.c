#include <stdint.h>

#include <zvb_gfx.h>
#include <zvb_sprite.h>
#include <zgdk.h>

#include "main.h"
#include "app_state.h"
#include "assets.h"
#include "render.h"

/* A king run needs 12 covered rows plus the final full 3x4 card. */
#define DRAG_STACK_SPRITES (CARD_STACK_SPRITE_MAX_ROWS * CARD_TILE_W)
#define SPRITE_COUNT (2 + DRAG_STACK_SPRITES)
#define SPRITE_ARENA_CAPACITY (SPRITE_COUNT + 1)
#define SPRITE_UPLOAD_CHUNK 4
#define CURSOR_HOTSPOT_X 9
#define CURSOR_HOTSPOT_Y 4
#define TILE_PIXELS 16

static gfx_sprite sprite_arena[SPRITE_ARENA_CAPACITY];
static gfx_sprite* cursor_sprite;
static gfx_sprite* selected_marker_sprite;
static gfx_sprite* drag_stack_sprites[DRAG_STACK_SPRITES];
static uint8_t drag_stack_sprite_count;
static uint8_t sprite_dirty[SPRITE_COUNT];
static uint8_t first_dirty = SPRITE_COUNT;
static uint8_t end_dirty;
static uint8_t full_refresh = FLAG_ON;

static void set_sprite(gfx_sprite* sprite, uint8_t tile, uint16_t x, uint16_t y)
{
    if (sprite->tile != tile || sprite->x != x || sprite->y != y) {
        sprite->tile = tile;
        sprite->x = x;
        sprite->y = y;
        uint8_t slot = (uint8_t)(sprite - sprite_arena);
        sprite_dirty[slot] = FLAG_ON;
        if (slot < first_dirty) {
            first_dirty = slot;
        }
        if (slot >= end_dirty) {
            end_dirty = (uint8_t)(slot + 1);
        }
    }
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

void render_request_sprite_refresh(void)
{
    full_refresh = FLAG_ON;
}

void render_table(void)
{
    render_request_sprite_refresh();
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
    /* The win screen clears the gameplay overlays. */
    render_request_sprite_refresh();
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
        .y = 16,
        .x = 16,
        .tile = EMPTY_TILE,
        .flags = SPRITE_NONE,
        .options = SPRITE_OPTION_NONE,
    };

    if (sprites_register_arena(sprite_arena, SPRITE_ARENA_CAPACITY) != GFX_SUCCESS) {
        return GFX_FAILURE;
    }
    render_request_sprite_refresh();

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

    set_sprite(
        cursor_sprite,
        cursor_sprite->tile,
        sprite_x_from_hotspot(hotspot_x),
        sprite_y_from_hotspot(hotspot_y));
}

void render_set_cursor_visible(uint8_t visible)
{
    if (cursor_sprite == NULL) {
        return;
    }

    set_sprite(
        cursor_sprite,
        visible ? HAND_SELECTOR_TILE : EMPTY_TILE,
        cursor_sprite->x,
        cursor_sprite->y);
}

void render_set_selected_marker_tile(uint8_t tile_x, uint8_t tile_y, uint8_t visible)
{
    if (selected_marker_sprite == NULL) {
        return;
    }

    set_sprite(
        selected_marker_sprite,
        visible ? HAND_CLOSED_SELECTOR_TILE : EMPTY_TILE,
        sprite_pos_from_tile(tile_x),
        sprite_pos_from_tile(tile_y));
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
        gfx_sprite* sprite = drag_stack_sprites[i];
        set_sprite(sprite, EMPTY_TILE, sprite->x, sprite->y);
    }
    drag_stack_sprite_count = count;
    for (uint8_t row = 0; row < rows; row++) {
        for (uint8_t col = 0; col < CARD_TILE_W; col++) {
            set_sprite(
                drag_stack_sprites[sprite_index],
                grid[row][col],
                (uint16_t)(pixel_x + ((uint16_t)col * TILE_PIXELS) + TILE_PIXELS),
                (uint16_t)(pixel_y + ((uint16_t)row * TILE_PIXELS) + TILE_PIXELS));
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

            set_sprite(
                sprite,
                sprite->tile,
                (uint16_t)(pixel_x + ((uint16_t)col * TILE_PIXELS) + TILE_PIXELS),
                (uint16_t)(pixel_y + ((uint16_t)row * TILE_PIXELS) + TILE_PIXELS));
            sprite_index++;
        }
    }
}

void render_clear_drag_stack(void)
{
    for (uint8_t i = 0; i < drag_stack_sprite_count; i++) {
        gfx_sprite* sprite = drag_stack_sprites[i];

        set_sprite(sprite, EMPTY_TILE, sprite->x, sprite->y);
    }
    drag_stack_sprite_count = FLAG_OFF;
}

static gfx_error hide_unregistered_sprites(void)
{
    /* Zero coordinates hide a hardware slot even if it was a tall sprite. */
    static const gfx_sprite hidden[SPRITE_UPLOAD_CHUNK] = {{0}};
    for (uint8_t slot = SPRITE_COUNT; slot < GFX_SPRITES_COUNT;) {
        uint8_t count = (uint8_t)(GFX_SPRITES_COUNT - slot);
        if (count > SPRITE_UPLOAD_CHUNK) {
            count = SPRITE_UPLOAD_CHUNK;
        }
        gfx_error err = gfx_sprite_render_array(&vctx, slot, hidden, count);
        if (err != GFX_SUCCESS) {
            return err;
        }
        slot += count;
    }
    return GFX_SUCCESS;
}

void render_draw_sprites(void)
{
    if (full_refresh) {
        if (sprites_render(&vctx) != GFX_SUCCESS ||
            hide_unregistered_sprites() != GFX_SUCCESS) {
            return;
        }
        full_refresh = FLAG_OFF;
        for (uint8_t slot = first_dirty; slot < end_dirty; slot++) {
            sprite_dirty[slot] = FLAG_OFF;
        }
        first_dirty = SPRITE_COUNT;
        end_dirty = 0;
        return;
    }

    while (first_dirty < end_dirty) {
        uint8_t slot = first_dirty;
        if (!sprite_dirty[slot]) {
            first_dirty++;
            continue;
        }

        /* The SDK masks interrupts while VRAM is mapped. Bound each copy. */
        uint8_t count = 1;
        while (count < SPRITE_UPLOAD_CHUNK && slot + count < end_dirty &&
               sprite_dirty[slot + count]) {
            count++;
        }
        if (gfx_sprite_render_array(&vctx, slot, &sprite_arena[slot], count) != GFX_SUCCESS) {
            return;
        }
        do {
            sprite_dirty[first_dirty++] = FLAG_OFF;
        } while (--count);
    }
    first_dirty = SPRITE_COUNT;
    end_dirty = 0;
}
