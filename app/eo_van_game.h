/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 asilagy */
/* Name That Interval: Van's hangout. Included into app.c (one unity build) just before the select's "go".
 * Mostly chat: Van talks, a card at a time (any key goes on). Every few beats he measures two notes and you name how far apart
 * they are on the lit answer keys. Nothing is ever lost: a wrong answer is a shrug and the right key lights. */
#include "eo_interval.h"

enum { VM_CARDS, VM_ASK, VM_CHOICE, VM_HUM, VM_FISH, VM_BLINK, VM_TEN, VM_TITLE, VM_LISTEN, VM_MOMENT };   /* VM_TITLE: before the chat, where the level is picked */                        /* what the keys do now */
enum { VN_BEAT, VN_ASK, VN_CHOICE, VN_HUM, VN_FISH, VN_BLINK, VN_TEN, VN_LISTEN, VN_MOMENT };                         /* what comes when the cards run out */
#define VAN_CARDS 24u
#define VAN_LINES 10u
#define VAN_CPS 18u                                                  /* the typing speed: letters a second */
#define VAN_HUM_MS 15000u
#define VAN_FISH_MS 10000u                                            /* the long look at the fish: a quiet moment, not skippable */
#define VAN_BLINK_MS 7000u                                           /* the staring contest: he loses at the end, unskippably */
#define VAN_SNACK_MS 40000u                                          /* this long on a question: Van wonders if it is a snack break */

#define VAN_NSET 17u
static uint8_t van_cr_q, van_cr_gone, van_force_close;     /* the cricket lives on for this many more questions; the next cards are a close-up (a squint) */
static uint32_t van_lis_t0, van_cr_once, van_cr_next;   /* the fridge listening; a cricket trill due at (ms); the next stray trill */
static float van_cr_vol;
static uint8_t van_norm, van_mom, van_mom_ev;
static uint32_t van_set_used, van_mom_t0;        /* ordinary topics since the last set piece; which set pieces have had their turn this visit */
static uint8_t van_hum_seen, van_drop_force, van_drops;   /* the TV static gag this visit; name-drops (the force is a test hook) */    /* name-drops after higher/lower answers (the force is a test hook) */
static uint8_t pg_offers, van_qs;            /* 10-ISH invitations made, and questions answered this visit */
static uint8_t pg_on;                        /* 10-ISH is on the TV: the wallpaper of the rest of the chat */
static uint8_t van_tvshot;                  /* 1: the cards after the static gag stay on the static's picture (the TV shot) until the drawl is done */
static uint8_t van_cam, van_flip, van_cut;   /* the multicam: which camera is live (3 is the TV) */   /* the multicam: which camera is live */
static uint8_t van_mode, van_after, van_chips, van_cn, van_ci, van_pose;
static const char *van_cd[VAN_CARDS];
static uint32_t van_fish_t0, van_fish_bub, van_fish_next, van_blink_t0, van_blink_tk;
static uint32_t van_t0, van_typed_last, van_hum_t0, van_ask_t0, van_ev_at;
static uint8_t van_ev;                                               /* 1: play the interval again at van_ev_at */
static uint8_t van_since, van_snacked, van_first_q, van_cur, van_first_beat;         /* beats since a question; the beat now; ... */
static uint8_t van_recent[6];
static uint8_t van_missed, van_th = 0xFFu, van_tp, van_th_recent[3], van_resume, van_cmp, van_q_semi2, van_dir;   /* the thread now and where in it; a link back after a question; the compare question */
static uint8_t van_ls[VAN_LINES], van_ll[VAN_LINES], van_ln;         /* the card, wrapped */
static uint8_t van_q_semi, van_q_n, van_q_right, van_q_pick, van_last_semi;
static uint8_t van_q_semis[VAN_MAX_KEYS];
static const char *van_lbl[VAN_MAX_KEYS];                            /* the chips' names */
static char van_nbuf[3][44];
static uint8_t van_maxlines;

static uint32_t van_rnd(uint32_t n);
static uint8_t van_lastpick[6];
static uint32_t van_pick(uint32_t slot, uint32_t n)      /* a random line, never the one just used from this pool */
{
    uint32_t r = van_rnd(n);
    if (n > 1u && r == van_lastpick[slot])
        r = (r + 1u + van_rnd(n - 1u)) % n;
    van_lastpick[slot] = (uint8_t)r;
    return r;
}
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
static const char *van_text(const char *c) { return (c[0] == '*' || c[0] == '#' || c[0] == '@') ? c + 1 : c; }
static uint32_t van_kind(const char *c) { return c[0] == '*' ? 1u : c[0] == '#' ? 2u : c[0] == '@' ? 3u : 0u; }   /* 1 stage direction (slanted), 2 his own symbols, 3 a drawl: big and slow */
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
            if (best && text_w(&FONT_M, buf) > 224)
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
    if (van_tvshot && van_ci >= 2u) {    /* the drawl is done: the hiss fades and the room comes back */
        van_tvshot = 0;
        eo_noise_on = 0;
        eo_noise_dark = 0;
        eo_tv_hum = 0;
    }
    van_cut++;                           /* every card is a cut: wide on the first, then medium and close, flipping sides */
    {
        uint32_t cam, last = (van_cn > 1u && (uint32_t)van_ci + 1u == van_cn);
        if (van_pose == P_FISH)
            cam = 0u;
        else if (pg_on)                  /* with the game on: the talk stays on the TV, the game running under it */
            cam = (van_cut % 5u == 0u) ? ((van_cut & 1u) ? 1u : 2u) : 3u;   /* mostly the TV, the talk carries on over the game; a cutaway on Van only now and then */
        else if (van_ci == 0u)
            cam = 0u;
        else if (last)
            cam = 2u;                    /* the last line of a bit gets the close-up */
        else
            cam = (van_cut & 1u) ? 1u : 2u;
        if (van_force_close) {           /* a squint: the close-up */
            cam = 2u;
            van_force_close--;
        }
        van_cam = (uint8_t)cam;
        van_flip = (uint8_t)((van_cut >> 1) & 1u);
    }
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
    uint32_t n = (now - van_t0) * (van_ci < van_cn && van_kind(van_cd[van_ci]) == 3u ? 4u : VAN_CPS) / 1000u, l = van_len();
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
static void van_stop_sound(void);
static void van_play_pair(uint32_t root, uint32_t semi)    /* two notes, root then the other: warm, never lit */
{
    van_stop_sound();
    mv_sfx(0, 80, 640, root, V_WARM);
    mv_sfx(1, 860, 760, root + semi, V_WARM);
    seq_n = 2;
    seq_end = 1700;
    eo_seq_start(0, 0);
}
static void van_play_q(void)             /* the question's notes: one pair, or two (a gap, then another gap) */
{
    uint32_t r = set.root;
    if (!van_cmp) {
        van_play_pair(r, van_q_semi);
        return;
    }
    van_stop_sound();
    mv_sfx(0, 80, 520, r, V_WARM);
    mv_sfx(1, 700, 640, r + van_q_semi, V_WARM);
    mv_sfx(2, 2000, 520, r, V_WARM);
    mv_sfx(3, 2620, 640, r + van_q_semi2, V_WARM);
    seq_n = 4;
    seq_end = 3400;
    eo_seq_start(0, 0);
}
static void van_blip(uint32_t n)         /* an alien chirp for a letter: an FM warble that glides up or down */
{
    static const uint8_t SC[5] = {0, 2, 5, 7, 10};
    static const float GL[5] = {0.55f, -0.35f, 0.3f, -0.5f, 0.75f};
    eo_blub_soft = 1;
    eo_blub_glide = GL[(n + van_ci) % 5u];
    eo_blub_note = 62 + SC[(n * 3u + van_ci * 2u) % 5u] + ((n % 7u) == 0u ? 7 : 0);
    eo_blub_trig++;
}
static void van_stop_sound(void)
{
    van_tvshot = 0;
    eo_noise_dark = 0;
    eo_noise_on = 0;
    eo_tv_hum = 0;
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
    if (van_chips == 3u && van_q_pick < van_q_n)
        *mid |= 1u << van_answer_key[van_q_pick];
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
    van_cmp = lv->cmp;
    van_missed = 0;
    if (van_cmp) {                       /* no names yet: two gaps, which is bigger (or smaller)? */
        uint32_t i1 = 0, i2 = 0, tr;
        for (tr = 0; tr < 40u; tr++) {
            int32_t d;
            i1 = lv->pool[van_rnd(pn)];
            i2 = lv->pool[van_rnd(pn)];
            d = (int32_t)i1 - (int32_t)i2;
            if ((d < 0 ? -d : d) >= (int32_t)lv->mind && (i1 != van_q_semi || i2 != van_q_semi2))
                break;
        }
        van_dir = (uint8_t)van_rnd(2u);
        van_q_semi = (uint8_t)i1;
        van_q_semi2 = (uint8_t)i2;
        van_q_n = 2;
        van_lbl[0] = "FIRST";
        van_lbl[1] = "SECOND";
        van_q_right = (uint8_t)(van_dir == 0u ? (i1 > i2 ? 0u : 1u) : (i1 < i2 ? 0u : 1u));
        van_q_pick = 0xFFu;
        van_snacked = 0;
        van_nbuf[2][0] = 0;
        text_cat(van_nbuf[2], van_dir ? "Which gap is smaller?" : "Which gap is bigger?");
        van_say(P_TAPE, VN_ASK, van_first_q ? van_radio_down : van_lead[1u + van_pick(0u, 7u)], van_first_q ? van_lead[0] : van_nbuf[2], van_first_q ? van_lead[1] : 0, van_first_q ? van_nbuf[2] : 0);
        van_first_q = 0;
        (void)root;
        return;
    }
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
    van_say(P_TAPE, VN_ASK, van_first_q ? van_radio_down : van_lead[1u + van_pick(0u, 7u)], van_first_q ? van_lead[0] : 0, van_first_q ? van_lead[3] : 0, 0);
    van_first_q = 0;
}

static void van_ask_now(void)            /* the cards are done: the question stands, the keys are lit, the notes sound */
{
    van_mode = VM_ASK;
    van_chips = 1;
    van_ask_t0 = plat_ms();
    van_play_q();
    ui.redraw = 1;
}

/* ---- the beats */
static void van_hum_start(void)
{
    van_mode = VM_HUM;
    van_hum_seen = 1;
    van_chips = 0;
    van_hum_t0 = plat_ms();
    set.van_seen |= 2u;
    set_dirty = 1;
    eo_drone_set(KEY_NOTE0 + (int32_t)ROOT_MIN - 12, 0.12f);
    eo_noise_vol = 0.062f;
    eo_noise_dark = 1;                   /* mostly muffled static (80%) with a little bright hiss, and a tube TV's hum under it */
    eo_tv_hum = 1;                       /* a tube TV: 60 Hz hum, 120 Hz harmonic, 15.7 kHz whine, under the deep rumble */
    eo_noise_on = 1;
    ui.redraw = 1;
}
static void van_fish_start(void)
{
    van_mode = VM_FISH;
    van_chips = 0;
    van_fish_t0 = plat_ms();
    van_fish_bub = 0;
    van_fish_next = 400;                 /* the first bubble sound; no hiss under it, just bubbles */
    eo_noise_on = 0;
    ui.redraw = 1;
}
static void van_fish_stop(void)
{
    eo_noise_on = 0;
    van_say(P_TALK, VN_BEAT, "That's Simon~. She came with the hat. I guess.", "I swear sometimes it feels like she's talking to me.", "Maybe I was dreaming. I could've sworn she said something about visions of pizza... yeah.", 0);
}
static void van_fridge_start(void)       /* he stops and listens: the fridge really hums, for a few seconds, and nothing else happens */
{
    van_mode = VM_LISTEN;
    van_chips = 0;
    van_lis_t0 = plat_ms();
    eo_drone_set(KEY_NOTE0 + (int32_t)ROOT_MIN - 12 + 2, 0.10f);
    eo_noise_vol = 0.02f;
    eo_noise_on = 1;
    ui.redraw = 1;
}
static void van_fridge_stop(void)
{
    eo_noise_on = 0;
    eo_drone_set(-1, 0.0f);
    van_say(P_IDLE, VN_BEAT, van_fridge_after[0], van_fridge_after[1], van_fridge_after[2], 0);
}
static void van_cricket(float vol)       /* one trill, this faint */
{
    eo_cricket_vol = vol;
    eo_cricket_trig++;
}
static void van_room(int32_t num, int32_t den, int32_t px, int32_t py);
static void van_paint(uint32_t pose, int32_t ox, int32_t oy, int32_t num, int32_t den, uint32_t now, uint32_t talking);
/* ---- little moments: a few seconds of sound and picture between two topics, with a line before and a line after */
static void van_mblip(int32_t note, uint32_t soft, float glide)
{
    eo_blub_soft = (uint8_t)soft;
    eo_blub_glide = glide;
    eo_blub_note = note;
    eo_blub_trig++;
}
static void van_noise(float vol, uint32_t on)
{
    eo_noise_vol = vol;
    eo_noise_on = (uint8_t)on;
}
static void van_mom_start(uint32_t m)
{
    van_mode = VM_MOMENT;
    van_mom = (uint8_t)m;
    van_mom_ev = 0;
    van_mom_t0 = plat_ms();
    van_chips = 0;
    ui.redraw = 1;
}
static void van_mom_end(void)
{
    const char *a = van_mom_after[van_mom][0], *b = van_mom_after[van_mom][1];
    eo_noise_on = 0;
    eo_rd_bend = 1.0f;
    van_say(P_IDLE, VN_BEAT, a, b, 0, 0);
}
static float van_mom_radio(uint32_t now)   /* the radio's level in each moment */
{
    uint32_t t = now - van_mom_t0;
    switch (van_mom) {
    case 0: return 0.35f;
    case 1: return (t < 1200u || t > 5200u) ? 1.0f : 0.0f;
    case 2: return 0.5f;
    case 3: return 1.0f;
    case 4: return 0.25f;
    case 5: return 0.0f;
    case 6: return 0.2f;
    case 7: return 0.4f;
    case 8: return 0.0f;
    default: return 0.4f;
    }
}
static void van_mom_step(uint32_t now)   /* the sounds of each moment, by time */
{
    uint32_t t = now - van_mom_t0, m = van_mom, n;
    if (t >= van_mom_ms[m]) {
        van_mom_end();
        return;
    }
    switch (m) {
    case 0:                              /* eight steps upstairs, a pause, eight again */
        n = t / 380u;
        if ((n < 8u || (n >= 11u && n < 19u)) && van_mom_ev <= n) {
            van_mom_ev = (uint8_t)(n + 1u);
            van_mblip(26 + (int32_t)(n & 1u) * 2, 0, -0.5f);
        }
        break;
    case 1:                              /* the dial: static, a talk show, a polka, and back to the bossa */
        n = t / 150u;
        if (t >= 1200u && t < 5200u) {
            van_noise(t < 2400u ? 0.06f : 0.012f, 1);
            if (van_mom_ev <= n) {
                van_mom_ev = (uint8_t)(n + 1u);
                if (t < 2400u)
                    van_mblip(60 + (int32_t)(n % 8u) * 5, 1, 0.8f);
                else if (t < 3800u)
                    van_mblip((n & 1u) ? 68 : 73, 1, 0.1f);
                else
                    van_mblip((n & 1u) ? 64 : 40, (n & 1u) ? 1u : 0u, 0.0f);
            }
        } else {
            van_noise(0.0f, 0);
        }
        break;
    case 2:                              /* a slow snore */
        n = t / 1400u;
        if (van_mom_ev <= n && t > 600u) {
            van_mom_ev = (uint8_t)(n + 1u);
            van_mblip(38, 1, -0.4f);
        }
        break;
    case 3:                              /* the light buzzes and flickers; the radio sags and recovers */
        if (t < 2400u)
            van_noise(((t / 110u) & 1u) ? 0.0f : 0.03f, 1);
        else
            van_noise(0.0f, 0);
        n = t < 1500u ? 0u : t < 2200u ? (t - 1500u) : t < 3000u ? (3000u - t) * 7u / 8u : 0u;
        eo_rd_bend = 1.0f - 0.2f * (float)(n > 700u ? 700u : n) / 700.0f;
        break;
    case 4:                              /* rain on the window, and a drip now and then */
        van_noise(0.03f, 1);
        n = t / 700u;
        if (van_mom_ev <= n) {
            van_mom_ev = (uint8_t)(n + 1u);
            van_mblip(86 + (int32_t)((n * 7u) % 5u), 1, 0.0f);
        }
        break;
    case 5:                              /* channels: a burst of static and a beep for each */
        n = t / 1000u;
        if (van_mom_ev <= n) {
            van_mom_ev = (uint8_t)(n + 1u);
            van_mblip(55 + (int32_t)(n % 5u) * 6, 1, 0.3f);
        }
        van_noise(((t % 1000u) < 180u) ? 0.07f : 0.0f, 1);
        break;
    case 6:                              /* two knocks */
        if ((van_mom_ev == 0u && t >= 700u) || (van_mom_ev == 1u && t >= 1100u)) {
            van_mom_ev++;
            van_mblip(33, 0, -0.3f);
        }
        break;
    case 7:                              /* a loud bag, then crunching */
        n = t / 90u;
        if (t < 2200u) {
            van_noise((n & 1u) ? 0.0f : ((n * 7u) % 3u ? 0.05f : 0.09f), 1);
        } else {
            van_noise((((t - 2200u) % 600u) < 120u) ? 0.1f : 0.0f, 1);
        }
        break;
    case 8:                              /* nothing at all */
        break;
    default:                             /* the tape: a zip out, a snap back */
        if (van_mom_ev == 0u && t > 300u) {
            van_mom_ev = 1;
            van_mblip(44, 1, 1.2f);
        } else if (van_mom_ev == 1u && t > 1800u) {
            van_mom_ev = 2;
            van_noise(0.08f, 1);
            van_mblip(76, 0, -1.0f);
        } else if (van_mom_ev == 2u && t > 2000u) {
            van_noise(0.0f, 0);
        }
        break;
    }
}
static void van_dither(int32_t lvl)      /* darkens the picture with black rows (0 none .. 4 all) */
{
    int32_t y;
    for (y = 0; y < 144; y++)
        if ((y & 3) < lvl)
            mrect(0, y, 240, 1, C_BLACK);
}
static void van_mom_scene(uint32_t now)
{
    uint32_t t = now - van_mom_t0, m = van_mom;
    int32_t shake = 0, i;
    if (m == 4u) {                       /* the window, close: the room punched in, and rain */
        van_room(2, 1, 60, 0);
        for (i = 0; i < 70; i++) {
            uint32_t h = (uint32_t)(i + 1) * 2654435761u;
            int32_t x = (int32_t)((h >> 8) % 240u), y = (int32_t)((h >> 18) % 160u + t / 5u * (2u + (h >> 28) % 3u) / 2u) % 150 - 6;
            mrect(x, y, 1, 9, EO_RGB(190, 220, 250));
        }
        return;
    }
    if (m == 5u) {                       /* the TV, changing channels */
        uint32_t ch = t / 1000u;
        static const uint16_t BAR[5][4] = {{0xF800, 0x07E0, 0x001F, 0xFFE0}, {0x07FF, 0xF81F, 0xFFFF, 0x4208}, {0x2104, 0xA514, 0x2104, 0xA514}, {0xFD20, 0x0400, 0x780F, 0x07E0}, {0x8410, 0x8410, 0x2104, 0xFFFF}};
        mrect(0, 0, 240, 144, EO_RGB(22, 26, 38));
        mrect(18, 8, 204, 128, EO_RGB(64, 58, 74));
        for (i = 0; i < 4; i++)
            mrect(32 + i * 44, 16, 44, 96, BAR[ch % 5u][i]);
        if ((t % 1000u) < 180u)          /* the change: static */
            for (i = 0; i < 60; i++) {
                uint32_t h = ((uint32_t)i * 2654435761u ^ (t / 40u) * 40503u) * 2246822519u;
                mrect(32 + (int32_t)((h >> 8) % 176u), 16 + (int32_t)((h >> 20) % 96u), 3, 2, EO_RGB(220, 220, 230));
            }
        return;
    }
    if (m == 0u && ((t / 380u) < 8u || ((t / 380u) >= 11u && (t / 380u) < 19u)) && (t % 380u) < 70u)
        shake = 1;                       /* each step shakes the room a pixel */
    if (m == 6u) {                       /* close on Van, not moving */
        van_room(2, 1, 40, 60);
        van_paint(P_IDLE, -24, 6, 3, 1, now, 0);
        return;
    }
    van_room(1, 1, 0, shake);
    van_paint(m == 2u ? P_ASLEEP : m == 7u ? P_SNACK : m == 9u ? P_TAPE : m == 1u ? P_LOUNGE : P_IDLE, 48, 4 + shake, 3, 2, now, 0);
    if (m == 2u) {                       /* the lights go out as he nods off, and come back as he wakes */
        uint32_t u = t < 2000u ? t : t > 4500u ? (van_mom_ms[2] > t ? van_mom_ms[2] - t : 0u) * 2u : 2000u;
        van_dither((int32_t)(u / 500u > 3u ? 3u : u / 500u));
    }
    if (m == 3u && t < 2400u && ((t / 110u) & 1u))   /* the flicker */
        van_dither(3);
    if (m == 8u)                         /* quiet: just a thin letterbox, the room very still */
        van_dither(0);
    if (m == 9u && t > 300u && t < 2100u) {          /* the tape measure runs out across the screen, then snaps back */
        int32_t w = t < 1800u ? (int32_t)((t - 300u) * 220u / 1500u) : 0;
        mrect(10, 110, w, 4, EO_RGB(240, 200, 60));
        mrect(10, 112, w, 1, EO_RGB(60, 50, 20));
    }
}

static void van_blink_start(void)
{
    van_mode = VM_BLINK;
    van_chips = 0;
    van_blink_t0 = plat_ms();
    van_blink_tk = 0;
    ui.redraw = 1;
}
static void van_blink_stop(void)
{
    van_say(P_TALK, VN_BEAT, van_blink_after[0], van_blink_after[1], van_blink_after[2], 0);
}
/* ---- 10-ISH: Pong on the TV. Van is the left paddle and is okay at it, you are the right one on knob 4. No stakes, no win. */
#define PG_W 176                                                     /* the field, in pixels (positions are in 1/16 px) */
#define PG_H 96
#define PG_PH 24                                                     /* paddle height */
static int32_t pg_bx, pg_by, pg_vx, pg_vy, pg_pp, pg_vp, pg_aim;
static uint32_t pg_cut, pg_ms, pg_run, pg_turn_ms, pg_sp, pg_sv, pg_hint, pg_turned;
static void pg_serve(int32_t dir)
{
    pg_bx = PG_W * 8;
    pg_by = PG_H * 8;
    pg_vx = dir * 1000;
    pg_vy = ((int32_t)van_rnd(2) ? 1 : -1) * (160 + (int32_t)van_rnd(260));
    pg_aim = ((int32_t)van_rnd(21) - 10) * 16;
}
static void van_next_beat(void);
static void van_ten_start(void)          /* the TV goes on and stays on; the chat goes on around it */
{
    pg_on = 1;
    pg_sp = pg_sv = pg_hint = pg_turned = pg_run = pg_cut = 0;
    pg_pp = pg_vp = PG_H * 8;
    pg_ms = pg_turn_ms = plat_ms();
    pg_serve(-1);
    van_next_beat();
}
static void van_pong_turn(int32_t d)     /* knob 4: the right paddle; nothing else moves it */
{
    if (!pg_on)
        return;
    pg_pp = clamp(pg_pp + d * 72, PG_PH * 8, (PG_H - PG_PH / 2) * 16);
    pg_turned = 1;
    pg_turn_ms = plat_ms();
}
static void pg_blip(int32_t note)
{
    eo_blub_soft = 1;
    eo_blub_glide = 0.3f;
    eo_blub_note = note;
    eo_blub_trig++;
}
static uint32_t van_on_tv(uint32_t now);   /* is the shot the TV now (the game running), or a cutaway on Van (it waits)? */
static void van_pong_step(uint32_t now)
{
    uint32_t dt = now - pg_ms;
    int32_t half = PG_PH * 8, tgt, d;
    pg_ms = now;
    if (!van_on_tv(now)) {               /* a cutaway: the game waits, frozen, until the cut back */
        pg_cut += dt > 50u ? 50u : dt;
        return;
    }
    if (dt > 50u)
        dt = 50u;
    pg_run += dt;
    pg_bx += pg_vx * (int32_t)dt / 1000;
    pg_by += pg_vy * (int32_t)dt / 1000;
    if (pg_by < 48) {
        pg_by = 48;
        pg_vy = -pg_vy;
    } else if (pg_by > PG_H * 16 - 48) {
        pg_by = PG_H * 16 - 48;
        pg_vy = -pg_vy;
    }
    tgt = (pg_vx < 0 ? pg_by + pg_aim : PG_H * 8);          /* Van watches the ball coming, and drifts back to the middle otherwise */
    d = clamp(tgt - pg_vp, -(int32_t)dt * 800 / 1000, (int32_t)dt * 800 / 1000);
    pg_vp = clamp(pg_vp + d, half, PG_H * 16 - half);
    if (pg_vx < 0 && pg_bx <= 14 * 16) {                     /* the left paddle */
        if (pg_by >= pg_vp - half - 40 && pg_by <= pg_vp + half + 40) {
            pg_vx = -pg_vx;
            pg_vy += (pg_by - pg_vp) * 4;
            pg_vy = clamp(pg_vy, -520, 520);
            pg_aim = ((int32_t)van_rnd(31) - 15) * 16;       /* "okay": sometimes he is off */
            pg_blip(55);
        } else if (pg_bx <= 0) {
            pg_sp++;
            pg_blip(43);
            pg_serve(1);
        }
    } else if (pg_vx > 0 && pg_bx >= (PG_W - 14) * 16) {      /* the right paddle: yours */
        if (pg_by >= pg_pp - half - 40 && pg_by <= pg_pp + half + 40) {
            pg_vx = -pg_vx;
            pg_vy += (pg_by - pg_pp) * 4;
            pg_vy = clamp(pg_vy, -520, 520);
            pg_blip(62);
        } else if (pg_bx >= PG_W * 16) {
            pg_sv++;
            pg_blip(43);
            pg_serve(-1);
        }
    }
    ui.redraw = 1;
}
static void van_hum_stop(void)
{
    eo_noise_on = 0;
    eo_noise_dark = 0;
    eo_tv_hum = 0;
    eo_drone_set(-1, 0.0f);
}

static const char *van_pick_link(const char *const *pool, uint32_t n) { return pool[van_rnd(n)]; }
static void van_next_beat(void)
{
    const van_beat_t *b;
    const char *cards[6] = {0, 0, 0, 0, 0, 0};
    uint32_t i, tries, nc = 0, fresh = 0;
    if (set_dirty && !seq_on)
        (void)store_now();
    if (pg_on && !pg_turned && pg_hint < 3u && pg_run > 6000u + pg_hint * 8000u) {   /* he tells you how, after a while, between two beats */
        van_say(P_LOUNGE, VN_BEAT, van_pong_hint[pg_hint++], 0, 0, 0);
        return;
    }
    if (!pg_on && pg_offers < 3u && van_qs >= 2u + 2u * pg_offers) {   /* two questions in: "wanna play 10-ISH?" (asked again after two more if he is turned down) */
        if (!van_hum_seen) {             /* the TV acts up first: ten seconds of static, and then he has it working */
            uint32_t h;
            for (h = 0; h < VAN_NBEATS && van_beat[h].kind != K_HUM; h++) {
            }
            if (h < VAN_NBEATS) {
                van_cur = (uint8_t)h;
                van_say(van_beat[h].pose, VN_HUM, van_beat[h].c[0], van_beat[h].c[1], van_beat[h].c[2], 0);
                return;
            }
        }
        pg_offers++;
        van_cur = 85;
        van_say(P_LOUNGE, VN_CHOICE, van_beat[85].c[0], van_beat[85].c[1], van_beat[85].c[2], 0);
        return;
    }
    if (van_since >= VAN_BEATS) {
        van_since = 0;
        van_question();
        return;
    }
    if (van_th == 0xFFu || van_thread[van_th][van_tp > 5u ? 5u : van_tp] == 255u || van_tp >= 6u) {   /* a thread is done: another one, not one of the last three */
        {   /* pacing: after two ordinary topics comes a set piece (the fish, Meow, the TV, the song, the staring contest, Mom), so the chat keeps getting broken up */
            static const uint8_t SET_FIRST[VAN_NSET] = {12, 2, 37, 81, 84, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97};   /* the first beat of each set piece's thread: the fish, then Meow, first */
            uint32_t want = 0xFFu, k, th, pick = 0xFFu;
            if (van_norm >= 2u) {
                for (k = 0; k < VAN_NSET && want == 0xFFu; k++) {
                    if (van_set_used & (1u << k))
                        continue;
                    if (SET_FIRST[k] == 37u && (set.van_seen & 2u))
                        continue;
                    want = k;
                }
                if (want == 0xFFu) {     /* all of them have had a turn this visit: start the round again */
                    van_set_used = 0;
                    want = 0;
                }
            }
            if (want != 0xFFu) {
                if (want >= 2u) {        /* after the fish and Meow, the rest in a random order */
                    uint32_t n = 0, c[VAN_NSET];
                    for (k = 2; k < VAN_NSET; k++)
                        if (!(van_set_used & (1u << k)) && !(SET_FIRST[k] == 37u && (set.van_seen & 2u)))
                            c[n++] = k;
                    if (n)
                        want = c[van_rnd(n)];
                }
                for (th = 0; th < VAN_THREADS; th++)
                    if (van_thread[th][0] == SET_FIRST[want])
                        pick = th;
            }
            if (pick != 0xFFu) {
                van_th = (uint8_t)pick;
                van_set_used |= 1u << want;
                van_norm = 0;
            } else {
                for (tries = 0; tries < 20u; tries++) {
                    uint32_t ok = 1;
                    th = van_rnd(VAN_THREADS);
                    if (van_thread[th][0] == 37u && (set.van_seen & 2u))
                        ok = 0;
                    for (k = 0; k < VAN_NSET; k++)     /* the set pieces come on the schedule, not by chance */
                        if (van_thread[th][0] == SET_FIRST[k])
                            ok = 0;
                    for (i = 0; i < 3u; i++)
                        if (van_th_recent[i] == th)
                            ok = 0;
                    if (ok) {
                        van_th = (uint8_t)th;
                        break;
                    }
                }
                if (van_norm < 255u)
                    van_norm++;
            }
        }
        for (i = 2; i > 0; i--)
            van_th_recent[i] = van_th_recent[i - 1u];
        van_th_recent[0] = van_th;
        van_tp = 0;
        fresh = 1;
    }
    van_cur = van_thread[van_th][van_tp];
    van_since++;
    b = &van_beat[van_cur];
    if (van_tp == 0u && van_first_beat) {                 /* the very first beat of a visit has nobody to link from */
        van_first_beat = 0;
    } else if (van_resume) {
        cards[nc++] = van_pick_link(van_link_back, 4u);   /* after a question: back to the thread */
    } else if (fresh) {
        cards[nc++] = van_pick_link(van_link_new, 8u);
    } else {
        cards[nc++] = van_pick_link(van_link_in, 6u);
    }
    van_first_beat = 0;
    van_resume = 0;
    van_tp++;
    for (i = 0; i < 4u && b->c[i] && nc < 6u; i++)
        cards[nc++] = b->c[i];
    if (b->kind == K_MEOW) {             /* a meow from the next room, then Van explains */
        van_stop_sound();
        mv_sfx(0, 0, 520, 14, V_CATFX);
        seq_n = 1;
        seq_end = 600;
        eo_seq_start(0, 0);
    }
    if (b->kind == K_MOM)
        van_mom = b->arg;
    if (van_cur == 87u)                  /* the dots, and then, very faintly, something */
        van_cr_once = plat_ms() + 2600u, van_cr_vol = 0.006f;
    if (b->kind == K_PHONE) {            /* a ring: one high alien chirp */
        eo_blub_soft = 1;
        eo_blub_glide = 0.2f;
        eo_blub_note = 88;
        eo_blub_trig++;
    }
    van_say(b->pose, b->kind == K_ASK ? VN_CHOICE : b->kind == K_HUM ? VN_HUM : b->kind == K_FISH ? VN_FISH : b->kind == K_BLINK ? VN_BLINK : (b->kind == K_TEN && !pg_on) ? VN_TEN : b->kind == K_FRIDGE ? VN_LISTEN : b->kind == K_MOM ? VN_MOMENT : VN_BEAT, cards[0], cards[1], nc > 2u ? cards[2] : 0, nc > 3u ? cards[3] : 0);
    for (i = 4; i < nc; i++)
        van_cd[van_cn++] = cards[i];
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
    else if (a == VN_FISH)
        van_fish_start();
    else if (a == VN_BLINK)
        van_blink_start();
    else if (a == VN_TEN)
        van_ten_start();
    else if (a == VN_LISTEN)
        van_fridge_start();
    else if (a == VN_MOMENT)
        van_mom_start(van_mom);
    else
        van_next_beat();
}

static void van_answer(uint32_t ix)
{
    uint32_t i, right = ix == van_q_right, rt = set.root, count = right && !van_missed;   /* only a first try counts toward the level */
    char *p;
    van_q_pick = (uint8_t)ix;
    van_stop_sound();               /* the notes in the air stop too, not just the sequencer */
    if (!right) {                        /* a wrong answer: nothing lost, nothing shown; Van says something and you try again */
        van_missed = 1;
        eo_blub_soft = 1;                /* a low, sliding alien "hm": his own voice, no buzzer */
        eo_blub_glide = -0.7f;
        eo_blub_note = 57;
        eo_blub_trig++;
        van_say(P_COOKED, VN_ASK, van_wrong[van_pick(1u, 10u)], van_retry[van_pick(2u, 8u)], 0, 0);
        van_chips = 3;
        return;
    }
    if (van_qs < 255u)
        van_qs++;
    if (van_cr_q && --van_cr_q == 0u)
        van_cr_gone = 1;
    eo_blub_soft = 1;                    /* a happy upward alien chirp: his own voice, no bell */
    eo_blub_glide = 0.9f;
    eo_blub_note = 69;
    eo_blub_trig++;
    p = van_nbuf[0];
    p[0] = 0;
    if (van_cmp) {
        text_cat(p, van_lbl[van_q_right]);
        text_cat(p, van_dir ? " was the smaller one." : " was the bigger one.");
    } else {
        text_cat(p, van_iv[van_q_semi - 1u].name);
        text_cat(p, ".");
    }
    if (count) {
        if (set.van_prog < VAN_ROUNDS)
            set.van_prog++;
        set_dirty = 1;
    }
    if (count && set.van_prog >= VAN_ROUNDS && set.van_lvl < VAN_LEVELS) {    /* the level is done */
        const van_level_t *ol = van_level(set.van_lvl), *nl = van_level(set.van_lvl + 1u);
        if (set.van_top < set.van_lvl)
            set.van_top = set.van_lvl;
        set.van_lvl++;
        set.van_prog = 0;
        set_dirty = 1;
        van_nbuf[1][0] = 0;
        if (nl->cmp) {
            text_cat(van_nbuf[1], "*The gaps are closer together now.");
        } else {
            text_cat(van_nbuf[1], ol->cmp ? "*They have names now: " : "*New on the tape: ");
        }
        for (i = nl->cmp ? nl->pn : ol->cmp ? 0u : ol->pn; i < nl->pn; i++) {
            text_cat(van_nbuf[1], van_iv[nl->pool[i] - 1u].name);
            text_cat(van_nbuf[1], i + 1u < nl->pn ? ", " : ".");
        }
        van_say(P_LAUGH, VN_BEAT, van_nbuf[0], van_right[van_pick(3u, 12u)], van_up[van_rnd(2u)], van_nbuf[1]);
    } else if (count && set.van_prog >= VAN_ROUNDS) {
        if (set.van_top < set.van_lvl) {
            set.van_top = set.van_lvl;
            set_dirty = 1;
        }
        van_say(P_LAUGH, VN_BEAT, van_nbuf[0], van_right[van_pick(3u, 12u)], van_cmp ? 0 : van_iv[van_q_semi - 1u].joke, 0);
    } else if (!count) {                 /* got there on a retry: it does not count toward the level, and Van says so, kindly */
        van_say(P_TALK, VN_BEAT, van_nbuf[0], van_retry_ok[van_pick(4u, 4u)], 0, 0);
    } else {
        van_say(P_LAUGH, VN_BEAT, van_nbuf[0], van_right[van_pick(3u, 12u)], van_cmp ? 0 : van_iv[van_q_semi - 1u].joke, 0);
    }
    if (van_cmp && van_cn <= 3u && (van_drop_force || van_rnd(3u) == 0u)) {   /* now and then (not every time): the real name of the gap you picked, cheekily */
        static char dn[112];
        uint32_t semi = van_q_right ? van_q_semi2 : van_q_semi, t = van_pick(5u, 6u);
        const char *nm = van_name_lc[semi - 1u];
        dn[0] = 0;
        text_cat(dn, van_drop_pre[t]);
        if (t != 4u)
            text_cat(dn, nm[0] == 'o' ? "an " : "a ");
        text_cat(dn, nm);
        text_cat(dn, van_drop_post[t]);
        van_cd[van_cn++] = dn;
        if (van_drop_force || van_rnd(2u) == 0u)
            van_cd[van_cn++] = van_vibe[semi - 1u];
        van_drops++;
        van_drop_force = 0;
    }
    if (van_cr_gone) {                   /* the cricket's last question: he notices it has gone */
        van_cr_gone = 0;
        if (van_cn < 6u)
            van_cd[van_cn++] = van_cricket_bye;
    }
    van_chips = 2;
    van_resume = van_th != 0xFFu && van_tp > 0u && van_thread[van_th][van_tp < 6u ? van_tp : 5u] != 255u && van_tp < 6u;
}

static void van_choose(uint32_t ix)      /* an ask beat: your pick, Van's reply */
{
    const van_beat_t *b = &van_beat[van_cur];
    if (van_cur == 85u && ix == 0u) {   /* yes: two lines, and the TV goes on */
        static char rb[2][80];
        const char *r = b->re[0], *bar = r;
        uint32_t k = 0;
        while (*bar && *bar != '|')
            bar++;
        for (; r + k < bar && k < 78u; k++)
            rb[0][k] = r[k];
        rb[0][k] = 0;
        for (k = 0; bar[1u + k] && k < 78u; k++)
            rb[1][k] = bar[1u + k];
        rb[1][k] = 0;
        van_say(P_LOUNGE, VN_TEN, rb[0], rb[1], 0, 0);
        return;
    }
    if (van_cur == 87u) {               /* the cricket: yes, he squints ("are you sure?"); no, it gets a little louder. Then it lives on for a couple of questions */
        static char rb[2][80];
        const char *r = b->re[ix], *bar = r;
        uint32_t k = 0;
        while (*bar && *bar != '|')
            bar++;
        for (; r + k < bar && k < 78u; k++)
            rb[0][k] = r[k];
        rb[0][k] = 0;
        for (k = 0; bar[1u + k] && k < 78u; k++)
            rb[1][k] = bar[1u + k];
        rb[1][k] = 0;
        van_say(P_COOKED, VN_BEAT, rb[0], rb[1], 0, 0);
        van_force_close = 2;
        van_cam = 2;
        van_cr_once = plat_ms() + 3000u;
        van_cr_vol = ix == 0u ? 0.01f : 0.025f;
        van_cr_q = 3;
        van_cr_next = plat_ms() + 15000u;
        return;
    }
    if (van_cur == 81u) {               /* the song: any answer, then the whole song, then "what do you think?" */
        static char rb[112];
        const char *r = b->re[ix];
        const char *cards[VAN_CARDS];
        uint32_t k = 0, n = 0, i;
        for (; r[k] && r[k] != '@' && k < 110u; k++)
            rb[k] = r[k];
        rb[k] = 0;
        cards[n++] = rb;
        for (i = 0; i < VAN_NSONG && n < VAN_CARDS - 1u; i++)
            cards[n++] = van_song[i];
        cards[n++] = van_beat[82].c[0];
        van_cur = 82;                    /* the ask that follows */
        van_say(P_TALK, VN_CHOICE, cards[0], cards[1], cards[2], cards[3]);
        for (i = 4; i < n; i++)
            van_cd[van_cn++] = cards[i];
        return;
    }
    {
        static char rb[2][112];
        const char *r = b->re[ix], *bar = r;
        uint32_t k = 0;
        while (*bar && *bar != '|')
            bar++;
        if (*bar == '|') {              /* two cards: before the bar, then after it */
            for (; r + k < bar && k < 110u; k++)
                rb[0][k] = r[k];
            rb[0][k] = 0;
            for (k = 0; bar[1u + k] && k < 110u; k++)
                rb[1][k] = bar[1u + k];
            rb[1][k] = 0;
            van_say(P_TALK, VN_BEAT, rb[0], rb[1], 0, 0);
        } else {
            van_say(P_TALK, VN_BEAT, r, 0, 0, 0);
        }
    }
    van_stop_sound();
    mv_sfx(0, 0, 180, 12u + ix * 2u, V_TRI);
    seq_n = 1;
    seq_end = 260;
    eo_seq_start(0, 0);
}

static void van_enter(void)
{
    van_stop_sound();
    van_since = 0;
    van_th = 0xFFu;
    van_tp = 0;
    van_resume = 0;
    van_first_beat = 1;
    van_first_q = 1;
    {
        uint32_t k;
        for (k = 0; k < 3u; k++)
            van_th_recent[k] = 0xFFu;
    }
    van_snacked = 0;
    pg_on = 0;
    pg_offers = van_qs = van_hum_seen = 0;
    van_cr_q = 0;
    van_cr_once = 0;
    van_norm = van_set_used = 0;
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
    eo_voice_trim = 0.3f;                /* the interval notes sit under his voice (about 10 dB down) */
    van_mode = VM_TITLE;                 /* the title: Van, a line about the game, and the level (K1). PLAY starts the chat */
    van_cn = 0;
    ui.redraw = 1;
}
static void van_begin(void)              /* PLAY on the title: the chat starts, and the level is never shown again */
{
    uint32_t seen = set.van_seen & 1u;
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
    van_tvshot = 0;
    eo_noise_dark = 0;
    eo_blub_soft = 0;
    eo_noise_on = 0;
    eo_tv_hum = 0;
    eo_radio_on = 0;
    eo_voice_trim = 1.0f;
    van_cr_q = 0;
    van_cr_once = 0;
    eo_rd_bend = 1.0f;
    van_force_close = 0;
    eo_cricket_vol = 0.0f;
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
    if (van_mode == VM_TITLE) {
        if (new_btn & (1u << B_PLAY))
            van_begin();
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
            van_play_q();
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
    } else if (van_mode == VM_BLINK) {
        (void)new_keys;                  /* a staring contest: you cannot skip it (HOME leaves) */
    } else if (van_mode == VM_MOMENT) {
        (void)new_keys;                  /* a moment: no skipping (HOME leaves) */
    } else if (van_mode == VM_LISTEN) {
        (void)new_keys;                  /* listening: no skipping (HOME leaves) */
    } else if (van_mode == VM_FISH) {
        (void)new_keys;                  /* a long look: no skipping (HOME leaves) */
    } else if (van_mode == VM_HUM) {
        (void)new_keys;                  /* ten seconds of it, whatever you press: that is the gag (HOME still leaves) */
    }
}

static void van_level_step(int32_t d)    /* K1, on the title only: another level, up to three above the highest you have completed */
{
    int32_t m = (int32_t)set.van_top + (int32_t)EO_UNLOCK, l;
    if (van_mode != VM_TITLE)
        return;
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
    ui.redraw = 1;
}

static void van_tick(uint32_t now)
{
    {   /* the radio plays in the next room: quiet, and turned down before every question and for the long gags */
        uint32_t quiet = van_mode == VM_ASK || van_mode == VM_HUM || van_mode == VM_FISH || van_mode == VM_BLINK || van_mode == VM_LISTEN || van_chips || (van_cur == 87u && (van_mode == VM_CARDS || van_mode == VM_CHOICE)) ||
                         (van_mode == VM_CARDS && van_after == VN_ASK);
        eo_radio_vol = van_mode == VM_MOMENT ? van_mom_radio(now) : quiet ? 0.0f : (pg_on && van_on_tv(now)) ? 0.3f : 1.0f;   /* ducked under Pong while it plays, not gone */
        eo_radio_on = 1;
    }
    if (van_mode == VM_BLINK) {
        if (now - van_blink_t0 >= VAN_BLINK_MS) {
            van_blink_stop();
        } else if (now - van_blink_t0 >= van_blink_tk * 1000u + 800u && van_blink_tk < 5u) {   /* a quiet tick each second */
            van_blink_tk++;
            eo_blub_soft = 0;
            eo_blub_note = 48;
            eo_blub_trig++;
        }
    }
    if (pg_on)
        van_pong_step(now);
    if (van_mode == VM_MOMENT)
        van_mom_step(now);
    if (van_mode == VM_LISTEN && now - van_lis_t0 > 5500u)
        van_fridge_stop();
    if (van_cr_once && now >= van_cr_once) {
        van_cr_once = 0;
        van_cricket(van_cr_vol);
    } else if (van_cr_q && now >= van_cr_next && !seq_on && (van_mode == VM_CARDS || van_mode == VM_ASK)) {   /* it lives on, now and then, for a couple of questions */
        van_cr_next = now + 12000u + van_rnd(9000u);
        van_cricket(0.007f);
    }
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
        van_play_q();
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
    if (van_mode == VM_FISH) {
        if (now - van_fish_t0 > VAN_FISH_MS) {
            van_fish_stop();
        } else if (now - van_fish_t0 >= van_fish_next) {     /* a bubble comes up now and then: a small plip, never on a beat */
            eo_blub_soft = 0;
            eo_blub_glide = 0.9f;
            eo_blub_note = 72 + (int32_t)van_rnd(13u);
            eo_blub_trig++;
            van_fish_bub++;
            van_fish_next = (now - van_fish_t0) + 450u + van_rnd(1200u);
        }
    }
    if (van_mode == VM_HUM && now - van_hum_t0 > VAN_HUM_MS) {
        eo_drone_set(-1, 0.0f);          /* the drone goes; the static's picture and hiss stay under the next two cards (\"...\" and the drawl) */
        van_tvshot = 1;
        van_say(P_IDLE, VN_BEAT, van_hum_after[0], van_hum_after[1], van_hum_after[2], 0);
    }
}

/* ---- the screen */
static void van_slant(int32_t y, int32_t h)   /* italics, by a shear: the top of the line leans right (the font has no italic) */
{
    int32_t j, x;
    for (j = 0; j < h; j++) {
        int32_t py = y + j - mv_oy, sh = (h - 1 - j) * 3 / h;
        uint16_t *row;
        if (py < 0 || py >= mv_h)
            continue;
        row = &cv_px[(uint32_t)(py + cv_oy) * cv_w];
        for (x = 239; x >= sh; x--)
            row[x] = row[x - sh];
        for (x = 0; x < sh; x++)
            row[x] = 0;
    }
}
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
static void van_room(int32_t num, int32_t den, int32_t px, int32_t py)   /* the room, punched in: scale num/den, window starts at px,py */
{
    int32_t y;
    const uint8_t *rle = EO_ROOM_RLE;
    const uint16_t *row = EO_ROOM_ROW, *pal = EO_ROOM_PAL;
    if (!eo_res_ok())
        return;
    for (y = 0; y < 144; y++) {
        int32_t sy = (y + py) * den / num, dy = y - mv_oy, x = 0;
        const uint8_t *p, *e;
        if (dy < 0 || dy >= mv_h || sy < 0 || sy >= (int32_t)EO_ROOM_ROWS)
            continue;
        p = rle + row[sy];
        e = rle + row[sy + 1];
        while (p < e) {
            if (p[0]) {
                int32_t x0 = x * num / den - px, x1 = (x + p[1]) * num / den - px;
                if (x1 > 240)
                    x1 = 240;
                if (x0 < 0)
                    x0 = 0;
                if (x1 > x0)
                    cv_rect(x0, dy, x1 - x0, 1, pal[p[0]]);
            }
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
        if (van_chips == 3u) {
            if (i == van_q_pick) {
                bg = EO_RGB(100, 40, 40);
                ed = EO_RGB(220, 120, 120);
            }
        } else if (van_chips == 2u) {
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
        mrect(x, 114, w, 28, ed);
        mrect(x + 1, 115, w - 2, 26, bg);
        tw = text_w(&FONT_S, van_lbl[i]);
        if (tw > w - 6)
            tw = text_w(&FONT_XS, van_lbl[i]);
        mtext(x + (w - tw) / 2, 120, tw > w - 6 || text_w(&FONT_S, van_lbl[i]) > w - 6 ? &FONT_XS : &FONT_S, van_lbl[i], fg);
    }
}
static void van_lens(int32_t fx, int32_t fy, int32_t R, int32_t sc, int32_t blk, int32_t m, int32_t dim, int32_t dark, int32_t clip);
static int32_t van_lens_px(int32_t fx, int32_t fy, int32_t R, int32_t sc, int32_t x, int32_t y);
/* The TV's glass, pixel by pixel (the approved preview): the dark glass (18, 24, 36), his face in it as 55% of his own light (screen blend), his face bulged by a lens
 * (a fisheye, strongest in the middle), a dimmer scanline on every even row, then the static (5 px blocks, 18% of them lit, grey 30 to 90, never darkening anything,
 * so it shows on his skin too) and a roll bar. He sways a little and breathes (his size changes). Drawn as runs of one colour along each row. */
static void van_tv_glass(uint32_t now, int32_t fy, uint32_t f)
{
    uint32_t tm = now - van_hum_t0;
    int32_t fx = 120 + (int32_t)(4.0f * fm_sinf((float)(tm % 12566u) * 0.0005f)), sc = 500 + (int32_t)(15.0f * fm_sinf((float)(tm % 10472u) * 0.0006f));
    int32_t w = 68 * sc / 100, h = 66 * sc / 100, ox = fx - w / 2, oy = fy - h * 4 / 10, R2 = 150 * 150, bar = (int32_t)((tm * 140u / 1000u) % 300u) - 30, x, y;
    int32_t face = eo_res_ok();
    if (face)
        spr_pal = EO_CAST_PAL[2];
    for (y = 0; y < 240; y++) {
        int32_t runx = 0;
        uint32_t runc = 0xFFFFFFFFu;
        if (y < mv_oy || y >= mv_oy + mv_h)
            continue;
        for (x = 0; x <= 240; x++) {
            uint32_t col = 0;
            if (x < 240) {
                uint32_t r = 18, g = 24, b = 36, h5;
                int32_t c = -1;
                if (face) {
                    int32_t dx = x - fx, dy = y - fy, r2 = dx * dx + dy * dy, k = r2 < R2 ? 450 + 550 * r2 / R2 : 1000;
                    int32_t lx = fx + dx * k / 1000 - ox, ly = fy + dy * k / 1000 - oy;
                    if (lx >= 0 && ly >= 0 && lx * 68 / w < 68 && ly * 66 / h < 66)
                        c = cast_px(EO_VAN[0], 95 - (14 + lx * 68 / w), ly * 66 / h);
                }
                if (c >= 0) {
                    uint32_t cr = (((uint32_t)c >> 11) & 31u) * 255u / 31u * 55u / 100u, cg = (((uint32_t)c >> 5) & 63u) * 255u / 63u * 55u / 100u, cb = ((uint32_t)c & 31u) * 255u / 31u * 55u / 100u;
                    r = 255u - (255u - 18u) * (255u - cr) / 255u;
                    g = 255u - (255u - 24u) * (255u - cg) / 255u;
                    b = 255u - (255u - 36u) * (255u - cb) / 255u;
                }
                if (!(y & 1)) {          /* the scanlines */
                    r = r * 80u / 100u;
                    g = g * 80u / 100u;
                    b = b * 80u / 100u;
                }
                h5 = ((((uint32_t)(x / 5) * 73856093u) ^ ((uint32_t)(y / 5) * 19349663u) ^ (f * 83492791u)) * 2654435761u);
                if ((h5 >> 8) % 100u >= 82u) {       /* a lit static block: grey, only ever lighter than what is there */
                    uint32_t n = 30u + ((h5 >> 24) & 255u) * 60u / 255u;
                    r = r > n ? r : n;
                    g = g > n ? g : n;
                    b = b > n ? b : n;
                }
                if (y >= bar && y < bar + 3) {      /* the roll bar */
                    r = r > 60u ? r : 60u;
                    g = g > 70u ? g : 70u;
                    b = b > 90u ? b : 90u;
                }
                col = EO_RGB(r, g, b);
            }
            if (col != runc) {
                if (runc != 0xFFFFFFFFu && x > runx)
                    mrect(runx, y, x - runx, 1, (uint16_t)runc);
                runx = x;
                runc = col;
            }
        }
    }
}
static void van_hum_scene(uint32_t now)   /* the TV's glass: white noise with Van's face showing through it, like a reflection */
{
    uint32_t i;
    int32_t f = (int32_t)(now / 60u), fyv = van_mode == VM_HUM ? 112 : 66;     /* under the cards the glass is only the top of the screen: his face sits higher */
    if (!van_tint[1]) {                  /* Van's colours washed toward the screen's blue, once */
        for (i = 1; i < 16u && eo_res_ok(); i++) {
            uint32_t c = EO_CAST_PAL[2][i], r = (c >> 11) & 31u, g = (c >> 5) & 63u, b = c & 31u;
            r = r * 6u / 10u;
            g = g * 7u / 10u + 6u;
            b = b * 8u / 10u + 10u;
            van_tint[i] = (uint16_t)((r << 11) | ((g > 63u ? 63u : g) << 5) | (b > 31u ? 31u : b));
        }
    }
    van_tv_glass(now, fyv, (uint32_t)f);
    for (i = 0; i < 26u; i++) {          /* the rounded corners of the glass */
        int32_t w = 0;
        while ((26 - w) * (26 - w) + (26 - (int32_t)i) * (26 - (int32_t)i) > 26 * 26 && w < 26)
            w++;
        mrect(0, (int32_t)i, w, 1, EO_RGB(8, 8, 12));
        mrect(240 - w, (int32_t)i, w, 1, EO_RGB(8, 8, 12));
        mrect(0, 239 - (int32_t)i, w, 1, EO_RGB(8, 8, 12));
        mrect(240 - w, 239 - (int32_t)i, w, 1, EO_RGB(8, 8, 12));
    }
}

static int32_t van_ghost_px(const void *s, int32_t x, int32_t y)    /* Van's face as a see-through reflection: every other pixel, washed pale */
{
    int32_t c = van_tint_px(s, x, y);
    if (c < 0 || ((x + y) & 1))
        return -1;
    return (int32_t)(uint16_t)((((uint32_t)c >> 1) & 0x7BEFu) + 0x2104u * 3u);
}
static int32_t van_glass_px(const void *s, int32_t x, int32_t y)   /* the static gag's face (solid, tinted) but dimmed, with scanlines: for the Pong glass */
{
    int32_t c = van_tint_px(s, x, y);
    uint32_t k = (y & 1) ? 2u : 3u, r, g, b;      /* every other row darker: the scanlines */
    if (c < 0)
        return -1;
    r = (((uint32_t)c >> 11) & 31u) * k / 8u + 1u;
    g = (((uint32_t)c >> 5) & 63u) * k / 8u + 3u;
    b = ((uint32_t)c & 31u) * k / 8u + 2u;
    return (int32_t)(uint16_t)((r << 11) | (g << 5) | b);
}
static void van_tint_init(void)
{
    uint32_t i;
    if (van_tint[1] || !eo_res_ok())
        return;
    for (i = 1; i < 16u; i++) {
        uint32_t c = EO_CAST_PAL[2][i], r = (c >> 11) & 31u, g = (c >> 5) & 63u, b = c & 31u;
        r = r * 6u / 10u;
        g = g * 7u / 10u + 6u;
        b = b * 8u / 10u + 10u;
        van_tint[i] = (uint16_t)((r << 11) | ((g > 63u ? 63u : g) << 5) | (b > 31u ? 31u : b));
    }
}
/* Van's face as a reflection in a curved glass: he is close, so the middle of his face is blown up (a fisheye) and it eases to normal at the lens edge.
 * (fx, fy) the face centre, R the lens radius, sc his size in tenths, blk the block size in pixels, m: draw one block in m (1 solid, 2 checker, 3 sparse, so you see through),
 * dim: brightness in eighths, dark: 1 = his dark lines let no light through (skipped), clip: 1 = only inside the fish bowl's round, mx/my the bowl's centre-offset */
static int32_t van_lens_px(int32_t fx, int32_t fy, int32_t R, int32_t sc, int32_t x, int32_t y)   /* the face's colour seen at the screen point (x, y) through the lens, or -1 */
{
    int32_t dx = x - fx, dy = y - fy, r2 = dx * dx + dy * dy, k = 1000, sx, sy, X, Y, R2 = R * R;
    if (r2 < R2)
        k = 450 + 550 * r2 / R2;         /* strongest magnification at the centre, easing to none at the lens edge */
    X = fx + dx * k / 1000 - (fx - 34 * sc / 10);        /* in his picture, in screen pixels from its left */
    Y = fy + dy * k / 1000 - (fy - 264 * sc / 100);
    if (X < 0 || Y < 0)
        return -1;
    sx = X * 10 / sc;
    sy = Y * 10 / sc;
    if (sx >= 68 || sy >= 66)
        return -1;
    return cast_px(EO_VAN[0], 95 - (sx + 14), sy);    /* mirrored: a reflection */
}
static void van_lens(int32_t fx, int32_t fy, int32_t R, int32_t sc, int32_t blk, int32_t m, int32_t dim, int32_t dark, int32_t clip)
{
    int32_t x, y, bh = dark == 2 ? 2 : blk;     /* dark 2: the old TV's glass: his light added to the dark screen, in scanlines */
    if (!eo_res_ok())
        return;
    spr_pal = EO_CAST_PAL[2];
    for (y = 0; y < 240; y += bh) {
        if (y + bh <= mv_oy || y >= mv_oy + mv_h)
            continue;
        for (x = 0; x < 240; x += blk) {
            int32_t c;
            if (((x / blk + y / bh) % m) != 0 && dark != 2)
                continue;
            if (clip) {
                int32_t ex = x - 120, ey = y - 122;
                if (ex * ex * 88 * 88 + ey * ey * 104 * 104 >= 104 * 104 * 88 * 88)    /* only inside the bowl's round */
                    continue;
            }
            c = van_lens_px(fx, fy, R, sc, x, y);
            if (c < 0 || (dark == 1 && ((c >> 5) & 63) < 16))     /* dark lines let no light through */
                continue;
            if (dark == 2) {             /* screen blend over the glass (18, 24, 36), 55% of his light, every other row a little dimmer */
                uint32_t r = (((uint32_t)c >> 11) & 31u) * 255u / 31u * 55u / 100u, g = (((uint32_t)c >> 5) & 63u) * 255u / 63u * 55u / 100u, b = ((uint32_t)c & 31u) * 255u / 31u * 55u / 100u;
                uint32_t k = ((y / bh) & 1) ? 80u : 100u;
                r = (255u - (255u - 18u) * (255u - r) / 255u) * k / 100u;
                g = (255u - (255u - 24u) * (255u - g) / 255u) * k / 100u;
                b = (255u - (255u - 36u) * (255u - b) / 255u) * k / 100u;
                mrect(x, y, blk, bh, EO_RGB(r, g, b));
                continue;
            }
            if (dim < 8)
                c = (int32_t)((((uint32_t)c >> 11) & 31u) * (uint32_t)dim / 8u << 11 | ((((uint32_t)c >> 5) & 63u) * (uint32_t)dim / 8u) << 5 | (((uint32_t)c & 31u) * (uint32_t)dim / 8u));
            mrect(x, y, blk, blk, (uint16_t)c);
        }
    }
}
static void van_fish_reflect(uint32_t now)   /* Van at the bowl: big, off to the left, seen through a dither so the fish shows */
{
    van_lens(68 + (int32_t)((now / 90u) % 20u > 10u ? 1 : 0) * 2, 94, 99, 27, 3, 2, 8, 1, 1);
}
static void van_fish_scene(uint32_t now)   /* the room behind (soft, out of focus), then Simon~'s bowl right up close, bubbles inside it, Van's face on the glass */
{
    uint32_t i, t = now - van_fish_t0;
    int32_t y;
    for (y = 0; y < 240; y += 8)         /* a dim wall and a sill */
        mrect(0, y, 240, 8, EO_RGB(46 + (uint32_t)y / 14u, 56 + (uint32_t)y / 16u, 80 + (uint32_t)y / 18u));
    mrect(150, 18, 76, 92, EO_RGB(120, 150, 185));     /* the window, a soft glow */
    mrect(0, 204, 240, 36, EO_RGB(86, 62, 48));
    mrect(0, 200, 240, 6, EO_RGB(120, 90, 66));
    mdisc(120, 232, 70, EO_RGB(50, 38, 32));           /* her shadow on the sill */
    if (eo_res_ok()) {                   /* her bowl, four times the size */
        spr_pal = EO_CAST_PAL[0];
        spr_paint(cast_px, EO_CAST[0][((now / 900u) & 1u) ? 1u : 0u], EO_CAST_W, EO_CAST_H, -70, -66 + (int32_t)((now / 700u) & 1u) * 2, 4, 1);
    }
    for (i = 0; i < 9u; i++) {           /* bubbles inside the bowl only */
        uint32_t h = (i + 1u) * 2654435761u;
        int32_t r = 2 + (int32_t)((h >> 20) % 4u), bx = 120 + (int32_t)((h >> 8) % 150u) - 75 + (int32_t)((t / 160u + i * 3u) % 6u) - 3;
        int32_t by = 210 - (int32_t)(((t * (14u + (h >> 12) % 16u)) / 100u + (h >> 4) % 180u) % 190u), ex = bx - 120, ey = by - 122;
        if (ex * ex * 88 * 88 + ey * ey * 104 * 104 >= 100 * 100 * 84 * 84)
            continue;
        mdisc(bx, by, r, EO_RGB(150, 210, 240));
        mdisc(bx, by, r - 1 > 0 ? r - 1 : 1, EO_RGB(90, 165, 215));
        mrect(bx - r / 2, by - r / 2, 2, 2, EO_RGB(235, 250, 255));
    }
    van_fish_reflect(now);
    mrect(38, 52, 4, 14, EO_RGB(235, 250, 255));       /* a glint on the glass */
    mrect(44, 44, 10, 3, EO_RGB(235, 250, 255));
}

static void van_bands(uint32_t now)      /* the room and the box under it (no header: the words are the game); the TV gag takes the whole glass */
{
    static const uint8_t HY[3] = {0, 80, 160}, NY[2] = {0, 144}, NH[2] = {144, 96};
    uint32_t i;
    if (van_mode == VM_HUM || van_mode == VM_FISH || van_mode == VM_BLINK || van_mode == VM_TITLE) {
        for (i = 0; i < 3u; i++) {
            mv_oy = HY[i];
            mv_h = 80;
            cv_begin(240, 80, C_BLACK);
            van_scene(now);
            cv_blit(0, HY[i]);
        }
        return;
    }
    for (i = 0; i < 2u; i++) {
        mv_oy = NY[i];
        mv_h = NH[i];
        cv_begin(240, NH[i], C_BLACK);
        van_scene(now);
        cv_blit(0, NY[i]);
    }
}

static uint32_t van_on_tv(uint32_t now)   /* the TV shot: the game runs. Any other shot is a cutaway on Van and the game waits */
{
    (void)now;
    return pg_on && (van_chips || van_mode == VM_ASK || van_mode == VM_CHOICE || (van_mode == VM_CARDS && van_cam == 3u));
}
static void van_pong_draw(uint32_t now)   /* the TV in close-up: the glass with Van's face faint in it (like the static), then the game on top */
{
    const int32_t fx = 32, fy = 16, bz = 6;
    uint16_t lit = EO_RGB(150, 230, 150), dim = EO_RGB(46, 92, 62), frame = EO_RGB(64, 58, 74);
    int32_t y, i;
    uint32_t f = now / 70u;
    char sc[8];
    mrect(0, 0, 240, 144, EO_RGB(22, 26, 38));
    mrect(fx - bz, fy - bz, PG_W + 2 * bz, PG_H + 2 * bz, EO_RGB(14, 22, 30));
    if (eo_res_ok()) {
        van_tint_init();
        van_lens(120, 66, 80, 17, 3, 3, 5, 1, 0);     /* further back from the glass: smaller, darker, mostly see-through */
    }
    for (i = 0; i < 70; i++) {           /* a little static: single specks, a new set every frame */
        uint32_t h = ((uint32_t)i * 2654435761u ^ f * 40503u) * 2246822519u;
        mrect(fx - bz + (int32_t)((h >> 8) % (uint32_t)(PG_W + 2 * bz)), fy - bz + (int32_t)((h >> 20) % (uint32_t)(PG_H + 2 * bz)), 1, 1, EO_RGB(120, 130, 140));
    }
    mrect(18, 8, 204, fy - bz - 8, frame);                                  /* the bezel, over anything that spilled */
    mrect(18, fy + PG_H + bz, 204, 136 - (fy + PG_H + bz), frame);
    mrect(18, 8, fx - bz - 18, 128, frame);
    mrect(fx + PG_W + bz, 8, 222 - (fx + PG_W + bz), 128, frame);
    for (i = 0; i < 4; i++) {            /* rounded glass: a stepped corner at each end */
        int32_t cx = (i & 1) ? fx + PG_W + bz - 1 : fx - bz, cy = (i & 2) ? fy + PG_H + bz - 1 : fy - bz;
        int32_t sx = (i & 1) ? -1 : 1, sy = (i & 2) ? -1 : 1, k;
        for (k = 0; k < 4; k++)
            mrect(sx > 0 ? cx : cx - (3 - k), sy > 0 ? cy + k : cy - k, 4 - k, 1, frame);
    }
    for (y = 0; y < PG_H; y += 8)        /* the dashed line down the middle */
        mrect(fx + PG_W / 2 - 1, fy + y, 2, 4, dim);
    mrect(fx + 6, fy + pg_vp / 16 - PG_PH / 2, 4, PG_PH, lit);                 /* Van's */
    mrect(fx + PG_W - 10, fy + pg_pp / 16 - PG_PH / 2, 4, PG_PH, lit);         /* yours */
    mrect(fx + pg_bx / 16 - 2, fy + pg_by / 16 - 2, 5, 5, lit);
    sc[0] = (char)('0' + (pg_sv > 9u ? 9u : pg_sv));
    sc[1] = 0;
    mtext(fx + PG_W / 2 - 20, fy - 2, &FONT_XS, sc, dim);                       /* the score, quiet: Van's, then yours */
    sc[0] = (char)('0' + (pg_sp > 9u ? 9u : pg_sp));
    mtext(fx + PG_W / 2 + 14, fy - 2, &FONT_XS, sc, dim);
}

static void van_title_scene(uint32_t now)   /* before the chat: Van on his couch, what the game is, and the level to start at (K1) */
{
    char lv[48];
    const char *nm = van_level_name[(set.van_lvl >= 1u && set.van_lvl <= VAN_LEVELS ? set.van_lvl : 1u) - 1u];
    mrect(0, 0, 240, 240, C_BLACK);
    van_room(1, 1, 0, 0);
    van_paint(P_LOUNGE, 48, 4, 3, 2, now, 0);
    mrect(0, 0, 240, 16, EO_RGB(10, 16, 24));
    mtext(8, 0, &FONT_S, "NAME THAT INTERVAL", EO_RGB(110, 200, 90));
    mrect(0, 144, 240, 96, C_BLACK);
    mrect(0, 144, 240, 2, EO_RGB(110, 200, 90));
    mtext(8, 148, &FONT_S, "A game about nothing...", C_WHITE);
    mtext(8, 164, &FONT_S, "besides intervals...", C_WHITE);
    mtext(8, 180, &FONT_S, "and maybe life...", C_WHITE);
    mtext(8, 196, &FONT_S, "and snacks.", C_WHITE);
    lv[0] = 0;
    text_cat(lv, "LEVEL ");
    {
        char d[2] = {(char)('0' + set.van_lvl), 0};
        text_cat(lv, d);
    }
    text_cat(lv, "  ");
    text_cat(lv, nm);
    mtext(8, 216, &FONT_XS, lv, C_AMB);
    mtext(8, 228, &FONT_XS, "K1 LEVEL      PLAY START", C_GRAY);
}

static void van_scene(uint32_t now)
{
    uint32_t i;
    const char *c = van_cd[van_ci];
    int32_t bob = ((now / 600u) & 1u) ? 1 : 0;
    if (van_mode == VM_TITLE) {
        van_title_scene(now);
        return;
    }
    if (van_mode == VM_MOMENT) {
        van_mom_scene(now);
        mrect(0, 144, 240, 96, C_BLACK);
        mrect(0, 144, 240, 2, EO_RGB(110, 200, 90));
        if (van_cn)
            mtext(8, 150, &FONT_S, van_text(van_cd[van_ci]), van_kind(van_cd[van_ci]) == 1u ? EO_RGB(150, 190, 235) : C_WHITE);
        return;
    }
    if (van_mode == VM_LISTEN) {          /* the room punched in on the fridge, trembling a pixel with the hum */
        van_room(2, 1, 200, 40 + (int32_t)((now / 90u) & 1u));
        mrect(0, 144, 240, 96, C_BLACK);
        mrect(0, 144, 240, 2, EO_RGB(110, 200, 90));
        mtext(8, 150, &FONT_S, van_text(van_cd[van_ci]), C_WHITE);
        return;
    }
    if (van_mode == VM_HUM) {
        van_hum_scene(now);
        return;
    }
    if (van_mode == VM_FISH) {
        van_fish_scene(now);
        return;
    }
    if (van_mode == VM_BLINK) {          /* full screen on his face, the whole time, until he gives */
        uint32_t t = now - van_blink_t0;
        mrect(0, 0, 240, 240, EO_RGB(20, 36, 44));
        van_paint(P_TALK, -24, -22, 3, 1, now, 0);
        if (t + 220u >= VAN_BLINK_MS)    /* the blink: the whole screen goes dark for a moment */
            mrect(0, 0, 240, 240, C_BLACK);
        return;
    }
    if (van_tvshot && van_mode == VM_CARDS) {
        van_hum_scene(now);              /* still the TV: the static, his face on the glass */
    } else if (van_on_tv(now)) {
        van_pong_draw(now);
        if (van_chips || van_mode == VM_CHOICE)
            van_chip_row(now);
    }
    else {
        int32_t big = van_chips || van_mode == VM_CHOICE;
        uint32_t talk = van_mode == VM_CARDS && van_typed(now) < van_len();
        uint32_t cam = (van_chips || van_mode == VM_CHOICE) ? 0u : van_cam;   /* the chips need the wide shot */
        spr_flip = cam && van_flip && van_pose != P_FISH;
        if (cam == 1u) {                 /* medium: the room punched in 1.5x, Van from the chest up at 2x */
            van_room(3, 2, van_flip ? 90 : 30, 40);
            van_paint(van_pose, van_flip ? 20 : 32, -34, 2, 1, now, talk);
        } else if (cam == 2u) {          /* close-up: the wall 2x behind, his face 3x */
            van_room(2, 1, van_flip ? 200 : 40, 60);
            van_paint(van_pose, -24 + (van_flip ? 12 : 0), 6, 3, 1, now, talk);
        } else {                         /* wide: the whole room, Van on the couch */
            van_room(1, 1, 0, 0);
            van_paint(van_pose, big ? 56 : 48, big ? 12 : 4, big ? 4 : 3, big ? 3 : 2, now, talk);
        }
        spr_flip = 0;
        if (van_pose == P_FISH) {        /* Simone, in her bowl, on the sill (her sprite from her own game) */
            mrect(176, 84, 60, 50, EO_RGB(150, 200, 230));
            mrect(178, 86, 56, 46, EO_RGB(40, 90, 150));
            if (eo_res_ok()) {
                spr_pal = EO_CAST_PAL[0];
                spr_paint(cast_px, EO_CAST[0][((now / 700u) & 1u) ? 1u : 0u], EO_CAST_W, EO_CAST_H, 186, 92 + bob * 2, 1, 2);
            }
        }
        if (van_chips || van_mode == VM_CHOICE)
            van_chip_row(now);
    }
    /* the dialogue box: five lines of the bigger font */
    mrect(0, 144, 240, 96, C_BLACK);
    mrect(0, 144, 240, 2, EO_RGB(110, 200, 90));
    {
        uint32_t n = van_typed(now), k, kind = van_kind(c), li = 0, ws;
        const char *s = van_text(c);
        uint16_t col = kind == 1u ? EO_RGB(150, 190, 235) : C_WHITE;
        if (kind == 2u) {
            van_gibber(158, n, now);
        } else if (kind == 3u) {         /* a drawl: one line, big, a letter at a time, in the middle of the box */
            char buf[64];
            uint32_t m = 0, j, len = eo_strlen(s);
            const felucca_font_t *ft = &FONT_L;
            for (j = 0; j < len && j < n && m < 62u; j++)
                buf[m++] = s[j];
            buf[m] = 0;
            if (text_w(ft, s) > 228)
                ft = &FONT_M;
            mtext((240 - text_w(ft, s)) / 2, 172, ft, buf, col);
        } else {
            for (k = 0; k < van_ln; k++)
                if (n >= van_ls[k])
                    li = k;
            ws = li >= 4u ? li - 3u : 0u;     /* four lines of the medium font show; more lines scroll up as he types */
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
                mtext(8, 147 + (int32_t)k * 23, &FONT_M, buf, col);
                if (kind == 1u)
                    van_slant(147 + (int32_t)k * 23, 23);
                for (j = 0; j < len; j++)        /* the e-acute: an e with its accent drawn by hand */
                    if (s[st + j] == '~') {
                        char pre[64];
                        uint32_t q;
                        for (q = 0; q < j; q++)
                            pre[q] = buf[q];
                        pre[q] = 0;
                        mrect(8 + text_w(&FONT_M, pre) + text_w(&FONT_M, "e") / 2 + 1, 146 + (int32_t)k * 23, 4, 2, col);
                        mrect(8 + text_w(&FONT_M, pre) + text_w(&FONT_M, "e") / 2 - 2, 148 + (int32_t)k * 23, 4, 2, col);
                    }
            }
        }
        (void)i;
        if (van_mode == VM_CARDS && n >= van_len() && ((now / 450u) & 1u)) {
            int32_t r;
            for (r = 0; r < 5; r++)
                mrect(226 + r, 230 + r, 10 - 2 * r, 1, C_AMB);
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
