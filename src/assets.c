#include <stdint.h>

#include <zos_vfs.h>
#include <zvb_gfx.h>

#include "main.h"
#include "app_state.h"
#include "assets.h"

extern uint8_t _ztm_solitaire_start;
extern uint8_t _ztm_solitaire_end;

#define CARD_TILE_COUNT (CARD_TILE_W * CARD_TILE_H)
#define CARD_COLOR_VARIANTS 2
#define CARD_TILESET_VRAM_SCALE 2

enum {
    COLOR_RED = 0,
    COLOR_BLACK = 1,
};

enum {
    RANK_ACE = 0,
    RANK_JACK = 10,
    RANK_QUEEN = 11,
    RANK_KING = 12,
};

typedef struct {
    uint8_t top;
    uint8_t mid;
    uint8_t bottom;
} FaceColumn;

typedef struct {
    uint8_t top;
    uint8_t mid_left;
    uint8_t mid_center;
    uint8_t mid_right;
    uint8_t bottom_left;
    uint8_t bottom_center;
    uint8_t bottom_right;
} QueenFace;

typedef enum {
    TOP_LEFT = 0,
    TOP_CENTRE,
    TOP_RIGHT,
    MIDDLE1_LEFT,
    MIDDLE1_CENTRE,
    MIDDLE1_RIGHT,
    MIDDLE2_LEFT,
    MIDDLE2_CENTRE,
    MIDDLE2_RIGHT,
    BOTTOM_LEFT,
    BOTTOM_CENTRE,
    BOTTOM_RIGHT,
} CardPos;

#define PIPS(r0, r1, r2, r3) \
    ((uint16_t)((r0) | ((r1) << 3) | ((r2) << 6) | ((r3) << 9)))

static const uint8_t kWhiteCardTile = 16;

enum {
    SUIT_MARKER_HEARTS = 11,
    SUIT_MARKER_DIAMONDS = 12,
    SUIT_MARKER_CLUBS = 27,
    SUIT_MARKER_SPADES = 28,
};

static const uint8_t kSuitTileBySuit[CARD_SUIT_COUNT] = {
    3,  /* Hearts */
    5,  /* Diamonds */
    6,  /* Clubs */
    8   /* Spades */
};

static const uint8_t kSuitMarkerTileBySuit[CARD_SUIT_COUNT] = {
    SUIT_MARKER_HEARTS,
    SUIT_MARKER_DIAMONDS,
    SUIT_MARKER_CLUBS,
    SUIT_MARKER_SPADES,
};

static const uint8_t kSuitColorBySuit[CARD_SUIT_COUNT] = {
    COLOR_RED,   /* Hearts */
    COLOR_RED,   /* Diamonds */
    COLOR_BLACK, /* Clubs */
    COLOR_BLACK, /* Spades */
};

static const uint8_t kRankGlyphRed[CARD_RANK_COUNT] = {
    48,49,50,51,52,53,54,55,56,57,58,59,60
};

static const uint8_t kRankGlyphBlack[CARD_RANK_COUNT] = {
    64,65,66,67,68,69,70,71,72,73,74,75,76
};

static const FaceColumn kJackFaceByColor[CARD_COLOR_VARIANTS] = {
    {1, 17, 33}, /* Red */
    {2, 18, 34}, /* Black */
};

static const FaceColumn kKingFaceByColor[CARD_COLOR_VARIANTS] = {
    {9, 25, 41},  /* Red */
    {10, 26, 42}, /* Black */
};

static const QueenFace kQueenFaceByColor[CARD_COLOR_VARIANTS] = {
    {4, 19, 20, 21, 35, 36, 37}, /* Red */
    {7, 22, 23, 24, 38, 39, 40}, /* Black */
};

static const uint8_t kBackCardTiles[CARD_TILE_H][CARD_TILE_W] = {
    {13, 14, 15},
    {29, 30, 31},
    {45, 46, 47},
    {61, 62, 63},
};

static gfx_error load_palette_from_file(gfx_context* ctx, const char* path)
{
    uint8_t from_color = 0;
    zos_dev_t dev = open(path, O_RDONLY);
    if (dev < 0) {
        return GFX_FAILURE;
    }

    while (1) {
        uint16_t size = sizeof(g_buf);
        if (read(dev, g_buf, &size) != ERR_SUCCESS) {
            close(dev);
            return GFX_FAILURE;
        }
        if (size == 0) {
            break;
        }

        if (gfx_palette_load(ctx, g_buf, size, from_color) != GFX_SUCCESS) {
            close(dev);
            return GFX_FAILURE;
        }

        from_color = (uint8_t)(from_color + (size / 2));
    }

    close(dev);
    return GFX_SUCCESS;
}

static gfx_error load_tileset_from_file(gfx_context* ctx, const char* path, uint8_t compression_mode, uint16_t vram_scale)
{
    uint16_t from_byte = 0;
    zos_dev_t dev = open(path, O_RDONLY);
    if (dev < 0) {
        return GFX_FAILURE;
    }

    while (1) {
        uint16_t size = sizeof(g_buf);
        gfx_tileset_options options = {
            .compression = compression_mode,
            .from_byte = from_byte,
            .pal_offset = 0,
            .opacity = 0,
        };

        if (read(dev, g_buf, &size) != ERR_SUCCESS) {
            close(dev);
            return GFX_FAILURE;
        }
        if (size == 0) {
            break;
        }

        if (gfx_tileset_load(ctx, g_buf, size, &options) != GFX_SUCCESS) {
            close(dev);
            return GFX_FAILURE;
        }

        from_byte = (uint16_t)(from_byte + (size * vram_scale));
    }

    close(dev);
    return GFX_SUCCESS;
}

static void init_card_grid(uint8_t grid[CARD_TILE_H][CARD_TILE_W], uint8_t tile)
{
    for (uint8_t row = 0; row < CARD_TILE_H; row++) {
        for (uint8_t col = 0; col < CARD_TILE_W; col++) {
            grid[row][col] = tile;
        }
    }
}

static void set_card_pos(uint8_t grid[CARD_TILE_H][CARD_TILE_W], CardPos pos, uint8_t tile)
{
    uint8_t row = (uint8_t)pos / CARD_TILE_W;
    uint8_t col = (uint8_t)pos % CARD_TILE_W;
    grid[row][col] = tile;
}

static void set_pips_for_rank(uint8_t grid[CARD_TILE_H][CARD_TILE_W], uint8_t rank, uint8_t suit_tile)
{
    static const uint16_t kPipMaskByRank[9] = {
        PIPS(0b000, 0b010, 0b000, 0b010), /* 2 */
        PIPS(0b000, 0b010, 0b010, 0b010), /* 3 */
        PIPS(0b000, 0b101, 0b000, 0b101), /* 4 */
        PIPS(0b000, 0b101, 0b010, 0b101), /* 5 */
        PIPS(0b000, 0b101, 0b101, 0b101), /* 6 */
        PIPS(0b000, 0b101, 0b111, 0b101), /* 7 */
        PIPS(0b000, 0b101, 0b111, 0b111), /* 8 */
        PIPS(0b000, 0b111, 0b111, 0b111), /* 9 */
        PIPS(0b010, 0b111, 0b111, 0b111), /* 10 */
    };

    if (rank == RANK_ACE) {
        set_card_pos(grid, MIDDLE2_CENTRE, suit_tile);
        return;
    }

    if (rank < 1U || rank >= 10U) {
        return;
    }

    {
        uint16_t mask = kPipMaskByRank[rank - 1U];
        for (uint8_t pos = 0; pos < CARD_TILE_COUNT; pos++) {
            if (mask & (uint16_t)(1U << pos)) {
                uint8_t row = (uint8_t)(pos / CARD_TILE_W);
                uint8_t col = (uint8_t)(pos % CARD_TILE_W);
                grid[row][col] = suit_tile;
            }
        }
    }
}

static void set_face_figure(
    uint8_t grid[CARD_TILE_H][CARD_TILE_W],
    uint8_t rank,
    uint8_t black,
    uint8_t suit_tile)
{
    uint8_t color_idx = black ? COLOR_BLACK : COLOR_RED;
    set_card_pos(grid, MIDDLE1_LEFT, suit_tile);

    if (rank == RANK_JACK) {
        const FaceColumn* face = &kJackFaceByColor[color_idx];
        set_card_pos(grid, MIDDLE1_CENTRE, face->top);
        set_card_pos(grid, MIDDLE2_CENTRE, face->mid);
        set_card_pos(grid, BOTTOM_CENTRE, face->bottom);
    } else if (rank == RANK_QUEEN) {
        const QueenFace* face = &kQueenFaceByColor[color_idx];
        set_card_pos(grid, MIDDLE1_CENTRE, face->top);
        set_card_pos(grid, MIDDLE2_LEFT, face->mid_left);
        set_card_pos(grid, MIDDLE2_CENTRE, face->mid_center);
        set_card_pos(grid, MIDDLE2_RIGHT, face->mid_right);
        set_card_pos(grid, BOTTOM_LEFT, face->bottom_left);
        set_card_pos(grid, BOTTOM_CENTRE, face->bottom_center);
        set_card_pos(grid, BOTTOM_RIGHT, face->bottom_right);
    } else if (rank == RANK_KING) {
        const FaceColumn* face = &kKingFaceByColor[color_idx];
        set_card_pos(grid, MIDDLE1_CENTRE, face->top);
        set_card_pos(grid, MIDDLE2_CENTRE, face->mid);
        set_card_pos(grid, BOTTOM_CENTRE, face->bottom);
    }
}

void assets_build_card_tile_grid(uint8_t grid[CARD_TILE_H][CARD_TILE_W], uint8_t card)
{
    uint8_t rank = (uint8_t)(card % CARD_RANK_COUNT);
    uint8_t suit = (uint8_t)((card / CARD_RANK_COUNT) % CARD_SUIT_COUNT);
    uint8_t black = (kSuitColorBySuit[suit] == COLOR_BLACK);

    init_card_grid(grid, kWhiteCardTile);
    set_card_pos(grid, TOP_LEFT, black ? kRankGlyphBlack[rank] : kRankGlyphRed[rank]);
    set_card_pos(grid, TOP_RIGHT, kSuitMarkerTileBySuit[suit]);

    if (rank < RANK_JACK) {
        set_pips_for_rank(grid, rank, kSuitTileBySuit[suit]);
    } else {
        set_face_figure(grid, rank, black, kSuitTileBySuit[suit]);
    }
}

void assets_build_back_tile_grid(uint8_t grid[CARD_TILE_H][CARD_TILE_W])
{
    for (uint8_t row = 0; row < CARD_TILE_H; row++) {
        for (uint8_t col = 0; col < CARD_TILE_W; col++) {
            grid[row][col] = kBackCardTiles[row][col];
        }
    }
}

gfx_error load_cards_palette(gfx_context* ctx)
{
    return load_palette_from_file(ctx, "assets/cards.ztp");
}

gfx_error load_cards_tileset(gfx_context* ctx)
{
    if (load_tileset_from_file(ctx, "assets/cards.zts", TILESET_COMP_4BIT, CARD_TILESET_VRAM_SCALE) != GFX_SUCCESS) {
        return GFX_FAILURE;
    }

    /* Keep one explicit transparent tile reserved for UI clears on layer 1. */
    if (gfx_tileset_add_color_tile(ctx, U8_MAX_VALUE, FLAG_OFF) != GFX_SUCCESS) {
        return GFX_FAILURE;
    }

    return GFX_SUCCESS;
}

uint8_t assets_get_layout_tile(uint8_t x, uint8_t y)
{
    const uint8_t* map = (const uint8_t*)&_ztm_solitaire_start;

    if (x >= SCREEN_TILE_W || y >= SCREEN_TILE_H) {
        return EMPTY_TILE;
    }

    return map[((uint16_t)y * SCREEN_TILE_W) + x];
}

void __assets__(void) __naked
{
    __asm__(
        "__ztm_solitaire_start:\n"
        "    .incbin \"assets/solitaire.ztm\"\n"
        "__ztm_solitaire_end:\n"
    );
}
