/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 asilagy */
/* Ear Olympics' music, shared by the games: pure C, no hardware, so the simulator build and the native unit tests
 * (tests/host/music_test.c) both include it. Notes are semitone offsets from a base key. */
#pragma once
#include <stdint.h>

static inline uint32_t eo_rng(uint32_t *s)           /* xorshift32: the same seed, the same run (Daily codes, tests) */
{
    uint32_t x = *s ? *s : 0x9E3779B9u;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *s = x;
    return x;
}
