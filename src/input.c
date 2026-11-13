#include <stdint.h>

#include <zgdk/input.h>

#include "input.h"

#define EVENT_NOT_PRESSED 0
#define EVENT_PRESSED 1
#define INPUT_USE_CONTROLLER 1
#define INPUT_SNES_PORT_NONE 0xFF

static uint16_t previous_mouse_buttons;
static uint8_t mouse_port;

static uint16_t read_mouse_buttons(ControllerMouse *mouse)
{
    /* Buttons and movement must come from the same complete transaction. */
    if (mouse_port == INPUT_SNES_PORT_NONE ||
        controller_read_mouse(mouse_port, mouse) == MOUSE_ERROR) {
        mouse->bits = 0;
        mouse->x = 0;
        mouse->y = 0;
        return 0;
    }

    return mouse->bits & (MOUSE_LEFT | MOUSE_RIGHT);
}

zos_err_t input_events_init(void)
{
    zos_err_t err = input_init(INPUT_USE_CONTROLLER);
    if (err != ERR_SUCCESS) {
        return err;
    }

    mouse_port = INPUT_SNES_PORT_NONE;
    if (controller_is_mouse(SNES_PORT1)) {
        mouse_port = SNES_PORT1;
    } else if(controller_is_mouse(SNES_PORT2)) {
        mouse_port = SNES_PORT2;
    }

    if (mouse_port != INPUT_SNES_PORT_NONE) {
        controller_set_mouse_sensitivity(mouse_port, MOUSE_HIGH);
        controller_set_mouse_sensitivity(mouse_port, MOUSE_MEDIUM);
    }

    ControllerMouse mouse;
    previous_mouse_buttons = read_mouse_buttons(&mouse);
    input_flush();
    return ERR_SUCCESS;
}

void input_poll_events(KeyEvents* ev)
{
    /*
     * ZGDK maps keyboard and SNES controller controls into one mask:
     * arrows/D-pad move, keyboard Space/Z maps to SNES B,
     * and keyboard X maps to SNES A for selection undo.
     */
    /* The previous poll flushed keyboard state and suppressed held pad
     * buttons, so this snapshot contains fresh keyboard/pad activations.
     */
    uint16_t pressed_input = input_read();
    uint16_t current_mouse_buttons;
    uint16_t pressed_mouse_buttons;
    uint16_t released_mouse_buttons;

    ControllerMouse mouse;
    current_mouse_buttons = read_mouse_buttons(&mouse);
    /* Consume keyboard/pad taps, clear stale keys, and suppress held pad
     * buttons through ZGDK. Keep mouse state for dragging and release edges.
     */
    input_flush();
    ev->mouse_dx = mouse.x;
    ev->mouse_dy = mouse.y;

    pressed_mouse_buttons = (current_mouse_buttons & ~previous_mouse_buttons);
    released_mouse_buttons = (previous_mouse_buttons & ~current_mouse_buttons);

    ev->up = (pressed_input & BUTTON_UP) ? EVENT_PRESSED : EVENT_NOT_PRESSED;
    ev->down = (pressed_input & BUTTON_DOWN) ? EVENT_PRESSED : EVENT_NOT_PRESSED;
    ev->left = (pressed_input & BUTTON_LEFT) ? EVENT_PRESSED : EVENT_NOT_PRESSED;
    ev->right = (pressed_input & BUTTON_RIGHT) ? EVENT_PRESSED : EVENT_NOT_PRESSED;
    ev->mouse_accept = (pressed_mouse_buttons & MOUSE_LEFT) ? EVENT_PRESSED : EVENT_NOT_PRESSED;
    ev->mouse_accept_held = (current_mouse_buttons & MOUSE_LEFT) ? EVENT_PRESSED : EVENT_NOT_PRESSED;
    ev->mouse_accept_released = (released_mouse_buttons & MOUSE_LEFT) ? EVENT_PRESSED : EVENT_NOT_PRESSED;
    ev->accept = ((pressed_input & BUTTON_B) || ev->mouse_accept) ? EVENT_PRESSED : EVENT_NOT_PRESSED;
    ev->mouse_cancel = (pressed_mouse_buttons & MOUSE_RIGHT) ? EVENT_PRESSED : EVENT_NOT_PRESSED;
    ev->cancel = ((pressed_input & BUTTON_A) || ev->mouse_cancel) ? EVENT_PRESSED : EVENT_NOT_PRESSED;
    ev->start = (pressed_input & BUTTON_START) ? EVENT_PRESSED : EVENT_NOT_PRESSED;
    ev->quit = (pressed_input & BUTTON_SELECT) ? EVENT_PRESSED : EVENT_NOT_PRESSED;

    previous_mouse_buttons = current_mouse_buttons;
}
