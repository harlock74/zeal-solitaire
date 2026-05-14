#pragma once

#include "input.h"

/*
 * Input/navigation bootstrap for milestone progression:
 * shared keyboard/controller controls with arrow/D-pad movement and action hook.
 */
void solitaire_init_game(void);
void solitaire_init_controls(void);
void solitaire_handle_input(const KeyEvents* ev);
