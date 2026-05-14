#include <stdint.h>

#include <zgdk/input.h>

#include "input.h"

enum {
    EVENT_NOT_PRESSED = 0,
    EVENT_PRESSED = 1,
    INPUT_USE_CONTROLLER = 1,
    INPUT_MOUSE_PORT_NONE = 0xFF,
};

static uint16_t previous_input = 0;
static uint16_t previous_mouse_buttons = 0;
static uint8_t mouse_port = INPUT_MOUSE_PORT_NONE;

static uint16_t read_controller_input(void)
{
    uint16_t controller_input = controller_read();
    uint16_t unused_bits = (uint16_t)(
        BUTTON_UNUSED1 |
        BUTTON_UNUSED2 |
        BUTTON_UNUSED3 |
        BUTTON_UNUSED4);

    /*
     * A real SNES pad leaves the unused upper bits inactive. In the emulator,
     * or without a pad attached, those lines can float and look like every
     * button is held, which hides keyboard edge presses.
     */
    if (controller_input & unused_bits) {
        return 0;
    }

    return controller_input;
}

static uint16_t read_combined_input(void)
{
    return (uint16_t)(keyboard_read() | read_controller_input());
}

static uint16_t read_mouse_buttons(void)
{
    if (mouse_port == INPUT_MOUSE_PORT_NONE) {
        return 0;
    }

    return (uint16_t)(controller_get(mouse_port) & (MOUSE_LEFT | MOUSE_RIGHT));
}

zos_err_t input_events_init(void)
{
    zos_err_t err = input_init(INPUT_USE_CONTROLLER);
    if (err != ERR_SUCCESS) {
        return err;
    }

    mouse_port = INPUT_MOUSE_PORT_NONE;
    if (controller_is(SNES_PORT1, SNES_MOUSE)) {
        mouse_port = SNES_PORT1;
    } else if (controller_is(SNES_PORT2, SNES_MOUSE)) {
        mouse_port = SNES_PORT2;
    }

    if (mouse_port != INPUT_MOUSE_PORT_NONE) {
        controller_set_mouse_sensitivity(mouse_port, MOUSE_MEDIUM);
        controller_set_mouse_sensitivity(mouse_port, MOUSE_MEDIUM);
    }

    previous_input = read_combined_input();
    previous_mouse_buttons = read_mouse_buttons();
    return ERR_SUCCESS;
}

void input_poll_events(KeyEvents* ev)
{
    /*
     * ZGDK maps keyboard and SNES controller controls into one mask:
     * arrows/D-pad move, keyboard Space/Z maps to SNES B,
     * and keyboard X maps to SNES A for selection undo.
     */
    uint16_t current_input = read_combined_input();
    uint16_t pressed_input = (uint16_t)(current_input & (uint16_t)~previous_input);
    uint8_t mouse_moved = 0;

    if (mouse_port != INPUT_MOUSE_PORT_NONE) {
        mouse_moved = controller_read_mouse(mouse_port);
    }

    uint16_t current_mouse_buttons = read_mouse_buttons();
    uint16_t pressed_mouse_buttons = (uint16_t)(
        current_mouse_buttons & (uint16_t)~previous_mouse_buttons);

    ev->up = (pressed_input & BUTTON_UP) ? EVENT_PRESSED : EVENT_NOT_PRESSED;
    ev->down = (pressed_input & BUTTON_DOWN) ? EVENT_PRESSED : EVENT_NOT_PRESSED;
    ev->left = (pressed_input & BUTTON_LEFT) ? EVENT_PRESSED : EVENT_NOT_PRESSED;
    ev->right = (pressed_input & BUTTON_RIGHT) ? EVENT_PRESSED : EVENT_NOT_PRESSED;
    ev->a = (pressed_input & BUTTON_B) ? EVENT_PRESSED : EVENT_NOT_PRESSED;
    ev->undo = (pressed_input & BUTTON_A) ? EVENT_PRESSED : EVENT_NOT_PRESSED;
    ev->start = (pressed_input & BUTTON_START) ? EVENT_PRESSED : EVENT_NOT_PRESSED;
    ev->quit = (pressed_input & BUTTON_SELECT) ? EVENT_PRESSED : EVENT_NOT_PRESSED;
    ev->mouse_present = (mouse_port != INPUT_MOUSE_PORT_NONE) ? EVENT_PRESSED : EVENT_NOT_PRESSED;
    ev->mouse_dx = 0;
    ev->mouse_dy = 0;
    ev->mouse_left = (pressed_mouse_buttons & MOUSE_LEFT) ? EVENT_PRESSED : EVENT_NOT_PRESSED;
    ev->mouse_right = (pressed_mouse_buttons & MOUSE_RIGHT) ? EVENT_PRESSED : EVENT_NOT_PRESSED;

    if (mouse_moved) {
        ev->mouse_dx = controller_get_mousex();
        ev->mouse_dy = controller_get_mousey();
    }

    previous_input = current_input;
    previous_mouse_buttons = current_mouse_buttons;
}
