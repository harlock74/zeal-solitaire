#include <stdint.h>

#include <zgdk/utils/rand.h>

#include "app_state.h"
#include "assets.h"
#include "render.h"
#include "solitaire.h"

enum {
    SELECTOR_ROW_TOP = FLAG_OFF,
    SELECTOR_ROW_TABLEAU = FLAG_ON,
    TOP_PILE_STOCK = FLAG_OFF,
    TOP_PILE_WASTE = FLAG_ON,
    TOP_PILE_FOUNDATION_FIRST = 2,
    CARD_FACE_DOWN = FLAG_OFF,
    CARD_FACE_UP = FLAG_ON,
    CARD_SUIT_BLACK_START = 2,
    CARD_RANK_ACE = FLAG_OFF,
    CARD_RANK_KING = (CARD_RANK_COUNT - 1),
    DEAL_WITH_ANIMATION = FLAG_ON,
    INVALID_MOVE_BLINKS = 2,
    INVALID_MOVE_BLINK_FRAMES = 4,
    SELECTOR_COL_MIN = FLAG_OFF,
    MOUSE_TILE_UNITS = 8,
    MOUSE_ROW_SPLIT_Y = 4,
    MOUSE_Y_DIRECTION = 1,
    WIN_MESSAGE_Y = 14,
    WIN_RESTART_Y = 15,
};

typedef struct {
    uint8_t card;
    uint8_t face_up;
} TableauCard;

/* Top row: stock, waste, foundation x4. */
static const uint8_t kTopRowX[SOLITAIRE_TOP_ROW_PILES] = {4, 9, 19, 24, 29, 34};
/* Left edge (x) of each 3x4 card at top-row piles. */
static const uint8_t kTopRowLeftX[SOLITAIRE_TOP_ROW_PILES] = {3, 8, 18, 23, 28, 33};
/* Bottom row: tableau columns 0..6. */
static const uint8_t kBottomRowX[SOLITAIRE_TABLEAU_COLS] = {4, 9, 14, 19, 24, 29, 34};
/* Left edge (x) of each 3x4 card at tableau columns 0..6. */
static const uint8_t kTableauLeftX[SOLITAIRE_TABLEAU_COLS] = {3, 8, 13, 18, 23, 28, 33};

static const uint8_t kTopRowCardY = 1;
static const uint8_t kTopRowY = 1;
static const uint8_t kEmptyTableauSelectorY = SOLITAIRE_TABLEAU_START_Y;
static const uint8_t kCardSelectorRowOffset = FLAG_OFF;
static const uint8_t kTableauRestoreHeight = (uint8_t)(SOLITAIRE_TABLEAU_MAX_ROWS + CARD_TILE_H - 1U);

static uint8_t selector_row = SELECTOR_ROW_TOP; /* Start on stock row. */
static uint8_t selector_col = SELECTOR_COL_MIN;
static uint8_t selected_active = FLAG_OFF;
static uint8_t selected_row = SELECTOR_ROW_TOP;
static uint8_t selected_col = SELECTOR_COL_MIN;
static uint8_t selected_depth = FLAG_OFF;
static uint8_t selected_count = FLAG_ON;
static uint8_t selected_card = FLAG_OFF;

static uint8_t deck[SOLITAIRE_DECK_SIZE];
static uint8_t deck_pos = FLAG_OFF;
static TableauCard tableau[SOLITAIRE_TABLEAU_COLS][SOLITAIRE_TABLEAU_MAX_ROWS];
static uint8_t tableau_count[SOLITAIRE_TABLEAU_COLS];
static uint8_t waste[SOLITAIRE_DECK_SIZE];
static uint8_t stock_count = FLAG_OFF;
static uint8_t waste_count = FLAG_OFF;
static uint8_t foundation_count[SOLITAIRE_FOUNDATION_PILES] = {FLAG_OFF, FLAG_OFF, FLAG_OFF, FLAG_OFF};
static uint8_t foundation_suit[SOLITAIRE_FOUNDATION_PILES] = {U8_MAX_VALUE, U8_MAX_VALUE, U8_MAX_VALUE, U8_MAX_VALUE};
static uint8_t scratch_card_grid[CARD_TILE_H][CARD_TILE_W];
static int16_t mouse_cursor_units_x = 0;
static int16_t mouse_cursor_units_y = 0;
static uint8_t mouse_tile_cursor_active = FLAG_OFF;
static uint8_t mouse_tile_cursor_x = 0;
static uint8_t mouse_tile_cursor_y = 0;
static uint8_t game_won = FLAG_OFF;

static void cancel_selection(void);
static void clear_selected_marker(void);
static void draw_selected_marker(void);
static uint8_t current_hand_x(void);
static uint8_t current_hand_y(void);
static uint8_t current_selector_x(void);
static uint8_t current_selector_y(void);
static void redraw_hand_markers(void);
static uint8_t selection_matches(uint8_t row, uint8_t col, uint8_t depth);
static void select_card(uint8_t row, uint8_t col, uint8_t depth, uint8_t count, uint8_t card);

static uint8_t rand_bounded_u8(uint8_t max_inclusive)
{
    uint8_t range = (uint8_t)(max_inclusive + FLAG_ON);
    uint8_t limit;
    uint8_t x;

    if (range == FLAG_OFF) {
        return FLAG_OFF;
    }

    limit = (uint8_t)(U8_MAX_VALUE - (U8_MAX_VALUE % range));
    do {
        x = (uint8_t)rand8();
    } while (x > limit);

    return (uint8_t)(x % range);
}

static void shuffle_deck(void)
{
    for (uint8_t i = 0; i < SOLITAIRE_DECK_SIZE; i++) {
        deck[i] = i;
    }

    for (uint8_t i = (uint8_t)(SOLITAIRE_DECK_SIZE - 1U); i > 0U; i--) {
        uint8_t j = rand_bounded_u8(i);
        uint8_t tmp = deck[i];
        deck[i] = deck[j];
        deck[j] = tmp;
    }

    deck_pos = 0;
}

static uint8_t pop_deck(void)
{
    if (deck_pos >= SOLITAIRE_DECK_SIZE) {
        shuffle_deck();
    }

    return deck[deck_pos++];
}

static void reset_pile_counters(void)
{
    waste_count = FLAG_OFF;
    for (uint8_t i = 0; i < SOLITAIRE_FOUNDATION_PILES; i++) {
        foundation_count[i] = FLAG_OFF;
        foundation_suit[i] = U8_MAX_VALUE;
    }
}

static void draw_tableau_card(uint8_t col, uint8_t depth)
{
    uint8_t x = kTableauLeftX[col];
    uint8_t y = (uint8_t)(SOLITAIRE_TABLEAU_START_Y + depth);

    if (tableau[col][depth].face_up == CARD_FACE_UP) {
        assets_build_card_tile_grid(scratch_card_grid, tableau[col][depth].card);
    } else {
        assets_build_back_tile_grid(scratch_card_grid);
    }

    render_place_card_grid(x, y, scratch_card_grid);
}

static void build_empty_top_slot_grid(uint8_t grid[CARD_TILE_H][CARD_TILE_W])
{
    uint8_t source_x = kTopRowLeftX[TOP_PILE_WASTE];

    for (uint8_t row = 0; row < CARD_TILE_H; row++) {
        for (uint8_t col = 0; col < CARD_TILE_W; col++) {
            grid[row][col] = assets_get_layout_tile(
                (uint8_t)(source_x + col),
                (uint8_t)(kTopRowCardY + row));
        }
    }
}

static void draw_top_pile(uint8_t pile, uint8_t has_card, uint8_t card, uint8_t face_up)
{
    uint8_t x = kTopRowLeftX[pile];

    if (!has_card) {
        if (pile == TOP_PILE_STOCK) {
            build_empty_top_slot_grid(scratch_card_grid);
            render_place_card_grid(x, kTopRowCardY, scratch_card_grid);
            return;
        }

        render_restore_card_area(x, kTopRowCardY);
        return;
    }

    if (face_up == CARD_FACE_UP) {
        assets_build_card_tile_grid(scratch_card_grid, card);
    } else {
        assets_build_back_tile_grid(scratch_card_grid);
    }

    render_place_card_grid(x, kTopRowCardY, scratch_card_grid);
}

static void render_stock_and_waste(void)
{
    uint8_t waste_top_card = FLAG_OFF;

    if (waste_count > FLAG_OFF) {
        waste_top_card = waste[(uint8_t)(waste_count - FLAG_ON)];
    }

    draw_top_pile(TOP_PILE_STOCK, stock_count > FLAG_OFF, FLAG_OFF, CARD_FACE_DOWN);
    draw_top_pile(TOP_PILE_WASTE, waste_count > FLAG_OFF, waste_top_card, CARD_FACE_UP);
}

static void render_foundation(uint8_t foundation)
{
    uint8_t pile = (uint8_t)(TOP_PILE_FOUNDATION_FIRST + foundation);
    uint8_t top_card = FLAG_OFF;

    if (foundation_count[foundation] > FLAG_OFF) {
        top_card = (uint8_t)(
            (foundation_suit[foundation] * CARD_RANK_COUNT) +
            (foundation_count[foundation] - FLAG_ON));
    }

    draw_top_pile(pile, foundation_count[foundation] > FLAG_OFF, top_card, CARD_FACE_UP);
}

static uint8_t card_rank(uint8_t card)
{
    return (uint8_t)(card % CARD_RANK_COUNT);
}

static uint8_t card_suit(uint8_t card)
{
    return (uint8_t)(card / CARD_RANK_COUNT);
}

static uint8_t card_is_black(uint8_t card)
{
    return (card_suit(card) >= CARD_SUIT_BLACK_START) ? FLAG_ON : FLAG_OFF;
}

static void redraw_tableau_column(uint8_t col)
{
    render_restore_tile_area(
        kTableauLeftX[col],
        SOLITAIRE_TABLEAU_START_Y,
        CARD_TILE_W,
        kTableauRestoreHeight);

    for (uint8_t depth = 0; depth < tableau_count[col]; depth++) {
        draw_tableau_card(col, depth);
    }
}

static void reveal_tableau_top_if_needed(uint8_t col)
{
    uint8_t depth;

    if (tableau_count[col] == FLAG_OFF) {
        return;
    }

    depth = (uint8_t)(tableau_count[col] - FLAG_ON);
    if (tableau[col][depth].face_up == CARD_FACE_DOWN) {
        tableau[col][depth].face_up = CARD_FACE_UP;
    }
}

static uint8_t can_place_on_tableau(uint8_t card, uint8_t dest_col)
{
    uint8_t dest_depth;
    uint8_t dest_card;

    if (tableau_count[dest_col] == FLAG_OFF) {
        return (card_rank(card) == CARD_RANK_KING) ? FLAG_ON : FLAG_OFF;
    }

    dest_depth = (uint8_t)(tableau_count[dest_col] - FLAG_ON);
    dest_card = tableau[dest_col][dest_depth].card;

    if (card_is_black(card) == card_is_black(dest_card)) {
        return FLAG_OFF;
    }

    return (card_rank(dest_card) == (uint8_t)(card_rank(card) + FLAG_ON)) ? FLAG_ON : FLAG_OFF;
}

static uint8_t can_place_on_foundation(uint8_t card, uint8_t foundation)
{
    uint8_t rank = card_rank(card);
    uint8_t suit = card_suit(card);

    if (foundation_count[foundation] == FLAG_OFF) {
        return (rank == CARD_RANK_ACE) ? FLAG_ON : FLAG_OFF;
    }

    if (foundation_suit[foundation] != suit) {
        return FLAG_OFF;
    }

    return (rank == foundation_count[foundation]) ? FLAG_ON : FLAG_OFF;
}

static uint8_t foundations_complete(void)
{
    for (uint8_t foundation = 0; foundation < SOLITAIRE_FOUNDATION_PILES; foundation++) {
        if (foundation_count[foundation] < CARD_RANK_COUNT) {
            return FLAG_OFF;
        }
    }

    return FLAG_ON;
}

static void render_win_message(void)
{
    clear_selected_marker();
    selected_active = FLAG_OFF;
    render_clear_ui_layer();
    render_draw_text_centered(WIN_MESSAGE_Y, "WELL DONE!");
    render_draw_text_centered(WIN_RESTART_Y, "PRESS ENTER TO PLAY AGAIN");
    game_won = FLAG_ON;
}

static void wait_frames(uint8_t frames)
{
    while (frames > FLAG_OFF) {
        gfx_wait_vblank(&vctx);
        gfx_wait_end_vblank(&vctx);
        frames--;
    }
}

static void show_invalid_move_feedback(void)
{
    uint8_t x = current_selector_x();
    uint8_t y = current_selector_y();

    for (uint8_t i = 0; i < INVALID_MOVE_BLINKS; i++) {
        render_clear_hand_selector(x, y);
        wait_frames(INVALID_MOVE_BLINK_FRAMES);
        redraw_hand_markers();
        wait_frames(INVALID_MOVE_BLINK_FRAMES);
    }
}

static void deal_opening_tableau(uint8_t animate)
{
    uint8_t dealt = FLAG_OFF;

    for (uint8_t col = 0; col < SOLITAIRE_TABLEAU_COLS; col++) {
        tableau_count[col] = FLAG_OFF;
    }

    /*
     * Real Klondike deal order:
     * row 0 -> columns 0..6
     * row 1 -> columns 1..6
     * ...
     * row 6 -> column 6
     */
    for (uint8_t row = 0; row < SOLITAIRE_TABLEAU_COLS; row++) {
        for (uint8_t col = row; col < SOLITAIRE_TABLEAU_COLS; col++) {
            uint8_t depth = tableau_count[col];
            uint8_t face_up = (col == row) ? CARD_FACE_UP : CARD_FACE_DOWN;

            tableau[col][depth].card = pop_deck();
            tableau[col][depth].face_up = face_up;
            tableau_count[col] = (uint8_t)(depth + FLAG_ON);
            dealt++;

            if (animate) {
                draw_tableau_card(col, depth);
                wait_frames(SOLITAIRE_DEAL_ANIM_FRAME_DELAY);
            }
        }
    }

    stock_count = (uint8_t)(SOLITAIRE_DECK_SIZE - dealt);
    reset_pile_counters();
}

static void draw_stock_to_waste(void)
{
    if (stock_count == FLAG_OFF) {
        return;
    }

    waste[waste_count] = deck[deck_pos];
    waste_count++;
    deck_pos++;
    stock_count--;
}

static void recycle_waste_to_stock(void)
{
    uint8_t first_stock_idx;

    if (waste_count == FLAG_OFF) {
        return;
    }

    first_stock_idx = (uint8_t)(SOLITAIRE_DECK_SIZE - waste_count);
    for (uint8_t i = 0; i < waste_count; i++) {
        deck[(uint8_t)(first_stock_idx + i)] = waste[i];
    }

    deck_pos = first_stock_idx;
    stock_count = waste_count;
    waste_count = FLAG_OFF;
}

static uint8_t remove_selected_source(void)
{
    if (selected_row == SELECTOR_ROW_TOP && selected_col == TOP_PILE_WASTE) {
        if (waste_count == FLAG_OFF || selected_depth != (uint8_t)(waste_count - FLAG_ON)) {
            return FLAG_OFF;
        }

        waste_count--;
        render_stock_and_waste();
        return FLAG_ON;
    }

    if (selected_row == SELECTOR_ROW_TABLEAU) {
        if (tableau_count[selected_col] == FLAG_OFF ||
            selected_count == FLAG_OFF ||
            selected_depth != (uint8_t)(tableau_count[selected_col] - selected_count)) {
            return FLAG_OFF;
        }

        tableau_count[selected_col] = (uint8_t)(tableau_count[selected_col] - selected_count);
        reveal_tableau_top_if_needed(selected_col);
        redraw_tableau_column(selected_col);
        return FLAG_ON;
    }

    return FLAG_OFF;
}

static uint8_t move_selected_to_tableau(uint8_t dest_col)
{
    uint8_t dest_depth;

    if (!selected_active ||
        (uint8_t)(tableau_count[dest_col] + selected_count) > SOLITAIRE_TABLEAU_MAX_ROWS) {
        return FLAG_OFF;
    }

    if (!can_place_on_tableau(selected_card, dest_col)) {
        return FLAG_OFF;
    }

    clear_selected_marker();
    if (!remove_selected_source()) {
        selected_active = FLAG_OFF;
        return FLAG_OFF;
    }

    dest_depth = tableau_count[dest_col];
    if (selected_row == SELECTOR_ROW_TOP) {
        tableau[dest_col][dest_depth].card = selected_card;
        tableau[dest_col][dest_depth].face_up = CARD_FACE_UP;
    } else {
        for (uint8_t i = 0; i < selected_count; i++) {
            uint8_t source_depth = (uint8_t)(selected_depth + i);
            uint8_t target_depth = (uint8_t)(dest_depth + i);
            tableau[dest_col][target_depth].card = tableau[selected_col][source_depth].card;
            tableau[dest_col][target_depth].face_up = CARD_FACE_UP;
        }
    }

    tableau_count[dest_col] = (uint8_t)(tableau_count[dest_col] + selected_count);
    redraw_tableau_column(dest_col);

    selected_active = FLAG_OFF;
    return FLAG_ON;
}

static uint8_t move_selected_to_foundation(uint8_t foundation)
{
    if (!selected_active ||
        selected_count != FLAG_ON ||
        !can_place_on_foundation(selected_card, foundation)) {
        return FLAG_OFF;
    }

    clear_selected_marker();
    if (!remove_selected_source()) {
        selected_active = FLAG_OFF;
        return FLAG_OFF;
    }

    foundation_suit[foundation] = card_suit(selected_card);
    foundation_count[foundation]++;
    render_foundation(foundation);

    selected_active = FLAG_OFF;
    if (foundations_complete()) {
        render_win_message();
    }
    return FLAG_ON;
}

static uint8_t handle_stock_action(void)
{
    cancel_selection();

    if (stock_count > FLAG_OFF) {
        draw_stock_to_waste();
    } else {
        recycle_waste_to_stock();
    }

    render_stock_and_waste();
    return FLAG_ON;
}

static uint8_t handle_waste_selection(void)
{
    uint8_t depth;
    uint8_t card;

    if (waste_count == FLAG_OFF) {
        return FLAG_OFF;
    }

    depth = (uint8_t)(waste_count - FLAG_ON);
    card = waste[depth];

    if (selection_matches(SELECTOR_ROW_TOP, TOP_PILE_WASTE, depth)) {
        cancel_selection();
        return FLAG_ON;
    }

    select_card(SELECTOR_ROW_TOP, TOP_PILE_WASTE, depth, FLAG_ON, card);
    return FLAG_ON;
}

static uint8_t first_face_up_depth(uint8_t col)
{
    for (uint8_t depth = 0; depth < tableau_count[col]; depth++) {
        if (tableau[col][depth].face_up == CARD_FACE_UP) {
            return depth;
        }
    }

    return tableau_count[col];
}

static uint8_t expand_or_cancel_tableau_selection(void)
{
    uint8_t first_depth = first_face_up_depth(selector_col);
    uint8_t depth;
    uint8_t count;
    uint8_t card;

    if (selected_depth == first_depth) {
        cancel_selection();
        return FLAG_ON;
    }

    depth = (uint8_t)(selected_depth - FLAG_ON);
    count = (uint8_t)(tableau_count[selector_col] - depth);
    card = tableau[selector_col][depth].card;
    select_card(SELECTOR_ROW_TABLEAU, selector_col, depth, count, card);
    return FLAG_ON;
}

static uint8_t handle_tableau_selection(void)
{
    uint8_t depth;
    uint8_t card;

    if (selected_active) {
        if (selected_row == SELECTOR_ROW_TABLEAU && selected_col == selector_col) {
            return expand_or_cancel_tableau_selection();
        }

        return move_selected_to_tableau(selector_col);
    }

    if (tableau_count[selector_col] == FLAG_OFF) {
        return FLAG_OFF;
    }

    depth = (uint8_t)(tableau_count[selector_col] - FLAG_ON);
    if (tableau[selector_col][depth].face_up != CARD_FACE_UP) {
        return FLAG_OFF;
    }

    card = tableau[selector_col][depth].card;
    if (selection_matches(SELECTOR_ROW_TABLEAU, selector_col, depth)) {
        cancel_selection();
        return FLAG_ON;
    }

    select_card(SELECTOR_ROW_TABLEAU, selector_col, depth, FLAG_ON, card);
    return FLAG_ON;
}

static uint8_t handle_select_action(void)
{
    if (selector_row == SELECTOR_ROW_TOP) {
        if (selector_col == TOP_PILE_STOCK) {
            return handle_stock_action();
        } else if (selector_col == TOP_PILE_WASTE) {
            return handle_waste_selection();
        } else if (selected_active) {
            return move_selected_to_foundation((uint8_t)(selector_col - TOP_PILE_FOUNDATION_FIRST));
        }
        return FLAG_OFF;
    }

    return handle_tableau_selection();
}

static void render_opening_tableau(void)
{
    for (uint8_t col = 0; col < SOLITAIRE_TABLEAU_COLS; col++) {
        for (uint8_t depth = 0; depth < tableau_count[col]; depth++) {
            draw_tableau_card(col, depth);
        }
    }
}

static uint8_t row_count(uint8_t row)
{
    return (row == SELECTOR_ROW_TOP) ? SOLITAIRE_TOP_ROW_PILES : SOLITAIRE_TABLEAU_COLS;
}

static uint8_t current_selector_x(void)
{
    if (selector_row == SELECTOR_ROW_TOP) {
        return kTopRowX[selector_col];
    }
    return kBottomRowX[selector_col];
}

static uint8_t tableau_selected_marker_y(void)
{
    return (uint8_t)(SOLITAIRE_TABLEAU_START_Y + selected_depth + kCardSelectorRowOffset);
}

static uint8_t current_selector_y(void)
{
    if (selector_row == SELECTOR_ROW_TOP) {
        return kTopRowY;
    }

    if (selected_active &&
        selected_row == SELECTOR_ROW_TABLEAU &&
        selected_col == selector_col) {
        return tableau_selected_marker_y();
    }

    if (tableau_count[selector_col] == FLAG_OFF) {
        return kEmptyTableauSelectorY;
    }

    return (uint8_t)(SOLITAIRE_TABLEAU_START_Y +
        (tableau_count[selector_col] - FLAG_ON) +
        kCardSelectorRowOffset);
}

static uint8_t selected_marker_x(void)
{
    if (selected_row == SELECTOR_ROW_TOP) {
        return kTopRowX[selected_col];
    }

    return kBottomRowX[selected_col];
}

static uint8_t selected_marker_y(void)
{
    if (selected_row == SELECTOR_ROW_TOP) {
        return kTopRowY;
    }

    return tableau_selected_marker_y();
}

static uint8_t current_hand_x(void)
{
    return mouse_tile_cursor_active ? mouse_tile_cursor_x : current_selector_x();
}

static uint8_t current_hand_y(void)
{
    return mouse_tile_cursor_active ? mouse_tile_cursor_y : current_selector_y();
}

static void draw_selected_marker(void)
{
    if (selected_active) {
        render_draw_closed_hand_selector(selected_marker_x(), selected_marker_y());
    }
}

static void redraw_hand_markers(void)
{
    render_draw_hand_selector(current_hand_x(), current_hand_y());
    draw_selected_marker();
}

static void clear_selected_marker(void)
{
    if (selected_active) {
        render_clear_hand_selector(selected_marker_x(), selected_marker_y());
    }
}

static uint8_t selection_matches(uint8_t row, uint8_t col, uint8_t depth)
{
    return (selected_active &&
        selected_row == row &&
        selected_col == col &&
        selected_depth == depth) ? FLAG_ON : FLAG_OFF;
}

static void cancel_selection(void)
{
    clear_selected_marker();
    selected_active = FLAG_OFF;
}

static void select_card(uint8_t row, uint8_t col, uint8_t depth, uint8_t count, uint8_t card)
{
    cancel_selection();
    selected_active = FLAG_ON;
    selected_row = row;
    selected_col = col;
    selected_depth = depth;
    selected_count = count;
    selected_card = card;
    draw_selected_marker();
}

static uint8_t undo_selection(void)
{
    if (!selected_active) {
        return FLAG_OFF;
    }

    clear_selected_marker();

    if (selected_count <= FLAG_ON || selected_row != SELECTOR_ROW_TABLEAU) {
        selected_active = FLAG_OFF;
        return FLAG_ON;
    }

    selected_depth++;
    selected_count--;
    selected_card = tableau[selected_col][selected_depth].card;
    draw_selected_marker();
    return FLAG_ON;
}

static uint8_t abs_diff_u8(uint8_t a, uint8_t b)
{
    if (a > b) {
        return (uint8_t)(a - b);
    }
    return (uint8_t)(b - a);
}

static uint8_t nearest_col_in_row(uint8_t target_row, uint8_t x)
{
    uint8_t best_idx = SELECTOR_COL_MIN;
    uint8_t best_diff = U8_MAX_VALUE;
    uint8_t count = row_count(target_row);
    uint8_t i = 0;

    for (i = 0; i < count; i++) {
        uint8_t px;
        uint8_t diff;

        if (target_row == SELECTOR_ROW_TOP) {
            px = kTopRowX[i];
        } else {
            px = kBottomRowX[i];
        }

        diff = abs_diff_u8(px, x);
        if (diff < best_diff) {
            best_diff = diff;
            best_idx = i;
        }
    }

    return best_idx;
}

static void move_selector_left(void)
{
    if (selector_col > SELECTOR_COL_MIN) {
        selector_col--;
    }
}

static void move_selector_right(void)
{
    uint8_t count = row_count(selector_row);
    if ((uint8_t)(selector_col + FLAG_ON) < count) {
        selector_col++;
    }
}

static void move_selector_vertical(void)
{
    uint8_t old_x = current_selector_x();

    if (selector_row == SELECTOR_ROW_TOP) {
        selector_row = SELECTOR_ROW_TABLEAU;
    } else {
        selector_row = SELECTOR_ROW_TOP;
    }

    selector_col = nearest_col_in_row(selector_row, old_x);
}

static int16_t clamp_mouse_units(int16_t value, uint8_t max_tile)
{
    int16_t max_value = (int16_t)max_tile * MOUSE_TILE_UNITS;

    if (value < 0) {
        return 0;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

static uint8_t mouse_units_to_tile(int16_t value)
{
    return (uint8_t)((value + (MOUSE_TILE_UNITS / 2)) / MOUSE_TILE_UNITS);
}

static void sync_mouse_cursor_to_selector(void)
{
    mouse_cursor_units_x = (int16_t)current_selector_x() * MOUSE_TILE_UNITS;
    mouse_cursor_units_y = (int16_t)current_selector_y() * MOUSE_TILE_UNITS;
    mouse_tile_cursor_x = current_selector_x();
    mouse_tile_cursor_y = current_selector_y();
}

static uint8_t apply_mouse_motion(const KeyEvents* ev)
{
    uint8_t mouse_tile_x;
    uint8_t mouse_tile_y;
    uint8_t old_row = selector_row;
    uint8_t old_col = selector_col;
    uint8_t old_cursor_x = mouse_tile_cursor_x;
    uint8_t old_cursor_y = mouse_tile_cursor_y;

    if (!ev->mouse_present || (ev->mouse_dx == 0 && ev->mouse_dy == 0)) {
        return FLAG_OFF;
    }

    mouse_cursor_units_x = clamp_mouse_units(
        (int16_t)(mouse_cursor_units_x + ev->mouse_dx),
        (uint8_t)(SCREEN_TILE_W - 1U));
    mouse_cursor_units_y = clamp_mouse_units(
        (int16_t)(mouse_cursor_units_y + (ev->mouse_dy * MOUSE_Y_DIRECTION)),
        (uint8_t)(SCREEN_TILE_H - 1U));

    mouse_tile_x = mouse_units_to_tile(mouse_cursor_units_x);
    mouse_tile_y = mouse_units_to_tile(mouse_cursor_units_y);
    mouse_tile_cursor_active = FLAG_ON;
    mouse_tile_cursor_x = mouse_tile_x;
    mouse_tile_cursor_y = mouse_tile_y;

    selector_row = (mouse_tile_y < MOUSE_ROW_SPLIT_Y) ?
        SELECTOR_ROW_TOP :
        SELECTOR_ROW_TABLEAU;
    selector_col = nearest_col_in_row(selector_row, mouse_tile_x);

    return (old_row != selector_row ||
        old_col != selector_col ||
        old_cursor_x != mouse_tile_cursor_x ||
        old_cursor_y != mouse_tile_cursor_y) ? FLAG_ON : FLAG_OFF;
}

void solitaire_init_game(void)
{
    shuffle_deck();
    selected_active = FLAG_OFF;
    game_won = FLAG_OFF;
    deal_opening_tableau(DEAL_WITH_ANIMATION);
    render_opening_tableau();
    render_stock_and_waste();
}

static void restart_game(void)
{
    render_table();
    solitaire_init_game();
    solitaire_init_controls();
}

void solitaire_init_controls(void)
{
    uint8_t x;
    uint8_t y;

    selector_row = SELECTOR_ROW_TOP;
    selector_col = SELECTOR_COL_MIN;
    mouse_tile_cursor_active = FLAG_OFF;

    x = current_selector_x();
    y = current_selector_y();
    render_draw_hand_selector(x, y);
    sync_mouse_cursor_to_selector();
}

void solitaire_handle_input(const KeyEvents* ev)
{
    uint8_t old_x = current_hand_x();
    uint8_t old_y = current_hand_y();
    uint8_t new_x;
    uint8_t new_y;
    uint8_t moved = FLAG_OFF;

    if (game_won) {
        if (ev->start) {
            restart_game();
        }
        return;
    }

    if (ev->left) {
        mouse_tile_cursor_active = FLAG_OFF;
        move_selector_left();
        moved = FLAG_ON;
    }
    if (ev->right) {
        mouse_tile_cursor_active = FLAG_OFF;
        move_selector_right();
        moved = FLAG_ON;
    }
    if (ev->up || ev->down) {
        mouse_tile_cursor_active = FLAG_OFF;
        move_selector_vertical();
        moved = FLAG_ON;
    }
    if (apply_mouse_motion(ev)) {
        moved = FLAG_ON;
    }

    new_x = current_hand_x();
    new_y = current_hand_y();

    if (moved && (old_x != new_x || old_y != new_y)) {
        render_clear_hand_selector(old_x, old_y);
        redraw_hand_markers();
        if (!mouse_tile_cursor_active) {
            sync_mouse_cursor_to_selector();
        }
    }

    if (ev->undo || ev->mouse_right) {
        if (undo_selection()) {
            redraw_hand_markers();
        } else {
            show_invalid_move_feedback();
        }
    }

    if (ev->a || ev->mouse_left) {
        old_x = current_hand_x();
        old_y = current_hand_y();

        if (!handle_select_action()) {
            show_invalid_move_feedback();
        }

        if (game_won) {
            return;
        }

        new_x = current_hand_x();
        new_y = current_hand_y();
        if (old_x != new_x || old_y != new_y) {
            render_clear_hand_selector(old_x, old_y);
        }

        redraw_hand_markers();
        if (!mouse_tile_cursor_active) {
            sync_mouse_cursor_to_selector();
        }
    }
}
