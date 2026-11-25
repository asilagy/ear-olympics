/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 asilagy */
/* Chord Dogtor: the chord qualities, the levels (which qualities you choose between), and the chord notes.
 * Each answer has its own key, always the same one: A F4, B G4, C A4, D B4 (white keys, so they are easy to find). */
#pragma once
#include <stdint.h>
enum { Q_MAJOR, Q_MINOR, Q_DIM, Q_AUG, Q_MAJ7, Q_MIN7, Q_DOM7, Q_SUS2, Q_SUS4, Q_NQ };
#define DOGTOR_LEVELS 6u
#define DOGTOR_ROUNDS 8u
#define DOGTOR_MAX_CHOICES 4u
static const char *const dt_q_name[Q_NQ] = {"MAJOR", "MINOR", "DIM", "AUG", "MAJ7", "MIN7", "DOM7", "SUS2", "SUS4"};
static const int8_t dt_q_iv[Q_NQ][4] = {            /* semitones above the root; -1 ends the list */
    {0, 4, 7, -1}, {0, 3, 7, -1}, {0, 3, 6, -1}, {0, 4, 8, -1}, {0, 4, 7, 11}, {0, 3, 7, 10}, {0, 4, 7, 10},
    {0, 2, 7, -1}, {0, 5, 7, -1}};
typedef struct {
    uint8_t n;                                         /* how many choices */
    uint8_t q[DOGTOR_MAX_CHOICES];                     /* which qualities, in key order A B C D */
    uint8_t moves;                                     /* 1: the root changes every round */
    const char *name;
} dogtor_level_t;
static const dogtor_level_t dt_level_tab[DOGTOR_LEVELS] = {
    {2, {Q_MAJOR, Q_MINOR}, 0, "Happy or sad"},
    {3, {Q_MAJOR, Q_MINOR, Q_DIM}, 0, "A tense third option"},
    {4, {Q_MAJOR, Q_MINOR, Q_DIM, Q_AUG}, 0, "The four triads"},
    {4, {Q_MAJOR, Q_MINOR, Q_DIM, Q_AUG}, 1, "Triads, any root"},
    {4, {Q_MAJ7, Q_MIN7, Q_DOM7, Q_DIM}, 1, "Sevenths"},
    {4, {Q_MAJOR, Q_MINOR, Q_SUS2, Q_SUS4}, 1, "Suspended"},
};
static const uint8_t dt_answer_key[DOGTOR_MAX_CHOICES] = {12, 14, 16, 18};
static const dogtor_level_t *dogtor_level(uint32_t l) { return &dt_level_tab[(l >= 1u && l <= DOGTOR_LEVELS ? l : 1u) - 1u]; }
static uint32_t dogtor_key_mask(uint32_t l)            /* the answer keys this level uses, as a bit per key */
{
    uint32_t i, m = 0;
    for (i = 0; i < dogtor_level(l)->n; i++)
        m |= 1u << dt_answer_key[i];
    return m;
}
