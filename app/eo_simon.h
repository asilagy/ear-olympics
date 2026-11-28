/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 asilagy */
/* Simon: follow the lights. A sequence of notes plays, lit as it sounds, and you play it back; each round adds one
 * note, and a wrong note ends the game. No root note: nothing to establish. The levels (how many keys are in play),
 * the next note, and the tempo. Pure C. */
#pragma once
#include "eo_music.h"

#define SIMON_LEVELS 8u
#define SIMON_MAX_LEN 20u                    /* a game that gets here is won */
#define SIMON_MAX_POOL 13u

typedef struct {
    uint8_t n;                               /* the keys in play: n offsets from the base key, in semitones */
    uint8_t off[SIMON_MAX_POOL];
} simon_level_t;

static const simon_level_t SIMON_LV[SIMON_LEVELS] = {   /* Simone Says: always the same four keys, C D E G. The levels are the pressure, not the notes */
    {4, {0, 2, 4, 7}}, {4, {0, 2, 4, 7}}, {4, {0, 2, 4, 7}}, {4, {0, 2, 4, 7}},
    {4, {0, 2, 4, 7}}, {4, {0, 2, 4, 7}}, {4, {0, 2, 4, 7}}, {4, {0, 2, 4, 7}},
};

/* how many of the newest notes light as she replays the sequence: the older ones sound in the dark (an ear test) */
static const uint8_t SIMON_LIT[SIMON_LEVELS] = {255, 255, 255, 255, 255, 255, 255, 255};   /* (the lights stay on: going dark is her trick, late in a level, not the rule) */
static inline uint32_t simon_lit_count(uint32_t level)
{
    return SIMON_LIT[level < 1u ? 0u : level > SIMON_LEVELS ? SIMON_LEVELS - 1u : level - 1u];
}

/* Simone's performance: how long her pre-roll is (the swell and the dazzle, before the sequence), and the chance, in percent,
 * that she has a lapse in the middle of it. Level 1 has neither: easy at the start. */
static inline uint32_t simon_pre_ms(uint32_t level)
{
    return level <= 2u ? 2800u : 3200u;
}
/* her tricks, in the order a player meets them: ONE new trick per level (WarioWare: show it once on a short round, let them play
 * it, then it joins a small random pool; a later level twists it). The chance, in percent, a round has each, by level 1..8.
 * The first few are about looks and sound (polish, not difficulty); the rules and the memory tricks come later. */
enum { SIMON_FX_NONE, SIMON_FX_PRE, SIMON_FX_LAPSE, SIMON_FX_RESTART, SIMON_FX_STALL, SIMON_FX_CRYSTAL, SIMON_FX_SLEEP, SIMON_FX_TWICE, SIMON_FX_DARK, SIMON_FX_EYES };   /* (TWICE is the riddle family: each has its own rule) */
static const uint8_t SIMON_PCT_PRE[SIMON_LEVELS]     = {0, 30, 28, 26, 25, 25, 25, 25};   /* the dazzle (level 1 debut) */
static const uint8_t SIMON_PCT_SLEEP[SIMON_LEVELS]   = {0, 15, 15, 12, 12, 12, 12, 12};   /* sleepy: gray-blue, rippling (level 2) */
static const uint8_t SIMON_PCT_CRYSTAL[SIMON_LEVELS] = {0, 0, 20, 15, 15, 15, 15, 15};    /* the crystal ball (level 3) */
static const uint8_t SIMON_PCT_LAPSE[SIMON_LEVELS]   = {0, 0, 0, 20, 15, 15, 15, 15};     /* she loses her place (level 4); level 8 adds the stall and the restart */
static const uint8_t SIMON_PCT_EYES[SIMON_LEVELS]    = {0, 0, 0, 0, 20, 12, 12, 12};       /* use your eyes: silent words (level 5) */
static const uint8_t SIMON_PCT_RIDDLE[SIMON_LEVELS]  = {0, 0, 0, 0, 0, 15, 12, 20};        /* riddles: a rule for how to play it back (level 6; more rules at 7 and 8) */
static const uint8_t SIMON_PCT_DARK[SIMON_LEVELS]    = {0, 0, 0, 0, 0, 0, 20, 20};        /* lights out (level 7) */
static inline uint32_t simon_pct(const uint8_t *t, uint32_t level)
{
    return t[level < 1u ? 0u : level > SIMON_LEVELS ? SIMON_LEVELS - 1u : level - 1u];
}
static inline uint32_t simon_pre_pct(uint32_t level) { return simon_pct(SIMON_PCT_PRE, level); }
static inline uint32_t simon_sleep_pct(uint32_t level) { return simon_pct(SIMON_PCT_SLEEP, level); }
static inline uint32_t simon_crystal_pct(uint32_t level) { return simon_pct(SIMON_PCT_CRYSTAL, level); }
static inline uint32_t simon_lapse_pct(uint32_t level) { return simon_pct(SIMON_PCT_LAPSE, level); }
static inline uint32_t simon_eyes_pct(uint32_t level) { return simon_pct(SIMON_PCT_EYES, level); }
static inline uint32_t simon_twice_pct(uint32_t level) { return simon_pct(SIMON_PCT_RIDDLE, level); }
static inline uint32_t simon_dark_pct(uint32_t level) { return simon_pct(SIMON_PCT_DARK, level); }

/* which trick debuts at which round (the sequence length) of which level: forced once a level (the last level is the finale: no debut) */
static inline uint32_t simon_debut(uint32_t level, uint32_t len)
{
    return level == 1u ? (len == 3u ? SIMON_FX_PRE : 0u) :
           level == 2u ? (len == 3u ? SIMON_FX_SLEEP : 0u) :
           level == 3u ? (len == 2u ? SIMON_FX_CRYSTAL : 0u) :
           level == 4u ? (len == 3u ? SIMON_FX_LAPSE : 0u) :
           level == 5u ? (len == 2u ? SIMON_FX_EYES : 0u) :
           level == 6u ? (len == 3u ? SIMON_FX_TWICE : 0u) :
           level == 7u ? (len == 4u ? SIMON_FX_DARK : 0u) : 0u;
}

/* lights out: how many of the newest notes still light */
static inline uint32_t simon_dark_lit(uint32_t level)
{
    return level == 7u ? 2u : 1u;
}

/* the riddles: a rule, in verse, for how to play the round back. Which rules a level may use, and how long the round may be:
 * 1 each note twice (level 6), 2 the second note thrice (7), 3 first and last only, 4 skip the first, 5 last note twice (8) */
static inline uint32_t simon_rule_ok(uint32_t rule, uint32_t len, uint32_t level)
{
    if (rule < 1u || rule > 5u)
        return 0u;
    if (level < (rule == 1u ? 6u : rule == 2u ? 7u : 8u))
        return 0u;
    return (rule == 3u || rule == 4u) ? (len >= 3u && len <= 5u) : (len >= 2u && len <= 4u);
}

/* a level graduates when you get a sequence this long: the wall is about 7 notes, past 9 you have to chunk */
static const uint8_t SIMON_GOAL[SIMON_LEVELS] = {4, 4, 5, 5, 5, 6, 6, 6};
static inline uint32_t simon_goal(uint32_t level)
{
    return SIMON_GOAL[level < 1u ? 0u : level > SIMON_LEVELS ? SIMON_LEVELS - 1u : level - 1u];
}

/* the fish's memory, on your turn: how long until the first bubble pops, then how long between pops (ms), by level */
static const uint16_t SIMON_POP_GRACE[SIMON_LEVELS] = {2600, 2400, 2000, 1700, 1500, 1300, 1200, 1100};
static const uint16_t SIMON_POP_EVERY[SIMON_LEVELS] = {900, 800, 700, 600, 550, 500, 450, 400};
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
    uint32_t cut = (len > 4u ? 30u * (len - 4u) : 0u) + 25u * (level > 1u ? level - 1u : 0u);
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
