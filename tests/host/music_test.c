/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 asilagy */
/* Ear Olympics' music, natively: every level of both games, many seeds. cc -std=c99 -Wall -Wextra -Werror */
#include <stdio.h>
#include "../../app/eo_copycat.h"
#include "../../app/eo_simon.h"

static int fails;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL " __VA_ARGS__); printf("\n"); fails++; } } while (0)

static uint32_t repeats;                 /* repeated notes in melodies, over every level */

int main(void)
{
    uint32_t level, seed, i, j;
    /* Copycat */
    for (level = 1; level <= COPYCAT_LEVELS; level++) {
        const copycat_level_t *lv = copycat_level(level);
        uint32_t seen[COPYCAT_MAX_POOL] = {0}, leapy = 0, withleap = 0, withchroma = 0, r;
        uint32_t total = 0;
        CHECK(lv->len <= COPYCAT_MAX_LEN && lv->n <= COPYCAT_MAX_POOL && lv->n >= 3, "copycat %u: shape", level);
        CHECK(lv->roots >= 1 && lv->roots <= 7, "copycat %u: roots", level);
        {   /* every root the level uses, with every seed */
            uint32_t roots[8], nr = 0;
            if (lv->roots == 1) {
                for (r = 7; r <= 14; r++)           /* the chosen root: C4 to G4 */
                    roots[nr++] = r;
            } else
                for (r = 0; r < lv->roots; r++)
                    roots[nr++] = COPYCAT_ROOTS[r];
            for (r = 0; r < nr; r++)
                for (seed = 1; seed <= 400; seed++) {
                    uint32_t rng = seed * 7u + r, n;
                    int8_t m[COPYCAT_MAX_LEN];
                    uint32_t leaps = 0, nch = 0;
                    int32_t lo = 99, hi = -99;
                    n = copycat_generate(level, roots[r], &rng, m);
                    total++;
                    CHECK(n == lv->len, "copycat %u: length %u, wanted %u", level, n, lv->len);
                    CHECK(!lv->start_root || m[0] == 0, "copycat %u: must start on the root", level);
                    for (i = 0; i < n; i++) {
                        int in_pool = 0;
                        int32_t key = (int32_t)roots[r] + m[i];
                        for (j = 0; j < lv->n; j++)
                            if (m[i] == lv->off[j]) {
                                in_pool = 1;
                                seen[j]++;
                            }
                        CHECK(in_pool, "copycat %u: note %u (%d) outside the pool", level, i, m[i]);
                        CHECK(key >= 0 && key <= COPYCAT_KEY_MAX, "copycat %u root %u: key %d off the keyboard", level,
                              roots[r], key);
                        if (i && m[i] == m[i - 1])
                            repeats++;
                        lo = m[i] < lo ? m[i] : lo;
                        hi = m[i] > hi ? m[i] : hi;
                        nch += (uint32_t)eo_chromatic(m[i]);
                        if (i == 0)
                            continue;
                        {
                            int32_t d = eo_absd(m[i], m[i - 1]);
                            CHECK(d <= (int32_t)lv->max_d, "copycat %u: jump %d too big", level, d);
                            if (d >= COPYCAT_LEAP) {
                                leaps++;
                                leapy++;
                                if (lv->chord_leaps) {
                                    int32_t pc = ((m[i] % 12) + 12) % 12;
                                    CHECK(pc == 0 || pc == 4 || pc == 7, "copycat %u: leap to a non-chord tone", level);
                                }
                            }
                            if (lv->fill && i >= 2 && eo_absd(m[i - 1], m[i - 2]) >= COPYCAT_LEAP) {
                                CHECK(d <= COPYCAT_STEP, "copycat %u: leap not followed by a step", level);
                                CHECK((m[i - 1] > m[i - 2]) ? m[i] < m[i - 1] : m[i] > m[i - 1],
                                      "copycat %u: the step goes on instead of filling the gap", level);
                            }
                        }
                    }
                    CHECK(leaps <= lv->max_leaps && leaps >= lv->min_leaps, "copycat %u: %u leaps", level, leaps);
                    CHECK(hi - lo <= (int32_t)lv->max_span, "copycat %u: span %d", level, hi - lo);
                    CHECK(nch >= lv->chroma_min && nch <= lv->chroma_max, "copycat %u: %u chromatic notes", level, nch);
                    withleap += leaps > 0;
                    withchroma += nch > 0;
                }
        }
        for (j = 0; j < lv->n; j++)
            CHECK(seen[j] > 0 || (lv->start_root && lv->max_leaps == 0) ||   /* a steps-only walk from the root */
                  (lv->intro && lv->len <= 3u),                              /* a short melody built around a new pitch */
                  "copycat %u: pool note %d never drawn", level, lv->off[j]);
        CHECK(lv->max_leaps == 0 ? leapy == 0 : withleap > 0, "copycat %u: leaps appear only where allowed", level);
        CHECK(lv->chroma == CHROMA_NONE ? withchroma == 0 : withchroma == total, "copycat %u: chromatic notes", level);
        printf("  ok   copycat level %2u: %u notes from %2u, %u root%s, %u melodies, %u%% with a leap\n", level,
               lv->len, lv->n, lv->roots, lv->roots == 1 ? "" : "s", total, withleap * 100u / total);
    }
    CHECK(COPYCAT_LV[0].lit == LIT_ALL && COPYCAT_LV[1].lit == LIT_NONE && COPYCAT_LV[2].lit == LIT_ALL &&
              COPYCAT_LV[3].lit == LIT_NONE,
          "the lights come back for new pitches (level 3) and go off again (level 4)");
    for (level = 1; level <= COPYCAT_LEVELS; level++) {   /* a level that brings in new pitches is lit and uses one every time */
        const copycat_level_t *lv = copycat_level(level);
        if (lv->intro) {
            uint32_t sd, hit = 0, all = 0;
            CHECK(lv->lit == LIT_ALL, "copycat %u: an intro level is lit", level);
            for (sd = 1; sd <= 400; sd++) {
                int8_t m[COPYCAT_MAX_LEN];
                uint32_t rng = sd * 2654435761u, k, used = 0;
                copycat_generate(level, 7, &rng, m);
                for (k = 0; k < lv->len; k++)
                    used |= m[k] >= lv->intro_lo && m[k] <= lv->intro_hi;
                hit += used;
                all++;
            }
            CHECK(hit == all, "copycat %u: a new pitch is in every melody (%u of %u)", level, hit, all);
        }
    }
    {   /* The ladder is gentle: a rough difficulty score per level may dip a little where a crutch is taken away,
         * but never falls far and never jumps far. (A weighing of what makes a melody hard to hold and copy.) */
        double prev = 0, first = 0, last = 0;
        for (level = 1; level <= COPYCAT_LEVELS; level++) {
            const copycat_level_t *lv = copycat_level(level);
            double d = lv->len + 0.25 * lv->n + 0.12 * lv->max_span;
            d += lv->min_leaps * (lv->chord_leaps ? 1.8 : 2.5) + (lv->max_leaps - lv->min_leaps) * 1.0;
            d += lv->chroma == CHROMA_PASSING ? 2.5 : lv->chroma == CHROMA_ANY ? 3.5 : 0.0;
            d += lv->drone ? 0.0 : 2.0;
            d += (!lv->drone && !lv->start_root) ? 1.5 : 0.0;
            d += lv->roots > 1 ? 1.0 + 0.5 * (lv->roots - 1) : 0.0;
            d += lv->lit == LIT_NONE ? 1.5 : lv->lit == LIT_CHROMATIC ? 0.5 : 0.0;
            d += lv->off[0] < 0 ? 1.5 : 0.0;
            d -= lv->contour ? 1.5 : 0.0;
            d -= lv->hear > 1 ? 1.5 : 0.0;
            if (level == 1)
                first = d;
            else {
                CHECK(d >= prev - 4.5, "copycat %u: difficulty falls from %.1f to %.1f", level, prev, d);
                CHECK(d <= prev + (level == COPYCAT_LEVELS ? 5.2 : 3.6), "copycat %u: difficulty jumps from %.1f to %.1f", level, prev, d);
            }
            printf("       level %2u: difficulty %5.1f, %u notes, %u rounds\n", level, d, lv->len, lv->rounds);
            CHECK(lv->rounds >= 8 && lv->rounds <= 12, "copycat %u: rounds", level);
            prev = last = d;
        }
        CHECK(last > first + 12.0, "the ladder climbs");
    }
    CHECK(repeats > 1000u, "melodies do repeat notes (%u)", repeats);
    CHECK(copycat_fallbacks == 0, "copycat: the generator never fell back (%u)", copycat_fallbacks);
    for (level = 30; level <= 32; level++)   /* the moving roots only ever multiply */
        CHECK(copycat_level(level)->roots >= copycat_level(level - 1)->roots, "copycat %u: roots go back", level);
    CHECK(copycat_pool_mask(1, 7) == ((1u << 9) | (1u << 11)), "copycat pool mask");
    CHECK(copycat_pool_mask(26, 7) == ((1u << 2) | (1u << 4) | (1u << 6) | (1u << 9) | (1u << 11) | (1u << 12) |
                                      (1u << 14) | (1u << 16) | (1u << 18) | (1u << 19)),
          "copycat wide pool mask");
    {   /* moving roots: along the cycle, never the same twice running */
        uint32_t rng = 3, prev = 0, k, seenr[7] = {0};
        for (k = 0; k < 500; k++) {
            uint32_t r = copycat_pick_root(32, 7, &rng, prev);
            CHECK(prev == 0 || r != prev, "root repeats");
            for (i = 0; i < 7; i++)
                seenr[i] += r == COPYCAT_ROOTS[i];
            prev = r;
        }
        for (i = 0; i < 7; i++)
            CHECK(seenr[i] > 0, "root %u never picked", i);
        CHECK(copycat_pick_root(8, 9, &rng, 7) == 9, "a one-root level keeps the chosen root");
        for (k = 0; k < 100; k++)
            CHECK(copycat_pick_root(29, 7, &rng, 14) != 14 && copycat_pick_root(29, 7, &rng, 14) <= 14, "three roots");
    }
    printf("  ok   copycat masks and roots\n");
    /* Simon */
    for (level = 1; level <= SIMON_LEVELS; level++) {
        const simon_level_t *lv = simon_level(level);
        uint32_t seen[SIMON_MAX_POOL] = {0}, rng = 5, prev = 255;
        CHECK(lv->n >= 4 && lv->n <= SIMON_MAX_POOL, "simon %u: shape", level);
        for (i = 0; i < 20000; i++) {
            uint8_t o = simon_next(level, &rng, prev);
            int in_pool = 0;
            for (j = 0; j < lv->n; j++)
                if (o == lv->off[j]) {
                    in_pool = 1;
                    seen[j]++;
                }
            CHECK(in_pool, "simon %u: note %u outside the pool", level, o);
            CHECK(o != prev, "simon %u: note repeats", level);
            prev = o;
        }
        for (j = 0; j < lv->n; j++)
            CHECK(seen[j] > 0, "simon %u: pool note %u never drawn", level, lv->off[j]);
        for (j = 0; j < lv->n; j++)
            CHECK(7u + lv->off[j] <= 26u, "simon %u: note %u off the keyboard", level, lv->off[j]);
        printf("  ok   simon level %u: %u keys\n", level, lv->n);
    }
    CHECK(simon_step_ms(1, 1) == 650 && simon_step_ms(4, 1) == 650 && simon_step_ms(5, 1) == 620, "simon tempo start");
    CHECK(simon_step_ms(20, 1) == 300 && simon_step_ms(40, 4) == 300, "simon tempo floor");
    for (level = 1; level <= SIMON_LEVELS; level++)
        for (i = 2; i <= SIMON_MAX_LEN; i++)
            CHECK(simon_step_ms(i, level) <= simon_step_ms(i - 1, level), "simon tempo never slows at %u", i);
    for (level = 2; level <= SIMON_LEVELS; level++)
        CHECK(simon_step_ms(6, level) < simon_step_ms(6, level - 1), "simon level %u is quicker", level);
    CHECK(simon_note_ms(300) >= 160 && simon_note_ms(650) < 650, "simon note length");
    CHECK(simon_pool_mask(1, 7) == ((1u << 7) | (1u << 9) | (1u << 11) | (1u << 14)), "simon pool mask");
    CHECK(simon_lit_count(1) >= 20 && simon_lit_count(2) >= 20 && simon_lit_count(3) == 3 && simon_lit_count(4) == 1, "simon lights fade at levels 3 and 4");
    CHECK(simon_goal(1) == 4 && simon_goal(2) == 5 && simon_goal(3) == 6 && simon_goal(4) == 7, "simon goals 4 5 6 7");
    for (level = 1; level <= SIMON_LEVELS; level++)
        CHECK(simon_pool_mask(level, 7) == simon_pool_mask(1, 7), "simon %u keeps the same four keys", level);
    CHECK(simon_word_ix(0) == 0 && simon_word_ix(2) == 1 && simon_word_ix(4) == 2 && simon_word_ix(7) == 3, "simon words");
    CHECK(simon_popped(1, 0, 6) == 0 && simon_popped(1, 2599, 6) == 0 && simon_popped(1, 2600, 6) == 1, "simon first pop after the grace");
    CHECK(simon_popped(1, 2600 + 900, 6) == 2 && simon_popped(1, 99999, 6) == 6, "simon pops oldest first, capped at the length");
    CHECK(simon_popped(4, 800, 9) == 1 && simon_popped(4, 800 + 320 * 3, 9) == 4, "simon pops faster at level 4");
    for (level = 2; level <= SIMON_LEVELS; level++)
        CHECK(SIMON_POP_GRACE[level - 1] < SIMON_POP_GRACE[level - 2] && SIMON_POP_EVERY[level - 1] < SIMON_POP_EVERY[level - 2], "simon pops sooner at level %u", level);
    printf("  ok   simon tempo and masks\n");
    {   /* the same seed, the same melody */
        uint32_t a = 77, b = 77;
        int8_t ma[COPYCAT_MAX_LEN], mb[COPYCAT_MAX_LEN];
        copycat_generate(3, 7, &a, ma);
        copycat_generate(3, 7, &b, mb);
        for (i = 0; i < COPYCAT_LV[2].len; i++)
            CHECK(ma[i] == mb[i], "determinism at %u", i);
        printf("  ok   determinism\n");
    }
    return fails ? 1 : 0;
}
