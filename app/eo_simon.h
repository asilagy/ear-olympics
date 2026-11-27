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

static const simon_level_t SIMON_LV[SIMON_LEVELS] = {   /* Simone Says: always the same four keys, C D E G. The levels are the pressure, not the notes */
    {4, {0, 2, 4, 7}},
    {4, {0, 2, 4, 7}},
    {4, {0, 2, 4, 7}},
    {4, {0, 2, 4, 7}},
};

/* a level graduates when you get a sequence this long: the wall is about 7 notes, past 9 you have to chunk */
static const uint8_t SIMON_GOAL[SIMON_LEVELS] = {5, 7, 9, 12};
static inline uint32_t simon_goal(uint32_t level)
{
    return SIMON_GOAL[level < 1u ? 0u : level > SIMON_LEVELS ? SIMON_LEVELS - 1u : level - 1u];
}

/* the fish's memory, on your turn: how long until the first bubble pops, then how long between pops (ms), by level */
static const uint16_t SIMON_POP_GRACE[SIMON_LEVELS] = {2600, 1800, 1200, 800};
static const uint16_t SIMON_POP_EVERY[SIMON_LEVELS] = {900, 650, 450, 320};
static inline uint32_t simon_popped(uint32_t level, uint32_t since_ms, uint32_t len)   /* bubbles popped by now, oldest first */
{
    uint32_t i = level < 1u ? 0u : level > SIMON_LEVELS ? SIMON_LEVELS - 1u : level - 1u, n;
    if (since_ms < SIMON_POP_GRACE[i])
        return 0;
    n = 1u + (since_ms - SIMON_POP_GRACE[i]) / SIMON_POP_EVERY[i];
    return n > len ? len : n;
}

/* a note's word: the mnemonic. DRIP, WATER, SPLASH, SPLISH for the keys C D E G (offsets 0, 2, 4, 7) */
static inline uint32_t simon_word_ix(uint32_t off)
{
    return off == 0u ? 0u : off == 2u ? 1u : off == 4u ? 2u : 3u;
}

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

/* the tempo: a note every step_ms, quickening as the sequence grows and again at each level, then no faster than 300 ms */
static inline uint32_t simon_step_ms(uint32_t len, uint32_t level)
{
    uint32_t cut = (len > 4u ? 30u * (len - 4u) : 0u) + 40u * (level > 1u ? level - 1u : 0u);
    return cut > 350u ? 300u : 650u - cut;
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
