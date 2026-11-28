/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 asilagy */
/* Name That Interval: Van's hangout. Included into app.c (one unity build) just before the select's "go".
 * Mostly chat: Van talks, a card at a time (any key goes on). Every few beats he measures two notes and you name how far apart
 * they are on the lit answer keys. Nothing is ever lost: a wrong answer is a shrug and the right key lights. */
#include "eo_interval.h"

enum { VM_CARDS, VM_ASK, VM_CHOICE, VM_HUM };                        /* what the keys do now */
enum { VN_BEAT, VN_ASK, VN_CHOICE, VN_HUM };                         /* what comes when the cards run out */
#define VAN_CARDS 6u
#define VAN_LINES 10u
#define VAN_CPS 30u                                                  /* the typing speed: letters a second */
#define VAN_HUM_MS 10000u
#define VAN_SNACK_MS 40000u                                          /* this long on a question: Van wonders if it is a snack break */

static uint8_t van_mode, van_after, van_chips, van_cn, van_ci, van_pose;
static const char *van_cd[VAN_CARDS];
static uint32_t van_t0, van_typed_last, van_hum_t0, van_ask_t0, van_ev_at;
static uint8_t van_ev;                                               /* 1: play the interval again at van_ev_at */
static uint8_t van_since, van_snacked, van_first_q, van_cur;         /* beats since a question; the beat now; ... */
static uint8_t van_recent[6];
static uint8_t van_ls[VAN_LINES], van_ll[VAN_LINES], van_ln;         /* the card, wrapped */
static uint8_t van_q_semi, van_q_n, van_q_right, van_q_pick, van_last_semi;
static uint8_t van_q_semis[VAN_MAX_KEYS];
static const char *van_lbl[VAN_MAX_KEYS];                            /* the chips' names */
static char van_nbuf[3][44];
static uint8_t van_maxlines;

static uint32_t van_rnd(uint32_t n)
{
    eo_seed_once();
    eo_seed = eo_seed * 1664525u + 1013904223u;
    return (eo_seed >> 16) % n;
}
static uint32_t van_mask(uint32_t n)     /* the answer keys that are lit */
{
    uint32_t m = 0, i;
    for (i = 0; i < n && i < VAN_MAX_KEYS; i++)
        m |= 1u << van_answer_key[i];
    return m;
}

/* ---- the card: its words, wrapped to the dialogue box, typed out letter by letter */
static const char *van_text(const char *c) { return (c[0] == '*' || c[0] == '#') ? c + 1 : c; }
static uint32_t van_kind(const char *c) { return c[0] == '*' ? 1u : c[0] == '#' ? 2u : 0u; }
static uint32_t van_wrap_into(const char *s, uint8_t *ls, uint8_t *ll)   /* returns the line count (up to VAN_LINES) */
{
    uint32_t len = eo_strlen(s), i = 0, n = 0;
    while (i < len && n < VAN_LINES) {
        uint32_t st, j, best = 0;
        while (i < len && s[i] == ' ')
            i++;
        if (i >= len)
            break;
        st = j = i;
        while (j < len) {
            uint32_t e = j, m = 0, k;
            char buf[64];
            while (e < len && s[e] != ' ')
                e++;
            for (k = st; k < e && m < 62u; k++)
                buf[m++] = s[k] == '~' ? 'e' : s[k];
            buf[m] = 0;
            if (best && text_w(&FONT_XS, buf) > 220)
                break;
            best = e;
            j = e;
            while (j < len && s[j] == ' ')
                j++;
        }
        ls[n] = (uint8_t)st;
        ll[n] = (uint8_t)(best - st);
        n++;
        i = j;
    }
    return n;
}
static void van_card_start(uint32_t now)
{
    van_ln = (uint8_t)van_wrap_into(van_text(van_cd[van_ci]), van_ls, van_ll);
    van_t0 = now;
    van_typed_last = 0;
    ui.redraw = 1;
}
static uint32_t van_len(void)
{
    return van_kind(van_cd[van_ci]) == 2u ? 26u : eo_strlen(van_text(van_cd[van_ci]));
}
static uint32_t van_typed(uint32_t now)
{
    uint32_t n = (now - van_t0) * VAN_CPS / 1000u, l = van_len();
    return n > l ? l : n;
}
static void van_say(uint32_t pose, uint32_t after, const char *a, const char *b, const char *c, const char *d)
{
    const char *l[4] = {a, b, c, d};
    uint32_t i;
    van_cn = 0;
    for (i = 0; i < 4u; i++)
        if (l[i] && l[i][0])
            van_cd[van_cn++] = l[i];
    van_ci = 0;
    van_pose = (uint8_t)pose;
    van_after = (uint8_t)after;
    van_mode = VM_CARDS;
    van_chips = 0;
    van_card_start(plat_ms());
}

/* ---- the sounds */
static void van_play_pair(uint32_t root, uint32_t semi)    /* two notes, root then the other: warm, never lit */
{
    seq_on = 0;
    mv_sfx(0, 80, 640, root, V_WARM);
    mv_sfx(1, 860, 760, root + semi, V_WARM);
    seq_n = 2;
    seq_end = 1700;
    eo_seq_start(0, 0);
}
static void van_blip(uint32_t n)         /* a bubble for a letter: low, rounded, a small tune on a pentatonic scale */
{
    static const uint8_t SC[5] = {0, 2, 4, 7, 9};
    eo_blub_soft = 1;
    eo_blub_note = 52 + SC[(n * 3u + van_ci * 2u) % 5u] + ((n % 7u) == 0u ? 5 : 0);
    eo_blub_trig++;
}
static void van_stop_sound(void)
{
    seq_on = 0;
    seq_lit = 0;
    voice_note = -1;
    voice_fx = 0xFFu;
    eo_drone_set(-1, 0.0f);
}

/* ---- keys and lights */
static void van_key_levels(uint32_t *bright, uint32_t *mid, uint32_t *dim)
{
    *mid = 0u;
    *dim = 0u;
    *bright = seq_on ? seq_lit : ui.keys;
    if (van_mode == VM_ASK || (van_mode == VM_CARDS && van_chips))
        *dim = van_mask(van_q_n);
    if (van_mode == VM_CHOICE)
        *dim = van_mask(van_q_n);
    if (van_chips == 2u) {
        *bright |= 1u << van_answer_key[van_q_right];
        if (van_q_pick != van_q_right && van_q_pick < van_q_n)
            *mid |= 1u << van_answer_key[van_q_pick];
    }
}

/* ---- a question */
static void van_question(void)
{
    const van_level_t *lv = van_level(set.van_lvl);
    uint32_t n = lv->keys, pn = lv->pn, i, j, root = set.root;
    uint8_t cand[12], cn = 0, t;
    uint32_t correct;
    do
        correct = lv->pool[van_rnd(pn)];
    while (correct == van_last_semi && pn > 1u && van_rnd(4u));   /* mostly not the same one twice */
    van_last_semi = (uint8_t)correct;
    for (i = 0; i < pn; i++)
        if (lv->pool[i] != correct)
            cand[cn++] = lv->pool[i];
    if (lv->close) {                     /* the wrong answers are the nearest neighbours: sort the others by distance */
        for (i = 1; i < cn; i++)
            for (j = i; j > 0; j--) {
                int32_t da = (int32_t)cand[j - 1u] - (int32_t)correct, db = (int32_t)cand[j] - (int32_t)correct;
                da = da < 0 ? -da : da;
                db = db < 0 ? -db : db;
                if (db < da) {
                    t = cand[j];
                    cand[j] = cand[j - 1u];
                    cand[j - 1u] = t;
                }
            }
    } else {                             /* any others, shuffled */
        for (i = cn; i > 1u; i--) {
            j = van_rnd(i);
            t = cand[i - 1u];
            cand[i - 1u] = cand[j];
            cand[j] = t;
        }
    }
    van_q_n = (uint8_t)n;
    van_q_semis[0] = (uint8_t)correct;
    for (i = 1; i < n; i++)
        van_q_semis[i] = cand[i - 1u];
    for (i = 1; i < n; i++)              /* the keys go from the smallest interval to the biggest */
        for (j = i; j > 0 && van_q_semis[j] < van_q_semis[j - 1u]; j--) {
            t = van_q_semis[j];
            van_q_semis[j] = van_q_semis[j - 1u];
            van_q_semis[j - 1u] = t;
        }
    for (i = 0; i < n; i++) {
        van_lbl[i] = van_iv[van_q_semis[i] - 1u].name;
        if (van_q_semis[i] == correct)
            van_q_right = (uint8_t)i;
    }
    van_q_semi = (uint8_t)correct;
    van_q_pick = 0xFFu;
    van_snacked = 0;
    (void)root;
    van_say(P_TAPE, VN_ASK, van_first_q ? van_lead[0] : van_lead[1u + van_rnd(3u)], van_first_q ? van_lead[3] : 0, 0, 0);
    van_first_q = 0;
}

static void van_ask_now(void)            /* the cards are done: the question stands, the keys are lit, the notes sound */
{
    van_mode = VM_ASK;
    van_chips = 1;
    van_ask_t0 = plat_ms();
    van_play_pair(set.root, van_q_semi);
    ui.redraw = 1;
}

/* ---- the beats */
static void van_hum_start(void)
{
    van_mode = VM_HUM;
    van_chips = 0;
    van_hum_t0 = plat_ms();
    set.van_seen |= 2u;
    set_dirty = 1;
    eo_drone_set(KEY_NOTE0 + (int32_t)ROOT_MIN - 12, 0.22f);
    ui.redraw = 1;
}

static void van_next_beat(void)
{
    const van_beat_t *b;
    uint32_t i, tries, ok;
    if (set_dirty && !seq_on)
        (void)store_now();
    if (van_since >= VAN_BEATS) {
        van_since = 0;
        van_question();
        return;
    }
    for (tries = 0; tries < 16u; tries++) {
        van_cur = (uint8_t)van_rnd(VAN_NBEATS);
        ok = !(van_beat[van_cur].kind == K_HUM && (set.van_seen & 2u));
        for (i = 0; i < 6u; i++)
            if (van_recent[i] == van_cur)
                ok = 0;
        if (ok)
            break;
    }
    for (i = 5; i > 0; i--)
        van_recent[i] = van_recent[i - 1u];
    van_recent[0] = van_cur;
    van_since++;
    b = &van_beat[van_cur];
    if (b->kind == K_MEOW) {             /* a meow from the next room, then Van explains */
        seq_on = 0;
        mv_sfx(0, 0, 520, 14, V_CATFX);
        seq_n = 1;
        seq_end = 600;
        eo_seq_start(0, 0);
    }
    van_say(b->pose, b->kind == K_ASK ? VN_CHOICE : b->kind == K_HUM ? VN_HUM : VN_BEAT, b->c[0], b->c[1], b->c[2], b->c[3]);
}

static void van_choice_now(void)         /* an ask beat: pick a reply (lit keys) */
{
    const van_beat_t *b = &van_beat[van_cur];
    uint32_t i;
    van_mode = VM_CHOICE;
    van_chips = 0;
    van_q_n = 0;
    for (i = 0; i < 3u && b->ch[i]; i++) {
        van_lbl[i] = b->ch[i];
        van_q_n++;
    }
    van_ask_t0 = plat_ms();
    ui.redraw = 1;
}

static void van_done_cards(void)
{
    uint32_t a = van_after;
    if (a == VN_ASK)
        van_ask_now();
    else if (a == VN_CHOICE)
        van_choice_now();
    else if (a == VN_HUM)
        van_hum_start();
    else
        van_next_beat();
}

static void van_answer(uint32_t ix)
{
    uint32_t i, right = ix == van_q_right, rt = set.root;
    char *p;
    van_q_pick = (uint8_t)ix;
    van_chips = 2;
    seq_on = 0;
    if (right) {
        mv_sfx(0, 0, 300, rt + 24u, V_PING);
        seq_n = 1;
        seq_end = 420;
        eo_seq_start(0, 0);
        if (set.van_prog < VAN_ROUNDS)
            set.van_prog++;
        set_dirty = 1;
    } else {
        mv_sfx(0, 0, 220, rt + 7u, V_TRI);
        mv_sfx(1, 240, 420, rt + 3u, V_TRI);
        seq_n = 2;
        seq_end = 700;
        eo_seq_start(0, 0);
        van_ev = 1;                      /* then the pair again, so you hear what it was */
        van_ev_at = plat_ms() + 1300u;
    }
    p = van_nbuf[0];
    p[0] = 0;
    text_cat(p, van_iv[van_q_semi - 1u].name);
    text_cat(p, ".");
    if (right && set.van_prog >= VAN_ROUNDS && set.van_lvl < VAN_LEVELS) {    /* the level is done */
        const van_level_t *ol = van_level(set.van_lvl), *nl = van_level(set.van_lvl + 1u);
        if (set.van_top < set.van_lvl)
            set.van_top = set.van_lvl;
        set.van_lvl++;
        set.van_prog = 0;
        set_dirty = 1;
        van_nbuf[1][0] = 0;
        text_cat(van_nbuf[1], "*New on the tape: ");
        for (i = ol->pn; i < nl->pn; i++) {
            text_cat(van_nbuf[1], van_iv[nl->pool[i] - 1u].name);
            text_cat(van_nbuf[1], i + 1u < nl->pn ? ", " : ".");
        }
        van_say(P_LAUGH, VN_BEAT, van_nbuf[0], van_right[van_rnd(5u)], van_up[van_rnd(2u)], van_nbuf[1]);
    } else if (right && set.van_prog >= VAN_ROUNDS) {
        if (set.van_top < set.van_lvl) {
            set.van_top = set.van_lvl;
            set_dirty = 1;
        }
        van_say(P_LAUGH, VN_BEAT, van_nbuf[0], van_right[van_rnd(5u)], van_iv[van_q_semi - 1u].joke, 0);
    } else {
        van_say(right ? P_LAUGH : P_COOKED, VN_BEAT, van_nbuf[0], right ? van_right[van_rnd(5u)] : van_wrong[van_rnd(5u)],
                van_iv[van_q_semi - 1u].joke, 0);
    }
    van_chips = 2;
}

static void van_choose(uint32_t ix)      /* an ask beat: your pick, Van's reply */
{
    const van_beat_t *b = &van_beat[van_cur];
    van_say(P_TALK, VN_BEAT, b->re[ix], 0, 0, 0);
    seq_on = 0;
    mv_sfx(0, 0, 180, 12u + ix * 2u, V_TRI);
    seq_n = 1;
    seq_end = 260;
    eo_seq_start(0, 0);
}

static void van_enter(void)
{
    uint32_t seen = set.van_seen & 1u;
    van_stop_sound();
    van_since = 0;
    van_first_q = 1;
    van_snacked = 0;
    van_ev = 0;
    van_chips = 0;
    van_q_n = 0;
    {
        uint32_t i;
        for (i = 0; i < 6u; i++)
            van_recent[i] = 0xFFu;
    }
    ui.state = ST_VAN;
    ui.text = TX_READY;
    if (seen) {
        van_say(P_TALK, VN_BEAT, van_again, 0, 0, 0);
    } else {
        set.van_seen |= 1u;
        set_dirty = 1;
        van_say(P_IDLE, VN_BEAT, van_intro[0], van_intro[1], van_intro[2], van_intro[3]);
    }
}

static void van_end(void)
{
    eo_blub_soft = 0;
    van_stop_sound();
    van_ev = 0;
    if (set_dirty)
        (void)store_now();
    sel_enter();
    sel_cur = 3;
}

static void van_key(uint32_t new_keys, uint32_t new_btn)
{
    uint32_t now = plat_ms(), i;
    if (new_btn & (1u << B_HOME)) {
        van_end();
        return;
    }
    if (van_mode == VM_CARDS) {
        if (!(new_keys || new_btn))
            return;
        if (van_typed(now) < van_len()) {
            van_t0 = now - 60000u;       /* finish the line */
        } else if (van_ci + 1u < van_cn) {
            van_ci++;
            van_card_start(now);
        } else {
            van_done_cards();
        }
    } else if (van_mode == VM_ASK) {
        if (new_btn & (1u << B_PLAY)) {
            van_play_pair(set.root, van_q_semi);
            van_ev = 0;
            return;
        }
        for (i = 0; i < van_q_n; i++)
            if (new_keys & (1u << van_answer_key[i]))
                van_answer(i);
    } else if (van_mode == VM_CHOICE) {
        for (i = 0; i < van_q_n; i++)
            if (new_keys & (1u << van_answer_key[i])) {
                van_choose(i);
                return;
            }
    } else if (van_mode == VM_HUM) {
        if ((new_keys || new_btn) && now - van_hum_t0 > 3000u) {
            eo_drone_set(-1, 0.0f);
            van_say(P_IDLE, VN_BEAT, van_hum_after[0], van_hum_after[1], 0, 0);
        }
    }
}

static void van_level_step(int32_t d)    /* K1: another level, up to three above the highest you have completed */
{
    int32_t m = (int32_t)set.van_top + (int32_t)EO_UNLOCK, l;
    if (set.pad & 1u)
        m = VAN_LEVELS;
    if (m > (int32_t)VAN_LEVELS)
        m = VAN_LEVELS;
    l = clamp((int32_t)set.van_lvl + d, 1, m);
    if (l == (int32_t)set.van_lvl)
        return;
    set.van_lvl = (uint8_t)l;
    set.van_prog = 0;
    set_dirty = 1;
    if (van_mode == VM_ASK || van_mode == VM_CARDS) {
        van_stop_sound();
        van_ev = 0;
        van_since = VAN_BEATS;           /* a fresh question at the new level, right after the next card */
        van_question();
    }
}

static void van_tick(uint32_t now)
{
    if (van_mode == VM_CARDS) {
        uint32_t n = van_typed(now);
        if (n != van_typed_last) {
            if (n > van_typed_last && van_kind(van_cd[van_ci]) != 2u && van_text(van_cd[van_ci])[n - 1u] != ' ')
                van_blip(n);
            van_typed_last = n;
        }
    }
    if (van_ev && now >= van_ev_at) {
        van_ev = 0;
        van_play_pair(set.root, van_q_semi);
    }
    if (van_mode == VM_ASK && !van_snacked && now - van_ask_t0 > VAN_SNACK_MS && !seq_on) {
        van_snacked = 1;
        van_ev = 0;
        van_cd[0] = van_snack;
        van_cn = 1;
        van_ci = 0;
        van_mode = VM_CARDS;
        van_after = VN_ASK;
        van_chips = 1;
        van_card_start(now);
    }
    if (van_mode == VM_HUM && now - van_hum_t0 > VAN_HUM_MS) {
        eo_drone_set(-1, 0.0f);
        van_say(P_IDLE, VN_BEAT, van_hum_after[0], van_hum_after[1], 0, 0);
    }
}

/* ---- the screen */
static void van_gibber(int32_t y, uint32_t n, uint32_t now)   /* Van's own symbols: little glyphs, shifting */
{
    uint32_t i, r, k;
    for (i = 0; i < n && i < 26u; i++) {
        uint32_t h = (i * 2654435761u + (now / 140u) * 40503u) | 1u;
        for (k = 0; k < 9u; k++) {
            r = (h >> (k * 3u)) & 7u;
            if (r > 3u)
                mrect(10 + (int32_t)i * 8 + (int32_t)(k % 3u) * 2, y + (int32_t)(k / 3u) * 3, 2, 2, C_AMB);
        }
    }
}
static uint16_t van_tint[16];
static int32_t van_tint_px(const void *s, int32_t x, int32_t y)
{
    uint32_t i = (uint32_t)y * EO_CAST_W + (uint32_t)x, b = ((const uint8_t *)s)[i >> 1], v = (i & 1u) ? b & 15u : b >> 4;
    return v ? (int32_t)van_tint[v] : -1;
}
static void van_paint(uint32_t pose, int32_t ox, int32_t oy, int32_t num, int32_t den, uint32_t now, uint32_t talking)
{
    uint32_t alt = talking && ((now / 140u) & 1u);
    if (!eo_res_ok())
        return;
    spr_pal = EO_CAST_PAL[2];
    if (pose == P_IDLE || pose == P_LOUNGE || pose == P_FISH)
        spr_paint(cast_px, EO_CAST[2][pose == P_LOUNGE ? 1u : alt ? 1u : 0u], EO_CAST_W, EO_CAST_H, ox, oy, num, den);
    else
        spr_paint(cast_px, EO_VAN[pose - P_TALK], EO_CAST_W, EO_CAST_H, ox, oy, num, den);
}
static void van_room(void)
{
    int32_t y;
    const uint8_t *rle = EO_ROOM_RLE;
    const uint16_t *row = EO_ROOM_ROW, *pal = EO_ROOM_PAL;
    if (!eo_res_ok())
        return;
    for (y = 0; y < (int32_t)EO_ROOM_ROWS; y++) {
        int32_t py = 36 + y - mv_oy, x = 0;
        const uint8_t *p = rle + row[y], *e = rle + row[y + 1];
        if (py < 0 || py >= mv_h)
            continue;
        while (p < e) {
            if (p[0])
                cv_rect(x, py, p[1], 1, pal[p[0]]);
            x += p[1];
            p += 2;
        }
    }
}
static void van_chip_row(uint32_t now)
{
    uint32_t i, n = van_q_n;
    int32_t w = n ? (232 - (int32_t)(n - 1u) * 4) / (int32_t)n : 0;
    for (i = 0; i < n; i++) {
        int32_t x = 4 + (int32_t)i * (w + 4), tw;
        uint16_t bg = EO_RGB(30, 30, 50), fg = C_WHITE, ed = EO_RGB(120, 120, 160);
        if (van_chips == 2u) {
            if (i == van_q_right) {
                bg = EO_RGB(30, 110, 60);
                ed = EO_RGB(120, 230, 150);
            } else if (i == van_q_pick) {
                bg = EO_RGB(100, 40, 40);
                ed = EO_RGB(220, 120, 120);
            }
        } else if (((now / 500u) & 1u) && van_mode == VM_ASK && seq_on) {
            ed = EO_RGB(200, 200, 230);
        }
        mrect(x, 154, w, 24, ed);
        mrect(x + 1, 155, w - 2, 22, bg);
        tw = text_w(&FONT_XS, van_lbl[i]);
        mtext(x + (w - tw) / 2, 161, &FONT_XS, van_lbl[i], fg);
    }
}
static void van_hum_scene(uint32_t now)
{
    uint32_t i, t = now - van_hum_t0;
    if (!van_tint[1]) {                  /* Van's colours washed toward the screen's blue, once */
        for (i = 1; i < 16u && eo_res_ok(); i++) {
            uint32_t c = EO_CAST_PAL[2][i], r = (c >> 11) & 31u, g = (c >> 5) & 63u, b = c & 31u;
            r = r * 5u / 10u;
            g = g * 6u / 10u + 4u;
            b = b * 7u / 10u + 10u;
            van_tint[i] = (uint16_t)((r << 11) | ((g > 63u ? 63u : g) << 5) | (b > 31u ? 31u : b));
        }
    }
    mrect(0, 36, 240, 144, EO_RGB(10, 18, 34));
    if (eo_res_ok())
        spr_paint(van_tint_px, EO_VAN[0], EO_CAST_W, EO_CAST_H, 24, 36, 2, 1);
    for (i = 0; i < 90u; i++) {          /* the static: speckles, new every few frames */
        uint32_t h = (i * 2246822519u) ^ ((now / 66u) * 3266489917u);
        int32_t sx = (int32_t)((h >> 3) % 240u), sy = 36 + (int32_t)((h >> 13) % 144u);
        mrect(sx, sy, 2 + (int32_t)(h & 1u), 1, ((h >> 20) & 3u) ? EO_RGB(70, 90, 120) : EO_RGB(190, 205, 230));
    }
    mrect(0, 36 + (int32_t)((t / 9u) % 150u), 240, 2, EO_RGB(40, 60, 90));       /* a slow roll bar */
}

static void van_scene(uint32_t now)
{
    uint32_t i, lvl = set.van_lvl;
    const char *c = van_cd[van_ci];
    char tb[40], nb[12];
    int32_t bob = ((now / 600u) & 1u) ? 1 : 0;
    /* the header: the game, the level, and the round pips */
    mrect(0, 0, 240, 36, C_BLACK);
    mctext(3, &FONT_XS, "NAME THAT INTERVAL", EO_RGB(110, 200, 90));
    tb[0] = 0;
    text_cat(tb, "LEVEL ");
    int_text((int32_t)lvl, nb);
    text_cat(tb, nb);
    mtext(6, 20, &FONT_XS, tb, C_GRAY);
    for (i = 0; i < VAN_ROUNDS; i++)
        mrect(240 - 6 - (int32_t)(VAN_ROUNDS - i) * 12, 22, 9, 9, i < set.van_prog ? EO_RGB(110, 200, 90) : EO_RGB(40, 50, 40));
    if (van_mode == VM_HUM) {
        van_hum_scene(now);
    } else {
        int32_t big = van_chips || van_mode == VM_CHOICE;
        van_room();
        van_paint(van_pose, big ? 56 : 48, big ? 54 : 44, big ? 4 : 3, big ? 3 : 2, now,
                  van_mode == VM_CARDS && van_typed(now) < van_len());
        if (van_pose == P_FISH) {        /* Simone, in her bowl, on the sill (her sprite from her own game) */
            mrect(176, 120, 60, 50, EO_RGB(150, 200, 230));
            mrect(178, 122, 56, 46, EO_RGB(40, 90, 150));
            if (eo_res_ok()) {
                spr_pal = EO_CAST_PAL[0];
                spr_paint(cast_px, EO_CAST[0][((now / 700u) & 1u) ? 1u : 0u], EO_CAST_W, EO_CAST_H, 186, 128 + bob * 2, 1, 2);
            }
        }
        if (van_chips || van_mode == VM_CHOICE)
            van_chip_row(now);
    }
    /* the dialogue box */
    mrect(0, 180, 240, 60, C_BLACK);
    mrect(0, 180, 240, 2, van_mode == VM_HUM ? EO_RGB(70, 90, 120) : EO_RGB(110, 200, 90));
    if (van_mode == VM_HUM)
        return;
    {
        uint32_t n = van_typed(now), k, kind = van_kind(c), li = 0, ws;
        const char *s = van_text(c);
        uint16_t col = kind == 1u ? EO_RGB(150, 190, 235) : C_WHITE;
        if (kind == 2u) {
            van_gibber(190, n, now);
        } else {
            for (k = 0; k < van_ln; k++)
                if (n >= van_ls[k])
                    li = k;
            ws = li >= 4u ? li - 3u : 0u;
            for (k = 0; k < 4u && ws + k < van_ln; k++) {
                char buf[64];
                uint32_t st = van_ls[ws + k], len = van_ll[ws + k], m = 0, j;
                if (n <= st)
                    break;
                if (n - st < len)
                    len = n - st;
                for (j = 0; j < len && m < 62u; j++)
                    buf[m++] = s[st + j] == '~' ? 'e' : s[st + j];
                buf[m] = 0;
                mtext(10, 186 + (int32_t)k * 14, &FONT_XS, buf, col);
            }
        }
        if (van_mode == VM_CARDS && n >= van_len() && ((now / 450u) & 1u)) {
            int32_t r;
            for (r = 0; r < 5; r++)
                mrect(226 + r, 228 + r, 10 - 2 * r, 1, C_AMB);
        }
    }
}

static uint32_t van_check_lines(void)    /* a test hook: the most lines any card takes (the box holds four) */
{
    uint32_t b, k, mx = 0;
    uint8_t ls[VAN_LINES], ll[VAN_LINES];
    for (b = 0; b < VAN_NBEATS; b++)
        for (k = 0; k < 4u; k++)
            if (van_beat[b].c[k]) {
                uint32_t n = van_wrap_into(van_text(van_beat[b].c[k]), ls, ll);
                mx = n > mx ? n : mx;
            }
    for (b = 0; b < VAN_NBEATS; b++)
        for (k = 0; k < 3u; k++)
            if (van_beat[b].kind == K_ASK && van_beat[b].re[k]) {
                uint32_t n = van_wrap_into(van_beat[b].re[k], ls, ll);
                mx = n > mx ? n : mx;
            }
    van_maxlines = (uint8_t)mx;
    return mx;
}
