#include <stdint.h>

#include <zgdk/input.h>

#include "input.h"

enum {
    EVENT_NOT_PRESSED = 0,
    EVENT_PRESSED = 1,
    INPUT_USE_CONTROLLER = 1,
    INPUT_SNES_PORT_NONE = 0xFF,
};

static uint16_t previous_input;
static uint16_t previous_mouse_buttons;
static uint8_t pad_port;
static uint8_t mouse_port;

static uint16_t read_button_input(void)
{
    uint16_t input = keyboard_read();
    if(pad_port != INPUT_SNES_PORT_NONE) {
        input |= controller_read_port(pad_port);
    }
    return input;
}

static uint16_t read_mouse_buttons(void)
{
    if (mouse_port == INPUT_SNES_PORT_NONE) {
        return 0;
    }

    return controller_read_port(mouse_port);
}

zos_err_t input_events_init(void)
{
    zos_err_t err = input_init(INPUT_USE_CONTROLLER);
    if (err != ERR_SUCCESS) {
        return err;
    }

    pad_port = INPUT_SNES_PORT_NONE;
    mouse_port = INPUT_SNES_PORT_NONE;
    if(controller_is(SNES_PORT1, SNES_PAD)) {
        pad_port = SNES_PORT1;
    } else if (controller_is(SNES_PORT2, SNES_PAD)) {
        pad_port = SNES_PORT2;
    }
    if (controller_is(SNES_PORT1, SNES_MOUSE)) {
        mouse_port = SNES_PORT1;
    } else if(controller_is(SNES_PORT2, SNES_MOUSE)) {
        mouse_port = SNES_PORT2;
    }

    if (mouse_port != INPUT_SNES_PORT_NONE) {
        controller_set_mouse_sensitivity(mouse_port, MOUSE_MEDIUM);
        controller_set_mouse_sensitivity(mouse_port, MOUSE_MEDIUM);
    }

    previous_input = read_button_input();
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
    uint16_t current_input = read_button_input();
    uint16_t pressed_input = (current_input & ~previous_input);
    uint16_t current_mouse_buttons = read_mouse_buttons();
    uint16_t pressed_mouse_buttons = (current_mouse_buttons & ~previous_mouse_buttons);
    uint8_t mouse_moved = 0;

    if (mouse_port != INPUT_SNES_PORT_NONE) {
        mouse_moved = controller_read_mouse(mouse_port);
    }

    ev->up = (pressed_input & BUTTON_UP) ? EVENT_PRESSED : EVENT_NOT_PRESSED;
    ev->down = (pressed_input & BUTTON_DOWN) ? EVENT_PRESSED : EVENT_NOT_PRESSED;
    ev->left = (pressed_input & BUTTON_LEFT) ? EVENT_PRESSED : EVENT_NOT_PRESSED;
    ev->right = (pressed_input & BUTTON_RIGHT) ? EVENT_PRESSED : EVENT_NOT_PRESSED;
    ev->mouse_accept = (pressed_mouse_buttons & MOUSE_LEFT) ? EVENT_PRESSED : EVENT_NOT_PRESSED;
    ev->accept = ((pressed_input & BUTTON_B) || ev->mouse_accept) ? EVENT_PRESSED : EVENT_NOT_PRESSED;
    ev->mouse_cancel = (pressed_mouse_buttons & MOUSE_RIGHT) ? EVENT_PRESSED : EVENT_NOT_PRESSED;
    ev->cancel = ((pressed_input & BUTTON_A) || ev->mouse_cancel) ? EVENT_PRESSED : EVENT_NOT_PRESSED;
    ev->start = (pressed_input & BUTTON_START) ? EVENT_PRESSED : EVENT_NOT_PRESSED;
    ev->quit = (pressed_input & BUTTON_SELECT) ? EVENT_PRESSED : EVENT_NOT_PRESSED;
    ev->mouse_dx = 0;
    ev->mouse_dy = 0;

    if (mouse_moved) {
        ev->mouse_dx = controller_get_mousex();
        ev->mouse_dy = controller_get_mousey();
    }

    previous_input = current_input;
    previous_mouse_buttons = current_mouse_buttons;
}
