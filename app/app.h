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
#define EO_VERSION "0.54.7-docs"
#define APP_SPLASH_RGB RGB(255, 196, 64)
#define APP_BLOCK 256u                   /* frames per audio block: 5.8 ms at 44.1 kHz */
#define APP_TICK                         /* 1 kHz, in the timer interrupt: the melodies are played on this clock */
void app_tick(void);

/* the store: the settings at 0xFC000, the panel calibration at 0xDC000 (the platform's: never move it) */
enum { OBJ_SETTINGS, OBJ_PANEL, OBJ_NOBJ };
#define APP_STORE_MAP {0xFC000u, 0xDC000u}

/* the resource region (fm1.toml resources; fm1-base device/res.c): the read-only art and sound live in the flash outside the image,
 * 0x97000 to 0xDBFFF (the panel calibration object starts at 0xDC000, so the region stops there): 69 sectors of 4 KiB, a
 * 32-byte header and then the payload (app/gen_res.py packs it from app/res/eo_*.h; eo_res.h reads it). tools/budget.py checks the fit. */
#define APP_RES_BASE 0x97000u
#define APP_RES_SECTORS 69u

/* backup and restore over USB (fm1.toml backup = true: the base's resource transfer is the restore's): the saved settings only */
#define APP_BACKUP_OBJS {OBJ_SETTINGS}
#define APP_BACKUP_MAGIC 0x594C4F45u     /* "EOLY" (little-endian) */
#define APP_BACKUP_VERSION 1u
#define APP_BACKUP_WHAT "replaces your saved levels"
int app_backup_playing(void);            /* 1: no restore or resource write now (a sound is playing): STOP FIRST */
void app_backup_flush(void);             /* stopped: what is not saved yet, saved */
void app_backup_hold(int on);            /* a backup is streaming: no saving meanwhile */
void app_backup_restored(void);          /* everything from the flash again */

/* the contract: the platform calls these */
void app_init(void);                     /* boot: the saved settings (or defaults) */
void app_render(int32_t *lr, uint32_t n, uint32_t master_q12);   /* audio interrupt: n stereo frames, 24-bit */
void app_panic(void);                    /* silence now (before an update rewrites the flash) */
void app_ui_init(void);                  /* the first screen, after the splash */
void app_frame(void);                    /* the UI: input and a redraw, about 60 times a second */
void app_input_only(void);               /* between frames: input only */
void app_say(const char *a, const char *b);   /* a short message on the screen and to a screen reader */

/* the host simulator's checks (host/plat_host.c "expect"): game, root, level, best, streak, score, prog, pending, dirty, note and drone (-1 none), state, text, pos, len, ansN, bright, mid, dim, smp, res */
#define APP_HOST_EXPECT
int app_expect(const char *what, int *got);
