#pragma once

#include <stdint.h>
#include <zvb_gfx.h>
#include "main.h"

extern gfx_context vctx;
extern uint8_t __at(G_BUF_ADDR) g_buf[SHARED_SCRATCH_BUF_SIZE];
extern uint8_t g_running;
