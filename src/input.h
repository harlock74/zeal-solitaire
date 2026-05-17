#pragma once

#include <stdint.h>
#include <zos_errors.h>

/*
 * One-shot gameplay events consumed by state modules.
 * ZGDK maps keyboard and SNES controller input into the same button mask.
 */
typedef struct {
    uint8_t up;
    uint8_t down;
    uint8_t left;
    uint8_t right;
    uint8_t accept;
    uint8_t mouse_accept;
    uint8_t mouse_accept_held;
    uint8_t mouse_accept_released;
    uint8_t cancel;
    uint8_t mouse_cancel;
    uint8_t start;
    uint8_t quit;
    int8_t mouse_dx;
    int8_t mouse_dy;
} KeyEvents;

zos_err_t input_events_init(void);
void input_poll_events(KeyEvents* ev);
