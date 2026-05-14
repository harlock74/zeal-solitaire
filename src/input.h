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
    uint8_t a;
    uint8_t undo;
    uint8_t start;
    uint8_t quit;
    uint8_t mouse_present;
    int8_t mouse_dx;
    int8_t mouse_dy;
    uint8_t mouse_left;
    uint8_t mouse_right;
} KeyEvents;

zos_err_t input_events_init(void);
void input_poll_events(KeyEvents* ev);
