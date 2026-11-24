/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 asilagy */
/* Copycat: you hear a root note, then a melody over it, and play the melody back. Ear training: the melody is not
 * lit (except where a level says so) and the root is only a foundation. The levels and the melody generator. Pure C.
 *
 * The ladder follows how aural-skills courses teach melodic dictation: a few scale degrees at a time; steps, then
 * thirds, then leaps (first to chord tones, which are easier to hear); a leap is followed by a step that fills the gap;
 * the drone goes away, then the start-on-the-root anchor; then the root moves around the cycle of fifths; chromatic notes
 * come last, first as passing and neighbour tones, then as leaps. */
#pragma once
#include "eo_music.h"

#define COPYCAT_LEVELS 34u
#define COPYCAT_MAX_LEN 8u
#define COPYCAT_MAX_POOL 20u
#define COPYCAT_STEP 2                  /* up to this many semitones is a step (a second) */
#define COPYCAT_THIRD 4                 /* up to this is a third, still easy to hold */
#define COPYCAT_LEAP 5                  /* this many semitones or more is a leap */
#define COPYCAT_KEY_MAX 26              /* the keyboard's top key (key 0 is F3) */

enum { CHROMA_NONE, CHROMA_PASSING, CHROMA_ANY };   /* how chromatic notes may appear */
enum { LIT_NONE, LIT_ALL, LIT_CHROMATIC };          /* which notes light as they sound */

typedef struct {
    uint8_t n;                           /* the notes the melody may use: n offsets from the root, in semitones */
    int8_t off[COPYCAT_MAX_POOL];
    uint8_t len;                         /* how many notes the melody has */
    uint8_t lit;                         /* LIT_*: the melody's keys light as they sound */
    uint8_t contour;                     /* crutch: the melody's shape is drawn on screen, coming out of the cat's mouth */
    uint8_t hear;                        /* crutch: how many times the melody plays before your turn (1 or 2) */
    uint8_t drone;                       /* the root keeps sounding, softly, under the melody and your turn */
    uint8_t hint;                        /* the level's notes glow on the keyboard */
    uint8_t start_root;                  /* the melody starts on the root: the anchor when there is no drone */
    uint8_t max_d;                       /* the biggest jump between two notes, in semitones */
    uint8_t min_leaps, max_leaps;        /* how many leaps a melody has: a level that teaches leaps always has one */
    uint8_t fill;                        /* a leap is followed by a step back toward where it came from */
    uint8_t chord_leaps;                 /* a leap lands on a chord tone of the root (1, 3 or 5) */
    uint8_t chroma;                      /* CHROMA_*: chromatic notes (outside the major scale) */
    uint8_t chroma_min, chroma_max;      /* how many chromatic notes a melody has */
    uint8_t roots;                       /* how many roots, along the cycle of fifths C G D A E F# C#: 1 is the chosen root */
    uint8_t max_span;                    /* the melody's highest note minus its lowest, in semitones */
    uint8_t rounds;                      /* melodies played right to graduate: few at the easy levels, more as it gets hard */
    uint8_t intro;                       /* a level that introduces new pitches: every melody uses at least one of them */
    int8_t intro_lo, intro_hi;           /* the new pitches, as offsets from the root (a range) */
} copycat_level_t;

#define P_D3 3, {0, 2, 4}
#define P_D5 5, {0, 2, 4, 5, 7}
#define P_D6 6, {0, 2, 4, 5, 7, 9}
#define P_D8 8, {0, 2, 4, 5, 7, 9, 11, 12}
#define P_CH8 8, {0, 1, 2, 3, 4, 5, 6, 7}                       /* C through G, every semitone */
#define P_WIDE 11, {-5, -3, -1, 0, 2, 4, 5, 7, 9, 11, 12}       /* a fifth below the root to the octave above */
#define P_CH13 13, {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12}
#define P_BOSS 18, {-5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12}

/* One new thing per level, braided across several tracks (how many notes, which pitches, leaps, how much help,
 * chromatic notes, moving roots): a short chromatic run is about as hard as a longer diatonic melody with a leap, so
 * they take turns. When new pitches arrive the melody is short again, and grows on the next level.
 * Fields: pool..., len, lit, contour, hear, drone, hint, start_root, max_d, min_leaps, max_leaps, fill, chord_leaps,
 *         chroma, chroma_min, chroma_max, roots, max_span, rounds, [intro, intro_lo, intro_hi]. A level that brings in new
 * pitches comes first with the lights on and a new pitch in every melody, then again with the lights off. */
static const copycat_level_t COPYCAT_LV[COPYCAT_LEVELS] = {
    /* A: home key, steps, with help */
    /* 1*/ {P_D3, 3, LIT_ALL, 1, 1, 1, 1, 0, 2, 0, 0, 0, 0, CHROMA_NONE, 0, 0, 1, 4, 8, 0, 0, 0},          /* do re mi, lit */
    /* 2*/ {P_D3, 3, LIT_NONE, 1, 1, 1, 1, 0, 2, 0, 0, 0, 0, CHROMA_NONE, 0, 0, 1, 4, 8, 0, 0, 0},         /* by ear */
    /* 3*/ {P_D5, 3, LIT_ALL, 1, 1, 1, 1, 0, 2, 0, 0, 0, 0, CHROMA_NONE, 0, 0, 1, 7, 8, 1, 5, 7},      /* fa and sol arrive, lit, one in every melody */
    /* 4*/ {P_D5, 3, LIT_NONE, 1, 1, 1, 1, 0, 2, 0, 0, 0, 0, CHROMA_NONE, 0, 0, 1, 7, 8, 0, 0, 0},      /* the same five notes, lights off */
    /* 5*/ {P_D5, 4, LIT_NONE, 1, 1, 1, 1, 0, 2, 0, 0, 0, 0, CHROMA_NONE, 0, 0, 1, 7, 8, 0, 0, 0},         /* longer */
    /* 6*/ {P_D5, 4, LIT_NONE, 1, 1, 1, 1, 0, 4, 0, 0, 0, 0, CHROMA_NONE, 0, 0, 1, 7, 8, 0, 0, 0},         /* thirds */
    /* 7*/ {P_D6, 3, LIT_ALL, 1, 1, 1, 1, 0, 4, 0, 0, 0, 0, CHROMA_NONE, 0, 0, 1, 9, 8, 1, 9, 9},      /* la arrives, lit */
    /* 8*/ {P_D6, 3, LIT_NONE, 1, 1, 1, 1, 0, 4, 0, 0, 0, 0, CHROMA_NONE, 0, 0, 1, 9, 8, 0, 0, 0},      /* la by ear */
    /* 9*/ {P_D6, 4, LIT_NONE, 1, 1, 1, 1, 0, 4, 0, 0, 0, 0, CHROMA_NONE, 0, 0, 1, 9, 8, 0, 0, 0},         /* longer */
    /*10*/ {P_D8, 3, LIT_ALL, 1, 1, 1, 1, 0, 4, 0, 0, 0, 0, CHROMA_NONE, 0, 0, 1, 12, 8, 1, 11, 12},    /* ti and the octave arrive, lit */
    /*11*/ {P_D8, 3, LIT_NONE, 1, 1, 1, 1, 0, 4, 0, 0, 0, 0, CHROMA_NONE, 0, 0, 1, 12, 8, 0, 0, 0},     /* the whole octave by ear */
    /*12*/ {P_D8, 4, LIT_NONE, 1, 1, 1, 1, 0, 4, 0, 0, 0, 0, CHROMA_NONE, 0, 0, 1, 12, 8, 0, 0, 0},        /* longer */
    /* the first chromatic notes, but only C through G and three notes: a short run of semitones */
    /*13*/ {P_CH8, 3, LIT_CHROMATIC, 1, 1, 1, 1, 0, 2, 0, 0, 0, 0, CHROMA_ANY, 1, 2, 1, 7, 10, 0, 0, 0},    /* semitones, lit */
    /*14*/ {P_CH8, 3, LIT_NONE, 1, 1, 1, 1, 0, 2, 0, 0, 0, 0, CHROMA_ANY, 1, 2, 1, 7, 10, 0, 0, 0},         /* by ear */
    /* B: leaps, always one in the melody */
    /*15*/ {P_D8, 3, LIT_NONE, 1, 1, 1, 1, 0, 7, 1, 1, 1, 1, CHROMA_NONE, 0, 0, 1, 12, 10, 0, 0, 0},        /* a leap to a chord tone */
    /*16*/ {P_D8, 4, LIT_NONE, 1, 1, 1, 1, 0, 7, 1, 1, 1, 1, CHROMA_NONE, 0, 0, 1, 12, 10, 0, 0, 0},        /* longer */
    /*17*/ {P_D8, 4, LIT_NONE, 0, 1, 1, 1, 0, 9, 1, 1, 1, 0, CHROMA_NONE, 0, 0, 1, 12, 10, 0, 0, 0},        /* to any note, no contour */
    /*18*/ {P_D8, 5, LIT_NONE, 0, 1, 1, 1, 0, 9, 1, 2, 1, 0, CHROMA_NONE, 0, 0, 1, 12, 10, 0, 0, 0},        /* up to two leaps */
    /*19*/ {P_CH8, 4, LIT_NONE, 0, 1, 1, 1, 0, 7, 1, 1, 1, 0, CHROMA_ANY, 1, 2, 1, 7, 10, 0, 0, 0},         /* semitones and a leap */
    /* the chromatic notes across the whole octave: passing tones between the scale's notes */
    /*20*/ {P_CH13, 4, LIT_CHROMATIC, 0, 1, 1, 0, 0, 2, 0, 0, 0, 0, CHROMA_PASSING, 1, 1, 1, 12, 10, 0, 0, 0},  /* lit */
    /*21*/ {P_CH13, 5, LIT_NONE, 0, 1, 1, 0, 0, 2, 0, 0, 0, 0, CHROMA_PASSING, 1, 1, 1, 12, 10, 0, 0, 0},       /* by ear, longer */
    /* C: the anchors come off */
    /*22*/ {P_D8, 4, LIT_NONE, 0, 1, 0, 0, 1, 9, 1, 2, 1, 0, CHROMA_NONE, 0, 0, 1, 12, 10, 0, 0, 0},        /* no drone: starts on the root */
    /*23*/ {P_D8, 5, LIT_NONE, 0, 1, 0, 0, 1, 9, 1, 2, 1, 0, CHROMA_NONE, 0, 0, 1, 12, 10, 0, 0, 0},        /* longer */
    /*24*/ {P_D8, 5, LIT_NONE, 0, 1, 0, 0, 0, 9, 1, 2, 1, 0, CHROMA_NONE, 0, 0, 1, 12, 12, 0, 0, 0},        /* starts anywhere */
    /*25*/ {P_WIDE, 4, LIT_ALL, 0, 1, 0, 0, 0, 9, 1, 2, 1, 0, CHROMA_NONE, 0, 0, 1, 14, 12, 1, -5, -1},      /* below the root too */
    /*26*/ {P_WIDE, 5, LIT_NONE, 0, 1, 0, 0, 0, 9, 1, 2, 1, 0, CHROMA_NONE, 0, 0, 1, 14, 12, 0, 0, 0},      /* below the root too */
    /*27*/ {P_WIDE, 6, LIT_NONE, 0, 1, 0, 0, 0, 9, 1, 2, 1, 0, CHROMA_NONE, 0, 0, 1, 14, 12, 0, 0, 0},      /* longer */
    /*28*/ {P_CH13, 5, LIT_CHROMATIC, 0, 1, 0, 0, 0, 9, 1, 2, 1, 0, CHROMA_ANY, 1, 2, 1, 12, 12, 0, 0, 0},  /* chromatic leaps, lit */
    /* D: the root moves (C G D, then A E, then F# C#) */
    /*29*/ {P_WIDE, 4, LIT_NONE, 0, 1, 0, 0, 1, 9, 1, 2, 1, 0, CHROMA_NONE, 0, 0, 3, 14, 12, 0, 0, 0},      /* C G D, starts on the root */
    /*30*/ {P_WIDE, 5, LIT_NONE, 0, 1, 0, 0, 1, 9, 1, 2, 1, 0, CHROMA_NONE, 0, 0, 3, 14, 12, 0, 0, 0},      /* longer */
    /*31*/ {P_WIDE, 5, LIT_NONE, 0, 1, 0, 0, 1, 9, 1, 2, 1, 0, CHROMA_NONE, 0, 0, 5, 14, 12, 0, 0, 0},      /* + A E */
    /*32*/ {P_WIDE, 5, LIT_NONE, 0, 1, 0, 0, 0, 9, 1, 2, 1, 0, CHROMA_NONE, 0, 0, 7, 14, 12, 0, 0, 0},      /* + F# C#: all seven, anywhere */
    /* E: chromatic notes in every key, and the boss */
    /*33*/ {P_CH13, 5, LIT_CHROMATIC, 0, 1, 0, 0, 0, 9, 1, 2, 1, 0, CHROMA_ANY, 1, 2, 7, 14, 12, 0, 0, 0},  /* passing, any key, lit */
    /*34*/ {P_BOSS, 6, LIT_NONE, 0, 1, 0, 0, 0, 12, 1, 2, 1, 0, CHROMA_ANY, 1, 1, 7, 16, 12, 0, 0, 0},         /* the boss */
};

/* the roots along the cycle of fifths, as keys: each in the octave where the keyboard holds the most room around it
 * (C4 G4 D4 A3 E4 F#4 C#4) */
static const uint8_t COPYCAT_ROOTS[7] = {7, 14, 9, 4, 11, 13, 8};

static inline const copycat_level_t *copycat_level(uint32_t level)   /* level is 1-based, and clamped */
{
    return &COPYCAT_LV[level < 1u ? 0u : level > COPYCAT_LEVELS ? COPYCAT_LEVELS - 1u : level - 1u];
}

static inline int32_t eo_absd(int32_t a, int32_t b) { return a > b ? a - b : b - a; }

static inline int eo_chromatic(int32_t off)     /* outside the major scale of the root? */
{
    int32_t pc = ((off % 12) + 12) % 12;
    return !(pc == 0 || pc == 2 || pc == 4 || pc == 5 || pc == 7 || pc == 9 || pc == 11);
}

/* a root for a round at a level: along the cycle, never the one just played when there is a choice. A level with one
 * root has the chosen one (given). */
static inline uint32_t copycat_pick_root(uint32_t level, uint32_t chosen, uint32_t *rng, uint32_t prev)
{
    const copycat_level_t *lv = copycat_level(level);
    uint32_t r;
    if (lv->roots <= 1u)
        return chosen;
    r = COPYCAT_ROOTS[eo_rng(rng) % lv->roots];
    while (r == prev)
        r = COPYCAT_ROOTS[eo_rng(rng) % lv->roots];
    return r;
}

/* whole-melody rules that a note-by-note walk cannot hold: the chromatic notes */
static inline int copycat_chroma_ok(const copycat_level_t *lv, const int8_t *m)
{
    uint32_t i, c = 0;
    for (i = 0; i < lv->len; i++) {
        if (!eo_chromatic(m[i]))
            continue;
        c++;
        if (lv->chroma == CHROMA_PASSING) {      /* in between two scale notes, a semitone from each */
            if (i == 0 || i + 1u >= lv->len || eo_chromatic(m[i - 1]) || eo_chromatic(m[i + 1]) ||
                eo_absd(m[i], m[i - 1]) != 1 || eo_absd(m[i], m[i + 1]) != 1)
                return 0;
        }
    }
    return c >= lv->chroma_min && c <= lv->chroma_max;
}

/* one try at a melody over a root. Steps are preferred over thirds over leaps (6:3:1); a leap, when the level asks for
 * it, is followed by a step back toward where it came from; the leaps and the span are capped. 1 when it worked. */
static inline int copycat_try(const copycat_level_t *lv, const int8_t *pool, uint32_t np, uint32_t *rng, int8_t *out)
{
    uint32_t i, leaps = 0;
    int32_t lo, hi;
    if (lv->start_root)
        out[0] = 0;
    else
        out[0] = pool[eo_rng(rng) % np];
    lo = hi = out[0];
    for (i = 1; i < lv->len; i++) {
        int8_t cand[COPYCAT_MAX_POOL];
        uint32_t w[COPYCAT_MAX_POOL], nc = 0, tot = 0, j, pick, r;
        int must_fill = lv->fill && i >= 2 && eo_absd(out[i - 1], out[i - 2]) >= COPYCAT_LEAP;
        for (j = 0; j < np; j++) {
            int32_t o = pool[j], d = eo_absd(o, out[i - 1]), nlo = o < lo ? o : lo, nhi = o > hi ? o : hi;
            if ((d == 0 && must_fill) || d > (int32_t)lv->max_d || nhi - nlo > (int32_t)lv->max_span)
                continue;
            if (must_fill) {                       /* a step, back across the gap */
                int up = out[i - 1] > out[i - 2];
                if (d > COPYCAT_STEP || (up ? o > out[i - 1] : o < out[i - 1]))
                    continue;
            }
            if (d >= COPYCAT_LEAP) {
                if (leaps >= lv->max_leaps)
                    continue;
                if (lv->chord_leaps) {             /* land on 1, 3 or 5 */
                    int32_t pc = ((o % 12) + 12) % 12;
                    if (pc != 0 && pc != 4 && pc != 7)
                        continue;
                }
            }
            cand[nc] = (int8_t)o;
            w[nc] = d == 0 ? 2u : d <= COPYCAT_STEP ? 6u : d <= COPYCAT_THIRD ? 3u : (leaps < lv->min_leaps ? 6u : 1u);
            tot += w[nc++];
        }
        if (!nc)
            return 0;
        r = eo_rng(rng) % tot;
        for (pick = 0; r >= w[pick]; pick++)
            r -= w[pick];
        if (eo_absd(cand[pick], out[i - 1]) >= COPYCAT_LEAP)
            leaps++;
        out[i] = cand[pick];
        if (out[i] < lo)
            lo = out[i];
        if (out[i] > hi)
            hi = out[i];
    }
    if (lv->intro) {                     /* a new pitch is always in the melody */
        int used = 0;
        for (i = 0; i < lv->len; i++)
            if (out[i] >= lv->intro_lo && out[i] <= lv->intro_hi)
                used = 1;
        if (!used)
            return 0;
    }
    return leaps >= lv->min_leaps && copycat_chroma_ok(lv, out);
}

static uint32_t copycat_fallbacks;       /* how often no melody was found (should stay 0) */

/* the level's notes that fit the keyboard over a root */
static inline uint32_t copycat_pool(const copycat_level_t *lv, uint32_t root, int8_t *pool)
{
    uint32_t i, np = 0;
    for (i = 0; i < lv->n; i++) {
        int32_t key = (int32_t)root + lv->off[i];
        if (key >= 0 && key <= COPYCAT_KEY_MAX)
            pool[np++] = lv->off[i];
    }
    return np;
}

/* a melody for a level over a root, as offsets in semitones (they can be below the root): len notes by the level's
 * motion rules (a note can repeat: it is played again, not held; but a leap is always answered by a step). Returns how many
 * notes it wrote. */
static inline uint32_t copycat_generate(uint32_t level, uint32_t root, uint32_t *rng, int8_t *out)
{
    const copycat_level_t *lv = copycat_level(level);
    int8_t pool[COPYCAT_MAX_POOL];
    uint32_t np = copycat_pool(lv, root, pool), t, i;
    for (t = 0; t < 400u; t++)
        if (copycat_try(lv, pool, np, rng, out))
            return lv->len;
    copycat_fallbacks++;                 /* two neighbouring notes, over and over: always playable */
    for (i = 0; i < lv->len; i++)
        out[i] = pool[i & 1u ? 1u : 0u];
    return lv->len;
}

/* the level's notes as a mask of keys, for a root key (the root itself excluded) */
static inline uint32_t copycat_pool_mask(uint32_t level, uint32_t root_key)
{
    const copycat_level_t *lv = copycat_level(level);
    uint32_t i, m = 0;
    for (i = 0; i < lv->n; i++) {
        int32_t key = (int32_t)root_key + lv->off[i];
        if (lv->off[i] && key >= 0 && key <= COPYCAT_KEY_MAX)
            m |= 1u << key;
    }
    return m;
}
