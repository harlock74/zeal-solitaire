#pragma once

#include <stdint.h>
#include <zvb_gfx.h>

#define SCREEN_TILE_W 40
#define SCREEN_TILE_H 30

#define LAYER_BG 0
#define LAYER_UI 1

/* Runtime transparent tile injected by load_cards_tileset(). */
#define EMPTY_TILE 255
#define SHARED_SCRATCH_BUF_SIZE 4096
/* Keep syscall scratch buffer inside one 16KB virtual page. */
#define G_BUF_ADDR 0xA000

/* Tiled tile IDs requested for the moving hand and picked-up-card marker. */
#define HAND_SELECTOR_TILE 79
#define HAND_CLOSED_SELECTOR_TILE 125
/* Direct tileset IDs used by ZGDK nprint_string() text rendering. */
#define FONT_ALPHA_A_TILE 80
#define FONT_DIGIT_TILE 115
#define FONT_EXCL_TILE 78

/* Shared card-grid dimensions used by assets/render/gameplay modules. */
#define CARD_TILE_W 3
#define CARD_TILE_H 4
#define CARD_RANK_COUNT 13
#define CARD_SUIT_COUNT 4

/* Solitaire setup constants (non-magic, easy to tweak in one place). */
#define SOLITAIRE_DECK_SIZE 52
#define SOLITAIRE_TOP_ROW_PILES 6
#define SOLITAIRE_TABLEAU_COLS 7
/* One tableau pile can temporarily hold its initial stack plus a full king run. */
#define SOLITAIRE_TABLEAU_MAX_ROWS 19
#define SOLITAIRE_FOUNDATION_PILES 4
#define SOLITAIRE_TABLEAU_START_Y 7
#define SOLITAIRE_DEAL_ANIM_FRAME_DELAY 3

/* Generic runtime flags/limits. */
#define FLAG_OFF 0
#define FLAG_ON 1
#define U8_MAX_VALUE 255

void init(void);
void deinit(void);
void update(void);
void draw(void);
