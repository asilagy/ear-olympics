/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 asilagy */
/* Ear Olympics: a WarioWare-style ear-training arcade for the M-VAVE FM-1. This header is the app's half of the
 * contract with fm1-base (device/unity.c, host/plat_host.c). */
#pragma once
#include <stdint.h>
#include "plat.h"

#define APP_SPLASH
void app_splash(void);
#define APP_NAME "EAR OLYMPICS"
#define EO_VERSION "0.17.4-simone"
#define APP_SPLASH_RGB RGB(255, 196, 64)
#define APP_BLOCK 256u                   /* frames per audio block: 5.8 ms at 44.1 kHz */
#define APP_TICK                         /* 1 kHz, in the timer interrupt: the melodies are played on this clock */
void app_tick(void);

/* the store: the settings at 0xFC000, the panel calibration at 0xDC000 (the platform's: never move it) */
enum { OBJ_SETTINGS, OBJ_PANEL, OBJ_NOBJ };
#define APP_STORE_MAP {0xFC000u, 0xDC000u}

/* the contract: the platform calls these */
void app_init(void);                     /* boot: the saved settings (or defaults) */
void app_render(int32_t *lr, uint32_t n, uint32_t master_q12);   /* audio interrupt: n stereo frames, 24-bit */
void app_panic(void);                    /* silence now (before an update rewrites the flash) */
void app_ui_init(void);                  /* the first screen, after the splash */
void app_frame(void);                    /* the UI: input and a redraw, about 60 times a second */
void app_input_only(void);               /* between frames: input only */
void app_say(const char *a, const char *b);   /* a short message on the screen and to a screen reader */

/* the host simulator's checks (host/plat_host.c "expect"): game, root, level, best, streak, score, prog, pending, dirty, note and drone (-1 none), state, text, pos, len, ansN, bright, mid, dim */
#define APP_HOST_EXPECT
int app_expect(const char *what, int *got);
