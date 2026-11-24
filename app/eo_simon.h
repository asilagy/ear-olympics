/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 asilagy */
/* Simon: follow the lights. A sequence of notes plays, lit as it sounds, and you play it back; each round adds one
 * note, and a wrong note ends the game. No root note: nothing to establish. The levels (how many keys are in play),
 * the next note, and the tempo. Pure C. */
#pragma once
#include "eo_music.h"

#define SIMON_LEVELS 4u
#define SIMON_MAX_LEN 20u                    /* a game that gets here is won */
#define SIMON_MAX_POOL 13u

typedef struct {
    uint8_t n;                               /* the keys in play: n offsets from the base key, in semitones */
    uint8_t off[SIMON_MAX_POOL];
} simon_level_t;

static const simon_level_t SIMON_LV[SIMON_LEVELS] = {
    {4, {0, 2, 4, 7}},                                    /* 1: four keys: C D E G */
    {5, {0, 2, 4, 7, 9}},                                 /* 2: five: the major pentatonic */
    {8, {0, 2, 4, 5, 7, 9, 11, 12}},                      /* 3: the major scale, an octave */
    {13, {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12}},     /* 4: every key of the octave */
};

static inline const simon_level_t *simon_level(uint32_t level)   /* level is 1-based, and clamped */
{
    return &SIMON_LV[level < 1u ? 0u : level > SIMON_LEVELS ? SIMON_LEVELS - 1u : level - 1u];
}

/* the next note of the sequence: any key in play but the one before it (prev is an offset, or 255 for none) */
static inline uint8_t simon_next(uint32_t level, uint32_t *rng, uint32_t prev)
{
    const simon_level_t *lv = simon_level(level);
    uint8_t o = lv->off[eo_rng(rng) % lv->n];
    uint32_t tries;
    for (tries = 0; o == prev && tries < 16u; tries++)
        o = lv->off[eo_rng(rng) % lv->n];
    return o;
}

/* the tempo: a note every step_ms, quickening as the sequence grows, then no faster than 330 ms */
static inline uint32_t simon_step_ms(uint32_t len)
{
    uint32_t cut = len > 4u ? 30u * (len - 4u) : 0u;
    return cut > 320u ? 330u : 650u - cut;
}

static inline uint32_t simon_note_ms(uint32_t step)  /* how long each note sounds within its step */
{
    uint32_t n = step * 60u / 100u;
    return n < 160u ? 160u : n;
}

/* the keys in play, as a mask, for a base key */
static inline uint32_t simon_pool_mask(uint32_t level, uint32_t base_key)
{
    const simon_level_t *lv = simon_level(level);
    uint32_t i, m = 0;
    for (i = 0; i < lv->n; i++)
        m |= 1u << (base_key + lv->off[i]);
    return m;
}
