#include <stdint.h>
#include <stdlib.h>

#include <zos_sys.h>
#include <zos_video.h>
#include <zvb_gfx.h>
#include <zgdk.h>
#include <core.h>

#include "main.h"
#include "app_state.h"
#include "assets.h"
#include "render.h"
#include "audio.h"
#include "splash.h"
#include "solitaire.h"
#include "input.h"

enum {
    SCREEN_DISABLED = FLAG_OFF,
    SCREEN_ENABLED = FLAG_ON,
};

gfx_context vctx;
uint8_t __at(G_BUF_ADDR) g_buf[SHARED_SCRATCH_BUF_SIZE];
uint8_t g_running = FLAG_ON;

static void print_error_u16(const char* prefix, uint16_t value)
{
    put_s(prefix);
    put_u16(value);
    put_c('\n');
}

static void load_ui_font_tiles(void)
{
    /*
     * nprint_string writes to layer 1; map spaces to the transparent tile
     * so centered text and clears do not leave visible blocks.
     */
    ascii_map(' ', 1, EMPTY_TILE);
    ascii_map('A', 26, FONT_ALPHA_A_TILE);
    ascii_map('a', 26, FONT_ALPHA_A_TILE);
    ascii_map('0', 10, FONT_DIGIT_TILE);
    ascii_map('!', 1, FONT_EXCL_TILE);
}

void init(void)
{
    zos_err_t err;

    /*
     * ZGDK input maps keyboard and SNES controller controls into one mask.
     */
    err = input_events_init();
    if (err != ERR_SUCCESS) {
        print_error_u16("Input init failed: ", err);
        exit(1);
    }

    gfx_enable_screen(SCREEN_DISABLED);

    err = gfx_initialize(ZVB_CTRL_VID_MODE_GFX_640_8BIT, &vctx);
    if (err != ERR_SUCCESS) {
        print_error_u16("GFX init failed: ", err);
        exit(1);
    }

    err = load_cards_palette(&vctx);
    if (err != GFX_SUCCESS) {
        print_error_u16("Palette load failed: ", err);
        exit(1);
    }

    err = load_cards_tileset(&vctx);
    if (err != GFX_SUCCESS) {
        print_error_u16("Tileset load failed: ", err);
        exit(1);
    }
    load_ui_font_tiles();

    err = render_init_sprites();
    if (err != GFX_SUCCESS) {
        print_error_u16("Sprite init failed: ", err);
        exit(1);
    }

    audio_init();
    splash_run_placeholder();

    render_table();
    gfx_enable_screen(SCREEN_ENABLED);
    solitaire_init_game();
    solitaire_init_controls();
}

void deinit(void)
{
    audio_deinit();
    ioctl(DEV_STDOUT, CMD_RESET_SCREEN, NULL);
}

void update(void)
{
    KeyEvents ev;

    input_poll_events(&ev);
    solitaire_handle_input(&ev);

    if (ev.quit) {
        g_running = FLAG_OFF;
    }

    audio_tick();
}

void draw(void)
{
    render_draw_sprites();
}

int main(void)
{
    init();

    while (g_running) {
        update();
        gfx_wait_vblank(&vctx);
        draw();
        gfx_wait_end_vblank(&vctx);
    }

    deinit();
    return 0;
}
