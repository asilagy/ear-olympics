/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 asilagy */
/* Ear Olympics (0.0.3): two games on one shell, kept apart on purpose.
 *   SIMON    follow the lights. A sequence plays, lit as it sounds; you play it back; each round adds a note; a
 *            wrong note ends the game. No root note: nothing to establish. (eo_simon.h)
 *   COPYCAT  ear training. The root sounds alone (ROOT, its key lit), then a melody plays over it, not lit (only level
 *            1 lights it); you play the melody back. The root is a foundation, not a note of the melody: it is a soft
 *            drone that keeps sounding under the melody (and, at the lower levels, your turn). (eo_copycat.h)
 *   PRESETS picks the game, KNOB 1 its level, SELECT Copycat's root (C4 .. G4: the speaker is small, so the notes stay
 *   at C4 and above). PLAY starts; HOME stops; SAVE keeps the settings (a new best is also kept after a round).
 *   The key LEDs at three levels: Copycat's root mid (bright while it is introduced), the level's other notes dim,
 *   the sounding or held key bright.
 * Part of the unity build (device/unity.c, host/plat_host.c), after the platform: gfx.c and announce.c are in
 * scope. Names added here start eo_: the platform's chip code (bsp/chip/) has plain names of its own (a variable
 * called rng once stopped the device build). */
#include "fastmath.h"
#include "eo_music.h"
#include "eo_copycat.h"
#include "eo_simon.h"
#include "eo_cat.h"
#include "eo_dogtor.h"
#include "eo_intro.h"
#include "eo_cast.h"
#include "eo_title.h"

enum { GAME_SIMON, GAME_COPYCAT, GAME_DOGTOR, GAMES };
static const char *const eo_game_name[GAMES] = {"SIMONE SAYS", "COPYCAT", "CHORD DOGTOR"};
static const uint8_t eo_levels[GAMES] = {SIMON_LEVELS, COPYCAT_LEVELS, DOGTOR_LEVELS};

#define SET_MAGIC 0x374C4F45u            /* "EOL7": the settings record; another magic loads defaults */
#define EO_CELEBRATE_MS 3200u               /* the level-up celebration, before the GOOD JOB card */
#define EO_UNLOCK 3u                     /* you may play this many levels above the highest you have completed */
#define EO_ROUNDS_UP 10u                 /* rounds played right to graduate a level (Simon: a sequence this long) */
typedef struct {
    uint32_t magic;
    uint8_t game;                        /* GAME_SIMON or GAME_COPYCAT */
    uint8_t root;                        /* Copycat's root key: ROOT_MIN .. ROOT_MAX (key 0 is F3) */
    uint8_t level[GAMES];                /* each game's level, from 1 */
    uint8_t prog[GAMES];                 /* Copycat: melodies played right at this level, toward the level's rounds */
    uint8_t top[GAMES];                  /* the highest level completed (10 rounds right): levels up to top + 3 are open */
    uint8_t voice;                       /* the melody voice: V_MEOW ... */
    uint8_t speed;                       /* playback speed in steps of 5%: 5..40 is 25% to 200% (20 is normal) */
    uint8_t mode;                        /* 0 challenge, 1 practice */
    uint8_t pad;
    uint16_t best[GAMES];                /* Simon: the longest sequence; Copycat: the longest streak */
} settings_t;
static settings_t set;
static uint8_t set_dirty;                /* changed since the last save: SAVE is lit */

#define KEY_NOTE0 53                     /* the MIDI note of key 0 (F3) */
#define ROOT_MIN 7u                      /* C4: the speaker is small and a low note is weak, so the game stays at C4 up */
#define ROOT_MAX 14u                     /* G4: a melody up to an octave above the root still fits the keyboard */
#define SIMON_BASE 7u                    /* Simon's lowest key: C4 */
#define ROOT_MS 1000u                    /* Copycat: the root alone, before the melody */
#define GLOW 2                           /* how bright the dim LEDs are: 1 low, 2 high (plat_lights) */
#define VOICE_GAIN 0.45f                 /* the melody voice's level before MASTER: 1.0 is full scale */
#define DRONE_GAIN 0.30f                 /* the root drone's level (it is also softer in tone) */

/* ------------------------------------------------------------------ voices --- */
/* the melody voice (the keys, a sequence's notes: the last note wins) and the root drone. The tones are tables of a few
 * harmonics: the FM-1's speaker barely plays a low note's fundamental, and a sine has nothing else, but the ear hears
 * a low pitch from its harmonics, which the speaker does play. The drone's table is rounder (fewer, softer harmonics),
 * so it sounds like a foundation and not like a note of the melody. The main loop and the 1 kHz tick set them; the
 * audio interrupt plays them. */
#define WT_N 256u
enum { V_MEOW, V_WARM, V_BELL, V_EPIANO, V_TRI, V_SQUARE, VOICES,   /* VOICES: the ones you can pick */
       V_CATFX = VOICES, V_CATSAD, V_PURR, V_HOWL, V_PING, VOICES_ALL };            /* the cat's success meow, its sad yowl, its purr */
static const char *const eo_voice_name[VOICES] = {"MEOW", "WARM", "BELL", "E-PIANO", "TRIANGLE", "SQUARE"};
/* each voice's level before MASTER, to make them about equally loud (measured in the simulator) */
static const float eo_voice_gain[VOICES_ALL] = {1.15f, 1.0f, 1.3f, 1.5f, 1.25f, 0.8f, 1.15f, 1.0f, 1.6f, 1.2f, 0.5f};
/* one cycle each, normalised, first sample again at the end: warm (the old voice), the drone, triangle, square, and a
 * bright saw for the meow's resonances */
static float eo_wt[WT_N + 1u], eo_wt_soft[WT_N + 1u], eo_wt_tri[WT_N + 1u], eo_wt_sq[WT_N + 1u], eo_wt_saw[WT_N + 1u];
static volatile uint8_t eo_voice;        /* the melody voice's sound: V_* (mirrors the setting; Simone has her own, a bell) */
static volatile uint8_t voice_fx = 0xFFu; /* the sequence's own voice for the note sounding now (V_CATFX, V_CATSAD), or 0xFF */

static volatile uint32_t eo_midi_keys;       /* the keys held from MIDI (a bit per key), as if pressed on the FM-1 */
static volatile int32_t voice_note = -1;     /* the melody voice: MIDI note, -1 = released */
static volatile float voice_vel = 0.8f;      /* 0..1 */
static volatile int32_t eo_drone_note = -1;  /* the drone: MIDI note, -1 = off */
static volatile float eo_drone_vol = 1.0f;   /* 0..1 */
static float voice_phase, voice_level, eo_drone_phase, eo_drone_level, gain_now;   /* gain_now glides to MASTER */
#define CHORD_VOICES 4
#define CHORD_GAIN 0.15f                 /* each chord note's level */
#define BEEP_GAIN 0.09f                  /* the heart monitor */
static volatile int32_t eo_chord_note[CHORD_VOICES] = {-1, -1, -1, -1};   /* Chord Dogtor's chord: MIDI notes, -1 = off */
static volatile uint8_t eo_soft_env;     /* 1: the melody voice fades in and out slowly (Chord Dogtor's ostinato) */
#define RV_N 2048u
static float eo_rv[3][RV_N], eo_rv_lp[3];   /* a small reverb (three damped combs): only the ostinato is sent into it */
static uint32_t eo_rv_i[3];
static const uint16_t eo_rv_len[3] = {1087, 1423, 1877};
static volatile uint8_t eo_water;        /* the crystal ball: the sound goes under water (low-pass, delay, reverb) */
static volatile uint32_t eo_smp_trig, eo_smp_halt;   /* the intro music: the main loop counts a start / a stop up */
static volatile uint8_t eo_smp_run;
static float eo_ulaw[256];               /* mu-law bytes to floats, once */
#define SMP_GAIN 1.1f
static volatile uint32_t eo_rec_stop, eo_rec_go, eo_rec_up;   /* Simone's intro: the music slows to a stop like a record (stop), or is whole again (go) */
static volatile uint32_t eo_bass_trig;  /* the select's bass: a plucked synth note (MIDI note, and how hard), one per trigger */
static volatile int32_t eo_bass_note;
static volatile float eo_bass_vel = 1.0f;
#define BASS_GAIN 0.28f
static volatile uint32_t eo_blub_trig;   /* her typing: a short bubbly chirp; its MIDI note */
static volatile int32_t eo_blub_note;
static volatile uint8_t eo_rev;          /* 1: the bell plays reversed: it swells in and stops, like a tape run backwards */
static volatile float eo_bend_v = 1.0f;  /* the record's speed now (for the tests) */
#define BLUB_GAIN 0.10f
static volatile uint32_t eo_beep_trig;   /* the main loop counts this up for each heart-monitor beep */
static float eo_cph[CHORD_VOICES], eo_clev[CHORD_VOICES];

static void eo_wavetable(float *t, const float *h, uint32_t nh)
{
    uint32_t i, k;
    float peak = 0.0f;
    for (i = 0; i < WT_N; i++) {
        float s = 0.0f;
        for (k = 0; k < nh; k++)
            s += h[k] * fm_sinf(6.2831853f * (float)((i * (k + 1u)) % WT_N) / (float)WT_N);
        t[i] = s;
        if (fm_fabsf(s) > peak)
            peak = fm_fabsf(s);
    }
    for (i = 0; i < WT_N; i++)
        t[i] /= peak;
    t[WT_N] = t[0];
}

static void eo_tones_init(void)
{
    static const float H[7] = {1.0f, 0.85f, 0.6f, 0.4f, 0.28f, 0.18f, 0.1f};   /* warm: harmonics 1 to 7 */
    static const float D[3] = {1.0f, 0.4f, 0.15f};                              /* the drone: soft and round */
    static const float T[7] = {1.0f, 0.0f, 0.111f, 0.0f, 0.04f, 0.0f, 0.0204f}; /* triangle: odd, 1/k squared */
    static const float Q[7] = {1.0f, 0.0f, 0.333f, 0.0f, 0.2f, 0.0f, 0.143f};   /* square: odd, 1/k, seven only */
    static const float S[12] = {1.0f, 0.5f, 0.333f, 0.25f, 0.2f, 0.167f, 0.143f, 0.125f, 0.111f, 0.1f, 0.091f, 0.083f};
    eo_wavetable(eo_wt, H, 7);
    eo_wavetable(eo_wt_soft, D, 3);
    eo_wavetable(eo_wt_tri, T, 7);
    eo_wavetable(eo_wt_sq, Q, 7);
    eo_wavetable(eo_wt_saw, S, 12);
    {   /* G.711 mu-law decode table */
        uint32_t b;
        for (b = 0; b < 256u; b++) {
            uint32_t u = ~b & 0xFFu, ex = (u >> 4) & 7u, ma = u & 15u;
            int32_t mag = (int32_t)(((ma << 3) + 0x84u) << ex) - 0x84;
            eo_ulaw[b] = (float)((u & 0x80u) ? -mag : mag) / 32768.0f;
        }
    }
}

static inline float eo_lookup(const float *t, float phase)     /* a table, interpolated; phase is 0..1 */
{
    float x = phase * (float)WT_N;
    uint32_t k = (uint32_t)x;
    return t[k] + (t[k + 1u] - t[k]) * (x - (float)k);
}

static inline float eo_note_inc(int32_t note)    /* a note's phase step per frame */
{
    return 440.0f * fm_exp2f((float)(note - 69) / 12.0f) / 44100.0f;
}

/* under water (the crystal ball): everything goes through a low-pass, a 180 ms echo and a small reverb, faded in and out */
#define WF_D 7938u
#define WF_C1 1423u
#define WF_C2 1811u
#define WF_C3 2113u
static int16_t wf_d[WF_D], wf_c1[WF_C1], wf_c2[WF_C2], wf_c3[WF_C3];
static uint32_t wf_i, wf_j1, wf_j2, wf_j3;
static uint8_t eo_sleep, wf_dream;      /* (sleepy: the same buffers, a brighter and much longer wash) */
static float wf_lp, wf_lp2, wf_wet, wf_k1, wf_k2, wf_k3;   /* (the k's damp each reverb loop: the highs die first, so it never rings like metal) */
static uint8_t wf_was;
static int16_t wf_clamp(float x)
{
    return (int16_t)(x > 32000.0f ? 32000.0f : x < -32000.0f ? -32000.0f : x);
}
static float eo_underwater(float s)
{
    float x, d, r, y1, y2, y3;
    if (eo_water && !wf_was) {           /* going in: forget the last time's echoes */
        uint32_t k;
        for (k = 0; k < WF_D; k++)
            wf_d[k] = 0;
        for (k = 0; k < WF_C1; k++)
            wf_c1[k] = 0;
        for (k = 0; k < WF_C2; k++)
            wf_c2[k] = 0;
        for (k = 0; k < WF_C3; k++)
            wf_c3[k] = 0;
        wf_lp = wf_lp2 = wf_k1 = wf_k2 = wf_k3 = 0.0f;
    }
    if (eo_water && !wf_was)
        wf_dream = eo_sleep;
    wf_was = eo_water;
    wf_wet += ((eo_water ? 1.0f : 0.0f) - wf_wet) * (eo_water ? 0.00012f : 0.00004f);   /* in over about 0.2 s, out over 0.6 s */
    if (!eo_water && wf_wet < 0.002f) {
        wf_wet = 0.0f;
        return s;
    }
    wf_lp += (s - wf_lp) * (wf_dream ? 0.5f : 0.11f);        /* about 800 Hz under water; about 5 kHz when sleepy */
    wf_lp2 += (wf_lp - wf_lp2) * (wf_dream ? 0.8f : 0.5f);   /* a second pole: softer still */
    x = wf_lp2;
    d = (float)wf_d[wf_i];
    wf_d[wf_i] = wf_clamp(x + d * (wf_dream ? 0.66f : 0.5f));
    if (++wf_i >= WF_D)
        wf_i = 0;
    y1 = (float)wf_c1[wf_j1];
    y2 = (float)wf_c2[wf_j2];
    y3 = (float)wf_c3[wf_j3];
    {
        float dk = wf_dream ? 0.2f : 0.3f;
        wf_k1 += (y1 - wf_k1) * dk;
        wf_k2 += (y2 - wf_k2) * dk;
        wf_k3 += (y3 - wf_k3) * dk;
    }
    {
        float fb = wf_dream ? 0.86f : 0.64f;
        wf_c1[wf_j1] = wf_clamp(x + wf_k1 * (fb + 0.02f));
        wf_c2[wf_j2] = wf_clamp(x + wf_k2 * fb);
        wf_c3[wf_j3] = wf_clamp(x + wf_k3 * (fb - 0.02f));
    }
    if (++wf_j1 >= WF_C1)
        wf_j1 = 0;
    if (++wf_j2 >= WF_C2)
        wf_j2 = 0;
    if (++wf_j3 >= WF_C3)
        wf_j3 = 0;
    r = (y1 + y2 + y3) * (1.0f / 3.0f);
    return s * (1.0f - wf_wet) + wf_wet * (x + (wf_dream ? 0.6f : 0.45f) * d + (wf_dream ? 0.75f : 0.4f) * r);
}

void app_render(int32_t *lr, uint32_t n, uint32_t master_q12)
{
    static int32_t last_note = -1;       /* the melody voice's note at the last block: a new one restarts its envelopes */
    static float age, e1, e2, e3, ph2, ph3, s1l, s1b, s2l, s2b;
    static float batk;                   /* the bell's strike ramp: 0 to 1 over 4 ms at each new note */
    uint32_t i, pkt;
    int port;
    int32_t note = voice_note, dnote = eo_drone_note;
    uint32_t vc = voice_fx < VOICES_ALL ? voice_fx : eo_voice < VOICES ? eo_voice : 0u;
    float target = note >= 0 ? VOICE_GAIN * voice_vel * eo_voice_gain[vc] : 0.0f;
    float dtarget = dnote >= 0 ? DRONE_GAIN * eo_drone_vol : 0.0f;
    static float last_inc;               /* a released note keeps its pitch while it fades: a frozen phase would leave a thump */
    float inc = note >= 0 ? eo_note_inc(note) : last_inc;
    float vk = eo_soft_env ? 0.0006f : 0.002f;
    if (vc == V_BELL && !eo_soft_env)    /* the bell: a quick strike (~5 ms) and a softer release (~25 ms), so it never clicks */
        vk = target > voice_level ? 0.005f : 0.0009f;
    static float lfo_ph;                 /* the ostinato swells in volume and fades back, about every 3.5 s */
    if (eo_soft_env) {
        lfo_ph += 0.28f * (float)n / 44100.0f;
        if (lfo_ph >= 1.0f)
            lfo_ph -= 1.0f;
        target *= 0.65f + 0.35f * fm_sinf(6.2831853f * lfo_ph);
    }
    float dinc = dnote >= 0 ? eo_note_inc(dnote) : 0.0f;
    if (note >= 0)
        last_inc = inc;
    float gain = (float)master_q12 / 4096.0f * 8300000.0f;   /* MASTER, read once a block */
    float f1 = 0.0f, f2 = 0.0f, mamp = 1.0f, inc_m = inc;
    static uint32_t beep_seen, smp_seen, smp_hseen, stop_seen, go_seen, up_seen, blub_seen, bass_seen;
    static float bend = 1.0f, blub_age = 1.0f, blub_ph, blub_inc, bass_age = 1.0f, bass_ph, bass_inc, bass_lp, bass_v;
    static uint8_t stopping, rising;
    static float smp_pos, smp_g;
    static uint8_t smp_run, smp_stop;
    static float beep_age = 1.0f, beep_ph;
    float cinc[CHORD_VOICES], ctgt[CHORD_VOICES];
    for (i = 0; i < CHORD_VOICES; i++) {
        int32_t cn = eo_chord_note[i];
        ctgt[i] = cn >= 0 ? CHORD_GAIN : 0.0f;
        cinc[i] = cn >= 0 ? eo_note_inc(cn) : 0.0f;
    }
    if (smp_seen != eo_smp_trig) {       /* the intro music starts from its first sample */
        smp_seen = eo_smp_trig;
        smp_pos = 0.0f;
        smp_g = 1.0f;
        smp_run = 1;
        smp_stop = 0;
    }
    if (smp_hseen != eo_smp_halt) {      /* and stops, fading out over a few ms */
        smp_hseen = eo_smp_halt;
        smp_stop = 1;
    }
    if (go_seen != eo_rec_go) {          /* the record is whole again */
        go_seen = eo_rec_go;
        stopping = 0;
        rising = 0;
        bend = 1.0f;
    }
    if (up_seen != eo_rec_up) {          /* the record spins up: very low, rising to full speed */
        up_seen = eo_rec_up;
        stopping = 0;
        rising = 1;
        bend = 0.04f;
    }
    if (stop_seen != eo_rec_stop) {      /* the record is slowing down */
        stop_seen = eo_rec_stop;
        stopping = 1;
    }
    if (bass_seen != eo_bass_trig) {     /* a bass note: restart its pluck (a new note starts from silence, so no click) */
        bass_seen = eo_bass_trig;
        bass_age = 0.0f;
        bass_inc = eo_note_inc(eo_bass_note);
        bass_v = eo_bass_vel;
    }
    if (blub_seen != eo_blub_trig) {
        blub_seen = eo_blub_trig;
        blub_age = 0.0f;
        blub_ph = 0.0f;
        blub_inc = eo_note_inc(eo_blub_note);
    }
    if (beep_seen != eo_beep_trig) {
        beep_seen = eo_beep_trig;
        beep_age = 0.0f;
        beep_ph = 0.0f;
    }
    if (note != last_note) {             /* a new note: the voices that change over time start again */
        if (note >= 0) {                 /* (a release restarts nothing: the tail just fades, or the partials would jump back up: a tap) */
            age = 0.0f;
            e1 = e2 = e3 = 1.0f;
            batk = 0.0f;
            ph2 = ph3 = 0.0f;
            s1l = s1b = s2l = s2b = 0.0f;
        }
        last_note = note;
    }
    if (vc == V_CATSAD && note >= 0) {   /* the fail sound: a long sad "meeeow", the pitch sagging and wobbling */
        float u = age * (1.0f / 0.8f);
        u = u > 1.0f ? 1.0f : u;
        f1 = 2.0f * fm_sinf(3.1415927f * (850.0f - 300.0f * u) / 44100.0f);
        f2 = 2.0f * fm_sinf(3.1415927f * (1500.0f - 600.0f * u) / 44100.0f);
        mamp = 1.0f - 0.3f * u;
        inc_m = inc * (1.0f - 0.45f * u) * (1.0f + 0.03f * u * fm_sinf(6.2831853f * 6.0f * age));
    } else if (vc == V_HOWL && note >= 0) {   /* a happy "awoooo": the pitch swoops up, holds with a little vibrato, and falls away */
        float u = age < 0.5f ? age * 2.0f : 1.0f, g = age < 0.35f ? age * (1.0f / 0.35f) : 1.0f, fall = age > 0.8f ? (age - 0.8f) * (1.0f / 0.35f) : 0.0f;
        fall = fall > 1.0f ? 1.0f : fall;
        f1 = 2.0f * fm_sinf(3.1415927f * (520.0f - 200.0f * u) / 44100.0f);     /* "aw" closing to "oo" */
        f2 = 2.0f * fm_sinf(3.1415927f * (1050.0f - 300.0f * u) / 44100.0f);
        mamp = 1.0f - 0.5f * fall;
        inc_m = inc * (0.72f + 0.28f * g) * (1.0f + 0.013f * g * fm_sinf(6.2831853f * 5.5f * age)) * (1.0f - 0.12f * fall);
    } else if ((vc == V_MEOW || vc == V_CATFX) && note >= 0) {
        /* "mee-ow": two resonances sweep from ee to ow. The pitch hardly moves for a melody's notes (you must copy
         * them), and rises and falls a lot for the cat's own success meow. */
        float u = age * (1.0f / 0.42f), sw;
        u = u > 1.0f ? 1.0f : u;
        sw = fm_sinf(3.1415927f * u);
        f1 = 2.0f * fm_sinf(3.1415927f * (400.0f + 500.0f * sw) / 44100.0f);
        f2 = 2.0f * fm_sinf(3.1415927f * (2200.0f - 1300.0f * u) / 44100.0f);
        mamp = 0.7f + 0.3f * sw;
        inc_m = inc * (1.0f + (vc == V_CATFX ? 0.14f : 0.006f) * sw);
    }
    while (plat_midi_in(&pkt, &port)) {  /* a MIDI keyboard plays the keys: notes 53 to 79 (F3 to G5) are the FM-1's 27 */
        uint32_t st = (pkt >> 8) & 0xF0u, d1 = (pkt >> 16) & 0x7Fu, d2 = pkt >> 24;
        if ((st == 0x90u || st == 0x80u) && d1 >= (uint32_t)KEY_NOTE0 && d1 < (uint32_t)(KEY_NOTE0 + NKEYS)) {
            uint32_t bit = 1u << (d1 - (uint32_t)KEY_NOTE0);
            if (st == 0x90u && d2)
                eo_midi_keys |= bit;
            else
                eo_midi_keys &= ~bit;
        }
    }
    for (i = 0; i < n; i++) {
        float s, v = 0.0f;
        voice_level += (target - voice_level) * vk;          /* ~10 ms attack and release (the ostinato: ~30 ms) */
        eo_drone_level += (dtarget - eo_drone_level) * 0.002f;
        gain_now += (gain - gain_now) * 0.004f;              /* ~5 ms: turning MASTER does not click */
        if (rising) {                                        /* 1.1 s from a low growl up to the real pitch */
            bend += 1.0f / (44100.0f * 1.1f);
            if (bend >= 1.0f) {
                bend = 1.0f;
                rising = 0;
            }
        }
        if (stopping && bend > 0.0f) {                       /* 0.7 s from full speed to a stop */
            bend -= 1.0f / (44100.0f * 0.7f);
            if (bend < 0.0f)
                bend = 0.0f;
        }
        voice_phase += ((vc == V_MEOW || vc == V_CATFX || vc == V_CATSAD || vc == V_HOWL) ? inc_m : inc) * bend;
        if (voice_phase >= 1.0f)
            voice_phase -= 1.0f;
        eo_drone_phase += dinc * bend;
        if (eo_drone_phase >= 1.0f)
            eo_drone_phase -= 1.0f;
        if (voice_level > 0.00002f) {
            switch (vc) {
            case V_WARM:
                v = eo_lookup(eo_wt, voice_phase);
                break;
            case V_TRI:
                v = eo_lookup(eo_wt_tri, voice_phase);
                break;
            case V_SQUARE:
                v = eo_lookup(eo_wt_sq, voice_phase);
                break;
            case V_PING:                 /* Chord Dogtor's soft bell: the same, but quieter */
            case V_BELL:                 /* inharmonic partials that die away at different speeds */
                ph2 += inc * 2.76f;
                ph3 += inc * 5.40f;
                if (ph2 >= 1.0f)
                    ph2 -= 1.0f;
                if (ph3 >= 1.0f)
                    ph3 -= 1.0f;
                if (batk < 1.0f)
                    batk += 1.0f / (44100.0f * 0.004f);
                if (batk > 1.0f)
                    batk = 1.0f;
                if (eo_rev) {            /* reversed: no decay, a swell that grows for 0.45 s (and stops when the key lifts) */
                    float g = age * (1.0f / 0.45f);
                    g = g > 1.0f ? 1.0f : g;
                    e1 = e2 = e3 = g * g;
                } else {
                    e1 *= 0.99993f;
                    e2 *= 0.99985f;
                    e3 *= 0.99970f;
                }
                v = 0.55f * batk * (fm_sinf(6.2831853f * voice_phase) * e1 + 0.5f * fm_sinf(6.2831853f * ph2) * e2 +
                             0.3f * fm_sinf(6.2831853f * ph3) * e3);
                break;
            case V_EPIANO: {             /* two-operator FM: a bright tine that settles into a soft tone */
                float m = fm_sinf(6.2831853f * voice_phase);
                e1 *= 0.99989f;
                e2 *= 0.99978f;
                v = 0.8f * e1 * fm_sinf(6.2831853f * voice_phase + 1.6f * e2 * m);
                break;
            }
            case V_PURR: {               /* a low rumble pulsing about 25 times a second */
                float in = eo_lookup(eo_wt_saw, voice_phase), pulse = 0.5f + 0.5f * fm_sinf(6.2831853f * 25.0f * age);
                s1l += 0.12f * (in - s1l);
                v = 0.9f * pulse * pulse * s1l;
                break;
            }
            default: {                   /* V_MEOW, V_CATFX, V_CATSAD: a bright saw through two moving resonances */
                float in = eo_lookup(eo_wt_saw, voice_phase), hp;
                hp = in - s1l - 0.25f * s1b;
                s1b += f1 * hp;
                s1l += f1 * s1b;
                hp = in - s2l - 0.25f * s2b;
                s2b += f2 * hp;
                s2l += f2 * s2b;
                v = 0.30f * mamp * (s1b + 0.8f * s2b);
                break;
            }
            }
        }
        age += 1.0f / 44100.0f;
        {   /* Chord Dogtor: the chord's notes (a soft pad), and the heart monitor's beep (1 kHz, 0.1 s, fading) */
            uint32_t j;
            float cs = 0.0f, bv = 0.0f;
            for (j = 0; j < CHORD_VOICES; j++) {
                eo_clev[j] += (ctgt[j] - eo_clev[j]) * 0.0012f;
                eo_cph[j] += cinc[j] * bend;
                if (eo_cph[j] >= 1.0f)
                    eo_cph[j] -= 1.0f;
                if (eo_clev[j] > 0.00002f)
                    cs += eo_lookup(eo_wt, eo_cph[j]) * eo_clev[j];
            }
            if (beep_age < 0.10f) {
                beep_ph += 1000.0f / 44100.0f;
                if (beep_ph >= 1.0f)
                    beep_ph -= 1.0f;
                bv = fm_sinf(6.2831853f * beep_ph) * (1.0f - beep_age * 10.0f) * BEEP_GAIN;
                beep_age += 1.0f / 44100.0f;
            }
            {   /* the voice (the ostinato's send only) through the reverb, then the chord and the beep, dry */
                float vo = v * voice_level, rin = eo_soft_env ? vo : 0.0f, wet = 0.0f;
                for (j = 0; j < 3u; j++) {
                    float y = eo_rv[j][eo_rv_i[j]];
                    eo_rv_lp[j] += 0.45f * (y - eo_rv_lp[j]);        /* each pass is a little darker */
                    eo_rv[j][eo_rv_i[j]] = rin + eo_rv_lp[j] * 0.83f;
                    if (++eo_rv_i[j] >= eo_rv_len[j])
                        eo_rv_i[j] = 0;
                    wet += y;
                }
                float bl = 0.0f, bs = 0.0f;
                if (bass_age < 0.30f) {          /* a plucked bass: a saw whose brightness closes as it decays, over a sub sine; 4 ms in, then a fall */
                    float env = (bass_age < 0.004f ? bass_age * 250.0f : 1.0f) * (1.0f - bass_age * (1.0f / 0.30f)) * (1.0f - bass_age * (1.0f / 0.30f));
                    float sw = eo_lookup(eo_wt_saw, bass_ph);
                    bass_ph += bass_inc;
                    if (bass_ph >= 1.0f)
                        bass_ph -= 1.0f;
                    {
                        float k = 0.30f - 1.6f * bass_age;                         /* the filter closes as the note decays */
                        bass_lp += (k < 0.03f ? 0.03f : k) * (sw - bass_lp);
                    }
                    bs = (bass_lp * 0.9f + fm_sinf(6.2831853f * bass_ph) * 0.55f) * env * bass_v * BASS_GAIN;
                    bass_age += 1.0f / 44100.0f;
                }
                if (blub_age < 0.07f) {          /* a bubble: a quick sine chirping up */
                    blub_ph += blub_inc * (1.0f + 0.9f * blub_age * (1.0f / 0.07f));
                    if (blub_ph >= 1.0f)
                        blub_ph -= 1.0f;
                    bl = fm_sinf(6.2831853f * blub_ph) * fm_sinf(3.1415927f * blub_age * (1.0f / 0.07f)) * BLUB_GAIN;
                    blub_age += 1.0f / 44100.0f;
                }
                v = vo * bend + cs * bend + bv + bl + bs + wet * 0.45f;
            }
        }
        if (smp_run) {                   /* 8 kHz to 44.1 kHz: a straight line between two neighbours */
            uint32_t k = (uint32_t)smp_pos;
            if (k + 1u >= EO_INTRO_LEN) {
                smp_run = 0;
            } else {
                float a0 = eo_ulaw[EO_INTRO[k]];
                v += (a0 + (eo_ulaw[EO_INTRO[k + 1u]] - a0) * (smp_pos - (float)k)) * SMP_GAIN * smp_g;
                smp_pos += EO_INTRO_RATE / 44100.0f;
                if (smp_stop) {
                    smp_g -= 0.002f;
                    if (smp_g <= 0.0f)
                        smp_run = 0;
                }
            }
        }
        s = (v + eo_lookup(eo_wt_soft, eo_drone_phase) * eo_drone_level * bend) * gain_now;
        s = eo_underwater(s);
        lr[2 * i] = (int32_t)s;
        lr[2 * i + 1] = (int32_t)s;
    }
    eo_smp_run = smp_run;
    eo_bend_v = bend;
}

/* ------------------------------------------------------------ the sequencer --- */
/* a timeline of notes on the 1 kHz clock: the main loop fills it (while seq_on is 0) and starts it; app_tick plays it
 * and says when it is done. A stimulus, a chime and the answer all go through it. For Copycat's introduction the tick
 * also sets the drone's level: loud while the root is alone (the first seq_root_ms), softer under the melody. */
#define SEQ_MAX 24u
static uint16_t seq_at[SEQ_MAX], seq_len[SEQ_MAX];    /* each note: when it starts and how long it sounds, ms */
static uint8_t seq_key[SEQ_MAX];                      /* and its key */
static uint8_t seq_vo[SEQ_MAX], seq_vo_stage[SEQ_MAX];   /* each note's own voice (0xFF: the chosen one); the stage is filled first */
static uint8_t seq_mute[SEQ_MAX];                     /* and whether it is silent (Simone: only when sim_mute is on) */
static uint8_t sim_mute;                              /* 1: this round every note is silent; 2: one of them is (use your eyes) */
static uint8_t seq_hl[SEQ_MAX];                       /* and whether it lights, when only some notes do */
static uint32_t seq_n, seq_end;
static volatile uint32_t seq_pos;        /* ms since the start */
static volatile uint32_t seq_lit;        /* the key sounding now, as a bit (0 when this sequence is not shown) */
static volatile uint32_t seq_root_ms;    /* 0, or: the drone is loud for this long, then soft */
static volatile uint8_t seq_on, seq_done, seq_show;

void app_tick(void)
{
    uint32_t i, p;
    int32_t act = -1;
    if (!seq_on)
        return;
    p = seq_pos++;
    if (seq_root_ms)
        eo_drone_vol = p < seq_root_ms ? 1.0f : 0.5f;
    if (p >= seq_end) {
        voice_note = -1;
        voice_fx = 0xFFu;
        seq_lit = 0;
        seq_on = 0;
        seq_done = 1;
        return;
    }
    for (i = 0; i < seq_n; i++)
        if (p >= seq_at[i] && p < (uint32_t)seq_at[i] + seq_len[i])
            act = (int32_t)i;
    if (act >= 0) {
        voice_vel = 0.9f;
        voice_fx = seq_vo[act];
        voice_note = (set.game == GAME_SIMON && sim_mute && seq_mute[act]) ? -1 : KEY_NOTE0 + seq_key[act] + (set.game == GAME_SIMON && seq_key[act] <= 26u ? 12 : 0);   /* her bell sounds an octave up: easier to hear; a silent note only lights */
        seq_lit = (seq_key[act] <= 26u && seq_vo[act] == 0xFFu && (seq_show == 1u || (seq_show == 2u && seq_hl[act]))) ? 1u << seq_key[act] : 0u;
    } else {
        voice_note = -1;
        seq_lit = 0;
    }
}

static void eo_seq_start(uint32_t root_ms, uint32_t show)  /* last, after the arrays are filled */
{
    uint32_t i;
    for (i = 0; i < SEQ_MAX; i++) {      /* the notes' own voices, once; the next sequence starts from the chosen one */
        seq_vo[i] = seq_vo_stage[i];
        seq_vo_stage[i] = 0xFFu;
    }
    voice_fx = 0xFFu;
    voice_note = -1;
    seq_root_ms = root_ms;
    seq_show = (uint8_t)show;
    seq_pos = 0;
    seq_done = 0;
    seq_on = 1;
}

static void eo_drone_set(int32_t note, float vol)    /* note -1: off */
{
    eo_drone_vol = vol;
    eo_drone_note = note;
}

void app_panic(void)                     /* silence now: a sequence stopped, both voices released */
{
    seq_on = 0;
    seq_lit = 0;
    voice_fx = 0xFFu;
    voice_note = -1;
    eo_drone_note = -1;
    eo_water = 0;
    voice_level = 0.0f;
    eo_drone_level = 0.0f;
    { uint32_t j; for (j = 0; j < CHORD_VOICES; j++) { eo_chord_note[j] = -1; eo_clev[j] = 0.0f; } }
}

/* ----------------------------------------------------------------- the game --- */
enum { ST_READY, ST_LISTEN, ST_TURN, ST_WIN, ST_MISS, ST_GAP, ST_LEVELUP, ST_INTRO, ST_CONTROLS, ST_MOVIE, ST_SELECT, ST_SIMONE, ST_WISH };
#define MV_PRES_MS 1700u                 /* the movie: ASilagy presents (up 0.5 s, still 0.9 s, a block dissolve 0.3 s) */
#define MV_MUSIC_MS 1800u                /* then a beat of black, and the music starts: the montage */
#define MV_ROSTER 5600u                  /* (in the music) the montage ends and the whole cast builds up, one by one */
#define MV_FINALE 7750u                  /* (in the music) the last chord stab: the title lands */
static uint32_t mv_t0, mv_m0;            /* the movie's start, and the music's */
static uint8_t mv_stage, eo_cat_seen, eo_sim_seen, sel_cur;   /* its sound cues done; the cat has introduced itself; the select's cursor */
static void mv_end(void);                /* the power-on movie and the game select (below the screen code) */
static void sel_move(int32_t d);
static void sel_blip(uint32_t key, uint8_t vo);
static void sim_end(void);
static void sim_key(uint32_t now);
static void sim_enter_up(void);
static void sel_go(void);
static void sel_enter(void);
enum { TX_READY, TX_ROOT, TX_LISTEN, TX_WATCH, TX_TURN, TX_NICE, TX_MISS, TX_OVER, TX_GOOD };
static const char *const TX[9] = {"READY", "ROOT", "LISTEN", "WATCH", "YOUR TURN", "NICE!", "MISS", "GAME OVER",
                                  "GOOD JOB!"};

#define EO_MAX_LEVELS 34u
typedef char eo_levels_fit[COPYCAT_LEVELS <= EO_MAX_LEVELS && SIMON_LEVELS <= EO_MAX_LEVELS ? 1 : -1];
/* what is different at a level: the three lines of the GOOD JOB card, by game and by the new level (2 and up) */
static const char *const eo_up_text[GAMES][EO_MAX_LEVELS + 1u][3] = {
    {   /* Simon */
        {0, 0, 0},
        {0, 0, 0},
        {"EVERYTHING GOES SLEEPY", "THE SCREEN RIPPLES, THE NOTES DRIFT", "NO HARDER, JUST DREAMIER"},
        {"A CRYSTAL BALL APPEARS", "BLUE SCREEN, THE WORDS ONLY", "SLEEPY MAY RETURN"},
        {"SHE LOSES HER PLACE", "WAIT FOR HER, THEN CARRY ON", "THE BALL AND THE SLEEP STILL COME"},
        {"SOME NOTES GO SILENT", "USE YOUR EYES: READ THE WORDS", "NOTHING ELSE CHANGES"},
        {"SHE SPEAKS IN RIDDLES", "THE RULE SAYS HOW TO PLAY IT BACK", "FIRST RIDDLE: EACH NOTE TWICE"},
        {"THE LIGHTS GO OUT", "THE OLDER NOTES SOUND IN THE DARK", "NEW RIDDLE: THE SECOND NOTE THRICE"},
        {"THE FINALE", "EVERY TRICK, ANY ROUND", "DO TOO WELL AND YOU PLAY BACKWARDS"},
    },
    {   /* Copycat: the new idea, a cat pun, then a word of cheer or a warning */
        {0, 0, 0},
        {0, 0, 0},
        {"THE LIGHTS ARE OFF NOW", "USE YOUR EARS, NOT YOUR EYES", "CATS SEE IN THE DARK. SO CAN YOU"},
        {"TWO NEW NOTES: FA AND SOL", "THE LIGHTS ARE BACK FOR THEM", "ONE NEW NOTE IN EVERY MELODY"},
        {"THE SAME NOTES, NO LIGHTS", "USE YOUR EARS, KITTEN", "FIVE NOTES: DO TO SOL"},
        {"FOUR NOTES IN A MELODY", "ONE MORE TO CATCH. NO FUR-GETTING", "STILL ONLY STEPS"},
        {"THIRDS JOIN THE STEPS", "SKIPPING A NOTE, CAT-WALK STYLE", "FOUR NOTES, STEPS AND THIRDS"},
        {"A NEW NOTE: LA, LIT UP", "THE LIGHTS ARE BACK. PURR-FECT", "LA IS IN EVERY MELODY"},
        {"LA, NOW BY EAR", "LIGHTS OFF. CLAWS OUT!", "SHORT MELODIES AGAIN"},
        {"SIX NOTES, FOUR IN A MELODY", "YOU'RE ON THE PROWL NOW", "STEPS AND THIRDS"},
        {"THE HIGH NOTES: TI AND DO", "LIGHTS ON TO MEET THEM", "A HIGH NOTE IN EVERY MELODY"},
        {"THE WHOLE OCTAVE, BY EAR", "LIGHTS OFF. EARS UP!", "THREE NOTES. NO RUSH"},
        {"FOUR NOTES OF THE OCTAVE", "YOU ARE PURR-FORMING WELL", "LISTEN CLOSELY"},
        {"SEMITONES: C TO G, LIT UP", "HALF STEPS. WATCH THE LIGHT", "SHORT AGAIN: THREE NOTES"},
        {"SEMITONES, NO LIGHTS", "HALF STEPS BY EAR. FELINE FINE?", "LISTEN FOR THE TINY STEPS"},
        {"LEAPS! BIG JUMPS ARE HERE", "MEOW! THIS IS GETTING HARDER", "A STEP FILLS THE GAP AFTER"},
        {"FOUR NOTES WITH A LEAP", "A BIT LONGER. STAY CALM", "CAT-APULT THROUGH IT"},
        {"LEAP TO ANY NOTE, NO SHAPE", "THE CONTOUR IS GONE. NO PEEKING!", "LISTEN FOR THE SHAPE YOURSELF"},
        {"FIVE NOTES, UP TO TWO LEAPS", "HOLD ON TO YOUR WHISKERS", "NINE LIVES, NO RUSH"},
        {"SEMITONES AND A LEAP", "TWO TRICKS IN ONE. YIKES, CAT", "FOUR NOTES ONLY"},
        {"PASSING NOTES, LIT UP", "CHROMATIC NOTES ACROSS THE OCTAVE", "ONE SEMITONE SNEAKS IN"},
        {"CHROMATIC, NO LIGHTS", "SNEAKY NOTES. TRUST YOUR EARS", "CAT BURGLAR MODE: ON"},
        {"NO DRONE. START ON THE ROOT", "NO MORE PURRING UNDER THE TUNE", "THE MELODY BEGINS ON THE ROOT"},
        {"A LONGER MELODY, NO DRONE", "FIVE NOTES. STAY ON THE ROOT", "YOU ARE CLAW-SOME"},
        {"IT NO LONGER STARTS ON THE ROOT", "FIND YOUR OWN WAY HOME", "TRUST YOUR EARS"},
        {"NOTES BELOW THE ROOT, LIT UP", "LOOK DOWN, KITTEN. LIGHTS ON", "A LOW NOTE IN EVERY MELODY"},
        {"BELOW THE ROOT, BY EAR", "LIGHTS OFF. LOOK OUT BELOW!", "FIVE NOTES AGAIN"},
        {"SIX NOTES, WIDE RANGE", "LONG TAILS, LONG MELODIES", "STAY CALM. KEEP LISTENING"},
        {"CHROMATIC LEAPS, LIT UP", "SEMITONE POUNCES!", "UP TO TWO IN A MELODY"},
        {"THE ROOT MOVES: C, G OR D", "A NEW KEY EVERY ROUND. SCAT, CAT!", "THE MELODY STARTS ON THE ROOT"},
        {"FIVE NOTES IN THREE KEYS", "THE ROOT STILL SOUNDS FIRST", "YOU CAN PURR-FORM THIS"},
        {"TWO MORE ROOTS: A AND E", "FIVE KEYS. FELINE FOCUSED", "STILL STARTS ON THE ROOT"},
        {"ALL SEVEN ROOTS: F# AND C#", "NINE LIVES NEEDED", "THE START IS FREE NOW"},
        {"CHROMATIC NOTES IN EVERY KEY", "LEAPS TOO, LIT UP FOR YOU", "PHEW. ALMOST THERE"},
        {"BOSS LEVEL: ANY NOTE, ANY ROOT", "SIX NOTES, UP TO TWO LEAPS", "NO LIGHTS. NO ANCHOR. NO FEAR"},
    },
    {{0, 0, 0}}   /* Chord Dogtor draws its own card */
};

/* the ready screen's level name, and the notes to choose from (Copycat; index 0 is unused) */
static const char *const eo_cc_name[COPYCAT_LEVELS + 1u] = {
    "", "Do re mi, with lights", "Lights off", "Fa and sol arrive", "Same notes, no lights", "Longer tunes",
    "Thirds join in", "La arrives", "La, by ear", "Longer again", "Ti and high do", "The whole octave, by ear",
    "Longer octave tunes", "First semitones", "Semitones, by ear", "Leaps arrive", "Longer leaps", "Leap anywhere",
    "Up to two leaps", "Semitones and a leap", "Passing tones, lit", "Passing tones, by ear", "No drone",
    "No drone, longer", "Start anywhere", "Below the root", "Below, by ear", "Long and low", "Chromatic leaps",
    "The root moves", "Moving, longer", "Two more roots", "All seven roots", "Chromatic in every key",
    "Boss: anything goes"};
static const char *eo_cc_pool(uint32_t n)
{
    switch (n) {
    case 3: return "3 notes: do re mi";
    case 5: return "5 notes: do to sol";
    case 6: return "6 notes: do to la";
    case 8: return "The whole octave (8 notes)";
    case 11: return "11 notes: a fifth below to the octave";
    case 13: return "The chromatic octave (13 notes)";
    default: return "18 notes: low to high, chromatic";
    }
}

static struct {
    char msg[2][24];                     /* a short message over the header */
    uint32_t msg_until;
    uint32_t keys, btn;                  /* held at the last input */
    uint8_t state, text;                 /* the game's state, and the big word on the screen */
    uint8_t redraw;
    uint8_t redraw_cat;                  /* only the cat's picture changed (an animation frame) */
    uint8_t result;                      /* Copycat: 0 none yet, 1 the last round was right, 2 it was a miss */
    uint32_t up_until;                   /* Copycat: the level-up celebration runs until then, before the card */
    uint32_t vol_until;                  /* the volume is shown on the screen until then (after a change) */
    uint8_t ctl_page;                    /* the controls screens: 0 to 2 */
    uint8_t kb_show;                     /* Copycat, lights off: after a miss the keyboard slides in, with the answer lit */
    uint32_t kb_at;                      /* when it started to slide in */
    char say[48];                        /* what the cat says (a line under it), and until when */
    uint32_t say_until;
} ui;
static uint8_t ans[SIMON_MAX_LEN];       /* this round's melody (Copycat) or the sequence so far (Simon), as keys */
static uint32_t ans_len, ans_pos;        /* its length, and how many notes you have played right */
static uint32_t streak;                  /* Copycat: melodies right in a row */
static uint32_t eo_score;                /* Simon: the last game's score */
static uint32_t gap_until;               /* the pause before the next round */
static uint32_t eo_seed;                 /* the melodies' random state (0: not seeded yet) */
static uint8_t save_pending;             /* a new best, to be kept in the next quiet moment */
static uint8_t eo_win_pending;           /* a round is won: its sound waits for you to let go of the last key */
static uint8_t eo_levelup;               /* and this win graduates you: the GOOD JOB card comes after the sound */
static uint32_t eo_win_at, eo_win_due;   /* when it was won; when the sound may start (you let go, plus a beat) */

static void text_cat(char *d, const char *s)
{
    while (*d)
        d++;
    while (*s)
        *d++ = *s++;
    *d = 0;
}

static int eo_streq(const char *a, const char *b)
{
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}

static void eo_say(const char *s, uint32_t ms)   /* the cat says something, for a while (Copycat's round screen) */
{
    uint32_t i;
    for (i = 0; s[i] && i < sizeof ui.say - 1u; i++)
        ui.say[i] = s[i];
    ui.say[i] = 0;
    ui.say_until = plat_ms() + ms;
}

static void int_text(int32_t v, char *b)     /* signed, no padding */
{
    char t[12];
    int n = 0;
    uint32_t u = (uint32_t)(v < 0 ? -v : v);
    do
        t[n++] = (char)('0' + u % 10u);
    while ((u /= 10u) && n < 11);
    if (v < 0)
        *b++ = '-';
    while (n)
        *b++ = t[--n];
    *b = 0;
}

static void note_name(int32_t note, char *b)
{
    static const char *const N[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    char o[12];
    b[0] = 0;
    text_cat(b, N[note % 12]);
    int_text(note / 12 - 1, o);
    text_cat(b, o);
}

void app_say(const char *a, const char *b)
{
    uint32_t i;
    char t[48];
    for (i = 0; i < 23u && a && a[i]; i++)
        ui.msg[0][i] = a[i];
    ui.msg[0][i] = 0;
    for (i = 0; i < 23u && b && b[i]; i++)
        ui.msg[1][i] = b[i];
    ui.msg[1][i] = 0;
    ui.msg_until = plat_ms() + 1300u;
    ui.redraw = 1;
    if (ann_on()) {                          /* and to a screen reader (announce.c, fm1-reader) */
        t[0] = 0;
        text_cat(t, ui.msg[0]);
        if (ui.msg[1][0]) {
            text_cat(t, " ");
            text_cat(t, ui.msg[1]);
        }
        ann_say(t, 0, ANN_NOW);
    }
}

static int eo_root_phase(void)           /* Copycat is introducing its root: nothing but the root is sounding yet */
{
    return ui.state == ST_LISTEN && seq_on && seq_root_ms && seq_pos < seq_root_ms;
}

static uint32_t eo_text(void)            /* the big word now: ROOT is the first part of Copycat's LISTEN */
{
    return eo_root_phase() ? TX_ROOT : ui.text;
}

static uint8_t dt_key_now;               /* Chord Dogtor: the right answer's key, for the lights */
static void dogtor_win_sound(void);
static uint32_t eo_level(void) { return set.level[set.game]; }
static uint32_t eo_rounds(uint32_t g)    /* melodies (Simon: sequence length) to play right to graduate the level now */
{
    return g == GAME_COPYCAT ? copycat_level(set.level[g])->rounds : g == GAME_DOGTOR ? DOGTOR_ROUNDS : simon_goal(set.level[g]);
}
static uint32_t eo_miss_cost(void)       /* a miss costs a quarter of the way: real progress, but not a reset */
{
    return eo_rounds(GAME_COPYCAT) / 4u;
}

static int32_t eo_step_pos = -1;         /* the melody stepper: the note last heard, -1 before the first */
static uint32_t eo_tries;                /* practice: which try at this melody */
static uint32_t eo_miss_run;             /* misses since the last right melody: past a few, the cat gives advice */
static uint32_t eo_say_ix, eo_tip_ix, eo_miss_ix, eo_nice_ix, eo_word_ix;
#define EO_TIP_AFTER 3u                  /* misses in a row before the cat starts giving advice */
#define EO_ASSIST_AFTER 3u               /* practice: wrong tries at one melody before the cat turns the lights on */
static const char *const eo_miss_word[7] = {"MISS", "UMM...", "WHOOPS.", "HISSS!", "WRONG!", "NYAAAHH!", "INCORRECT"};
static const char *const eo_nice_word[5] = {"NICE!", "WOW!", "SWEET!", "AMAZING!", "YUP."};
static const char *const eo_miss_say_dark[3] = {"It was THESE. Obviously.", "THESE. Not those. Sigh.",
                                                "Not even close. Kidding."};
static const char *const eo_miss_say_lit[3] = {"...really?", "The lights were ON.", "Look at the lights!"};
static const char *const eo_tips[6] = {"Not sure? Press PLAY first, then try.", "Turn SELECT: my song, your pace.",
                                       "Knob 3 slows my song. Easier, right?", "Practice mode is no shame. Knob 4.",
                                       "Be patient. Think before you act!", "Try singing the notes first. Meow."};
static const char *eo_word(uint32_t tx)  /* the big word: a miss and a win each have a few, taken in turn */
{
    if (tx == TX_MISS)
        return eo_miss_word[eo_word_ix % 7u];
    if (tx == TX_NICE)
        return eo_nice_word[eo_word_ix % 5u];
    return TX[tx];
}
static uint8_t eo_cc_root;               /* Copycat: this round's root key (the chosen one, unless the level moves it) */
static uint8_t eo_moves(void)            /* the level moves the root along the cycle of fifths */
{
    return set.game == GAME_COPYCAT && copycat_level(eo_level())->roots > 1u;
}
static uint32_t eo_croot(void)           /* the root now: before a round, on a moving level, there is none */
{
    return eo_moves() ? eo_cc_root : set.root;
}

static uint32_t eo_open(uint32_t g)      /* the highest level you may play: three above the highest completed */
{
    uint32_t m = set.top[g] + EO_UNLOCK;
    if (set.pad & 1u)                    /* debug mode: every level is open */
        return eo_levels[g];
    return m > eo_levels[g] ? eo_levels[g] : m;
}

#define SPEED_MIN 5u                     /* 25% */
#define SPEED_MAX 40u                    /* 200% */
#define SPEED_DEF 20u                    /* 100% */
static uint32_t eo_spd(uint32_t ms)      /* a time in the melody, at the chosen speed */
{
    return ms * 20u / (set.speed < SPEED_MIN || set.speed > SPEED_MAX ? SPEED_DEF : set.speed);   /* speed is percent / 5 */
}

static void eo_complete(uint32_t g)      /* ten rounds right at the current level: it counts as completed */
{
    if (set.mode)                            /* practice counts for nothing */
        return;
    if (set.level[g] > set.top[g]) {
        set.top[g] = set.level[g];
        set_dirty = 1;
    }
}


/* a screen reader that has just started listening (or asks) hears the whole screen */
static void speak(void)
{
    char t[200], n[12];
    if (ann_on() && ann_where() && ui.state == ST_LEVELUP && plat_ms() >= ui.up_until) {     /* the GOOD JOB card, in words */
        const char *const *ln = eo_up_text[set.game][eo_level()];
        uint32_t i;
        t[0] = 0;
        text_cat(t, "GOOD JOB. LEVEL ");
        int_text((int32_t)eo_level(), n);
        text_cat(t, n);
        for (i = 0; i < 3u && ln[i]; i++) {
            text_cat(t, ". ");
            text_cat(t, ln[i]);
        }
        text_cat(t, ". PLAY TO CONTINUE");
        ann_say(t, 0, ANN_NOW);
    } else if (ann_on() && ann_where()) {
        t[0] = 0;
        text_cat(t, eo_game_name[set.game]);
        text_cat(t, ". LEVEL ");
        int_text((int32_t)eo_level(), n);
        text_cat(t, n);
        if (set.game == GAME_COPYCAT) {
            note_name(KEY_NOTE0 + (int32_t)eo_croot(), n);
            text_cat(t, ". ROOT ");
            text_cat(t, n);
        }
        text_cat(t, ". ");
        text_cat(t, TX[eo_text()]);
        ann_say(t, 0, ANN_NOW);
    }
    ann_tick(plat_ms());
}

/* the three LED levels, for the keys and for the screen's keyboard alike */
static void key_levels(uint32_t *bright, uint32_t *mid, uint32_t *dim)
{
    uint32_t b = seq_on ? seq_lit : ui.keys;
    if (set.game == GAME_DOGTOR) {                  /* the answer keys: dim while you choose; the right one bright after */
        *mid = 0u;
        *dim = ui.state == ST_READY || ui.state == ST_LEVELUP ? 0u : dogtor_key_mask(eo_level());
        if (ui.state == ST_MISS || ui.state == ST_WIN)
            b = 1u << dt_key_now;
        *bright = b;
        return;
    }
    if (set.game == GAME_COPYCAT) {
        const copycat_level_t *lv = copycat_level(eo_level());
        if (eo_moves() && (ui.state == ST_READY || ui.state == ST_LEVELUP)) {
            *mid = 0u;                       /* the root moves: there is none until a round starts */
            *dim = 0u;
        } else {
            *mid = 1u << eo_croot();         /* the root, always, steady... */
            *dim = lv->hint ? copycat_pool_mask(eo_level(), eo_croot()) : 0u;
            if (eo_root_phase())
                b |= *mid;                   /* ...and bright while it is being introduced */
        }
    } else {
        *mid = 0u;                           /* Simon has no root */
        *dim = simon_pool_mask(eo_level(), SIMON_BASE);
    }
    *bright = b;                             /* a bright bit wins over the others */
}

/* the power-on movie's lights: once the music starts, a stadium wave rolls along the keys, left to right and round again,
 * a bright crest with a half-lit swell ahead of it and a dim wake behind */
#define MV_WAVE_AT 5720u
#define MV_STAB1 7480u                   /* the last two chord stabs, in ms of music */
#define MV_STAB2 7800u
static uint32_t mv_wave_t0;              /* when the music began (0: not yet) */
static void movie_wave(uint32_t *bright, uint32_t *mid, uint32_t *dim)
{
    uint32_t t, head, k;
    if (!mv_wave_t0 || plat_ms() < mv_wave_t0)
        return;
    t = plat_ms() - mv_wave_t0;
    if (t + MV_WAVE_AT >= MV_STAB1 && t + MV_WAVE_AT < MV_STAB1 + 220u) {      /* the two last chord stabs: every key lights */
        *bright = (1u << NKEYS) - 1u;
        return;
    }
    if (t + MV_WAVE_AT >= MV_STAB2 && t + MV_WAVE_AT < MV_STAB2 + 220u) {
        *bright = (1u << NKEYS) - 1u;
        return;
    }
    if (t + MV_WAVE_AT >= MV_STAB2 + 220u && t + MV_WAVE_AT < MV_STAB2 + 520u) {   /* and the second one rings out, dimmer */
        *mid = (1u << NKEYS) - 1u;
        return;
    }
    head = t / 55u;                       /* one key every 55 ms: the sweep crosses the keys once, in about a second and a half */
    if (head >= NKEYS + 4u)
        return;
    for (k = 0; k < NKEYS; k++) {
        int32_t d = (int32_t)head - (int32_t)k;      /* 0: the crest; positive: behind it */
        if (d == 0)
            *bright |= 1u << k;
        else if (d == 1 || d == -1)
            *mid |= 1u << k;
        else if (d == 2 || d == 3 || d == -2)
            *dim |= 1u << k;
    }
}

static uint32_t sim_in_preroll(void);
static void sim_dazzle_leds(uint32_t *bright, uint32_t *mid, uint32_t *dim);

static void lights(void)
{
    uint32_t bright, mid, dim, btn = ui.btn;
    key_levels(&bright, &mid, &dim);
    if (ui.state == ST_MOVIE || ui.state == ST_SELECT || ui.state == ST_SIMONE)
        bright = mid = dim = 0u;     /* no game yet: the keys stay dark */
    if (ui.state == ST_MOVIE)
        movie_wave(&bright, &mid, &dim);
    if (set.game == GAME_SIMON && sim_in_preroll())
        sim_dazzle_leds(&bright, &mid, &dim);
    if (set_dirty)
        btn |= 1u << B_SAVE;
    if (ui.state == ST_SELECT || ui.state == ST_SIMONE)
        btn |= 1u << B_PLAY;             /* steady: press it (not following the sequencer's notes) */
    else if (seq_on || (set.game == GAME_SIMON && ui.state == ST_READY))
        btn |= 1u << B_PLAY;             /* (Simone's game over: PLAY glows, she says it too) */
    plat_lights(btn, bright, (1u << NB) - 1u, dim, GLOW);   /* every button glows a little: readable in the dark */
    plat_lights_mid(0, mid);
}

static int store_now(void)               /* the settings to flash; not while a melody is sounding (a write holds the sound) */
{
    if (plat_store_save(OBJ_SETTINGS, &set, sizeof set) != 0)
        return -1;
    set_dirty = 0;
    return 0;
}

static void save_button(void)
{
    if (store_now() == 0)
        app_say("SAVED", "");
    else
        app_say("SAVE", "FAILED");
}

static void eo_seed_once(void)
{
    if (!eo_seed)
        eo_seed = (plat_ms() * 2654435761u) | 1u;      /* seeded by when you first press PLAY: different every time */
}

/* the success sound is always the cat: two happy meows, high, above the top key, so they cannot be mistaken for a note of
 * the melody, which you could play. Not lit. (Simon's is one plain blip.) */
static void play_chime(void)
{
    seq_at[0] = 0;
    seq_len[0] = 320;
    seq_key[0] = (uint8_t)(eo_croot() + 19u);
    seq_vo_stage[0] = V_CATFX;
    seq_at[1] = 400;
    seq_len[1] = 520;
    seq_key[1] = (uint8_t)(eo_croot() + 24u);
    seq_vo_stage[1] = V_CATFX;
    seq_n = 2;
    seq_end = 960;
    eo_seq_start(0, 0);
}

static void play_levelup_song(void)      /* the cat's song: happy meows climbing, a long one, then a purr */
{
    static const uint16_t AT[7] = {0, 260, 520, 780, 1150, 1350, 2050};
    static const uint16_t LEN[7] = {220, 220, 220, 300, 150, 620, 1050};
    static const uint8_t UP[6] = {12, 16, 19, 24, 19, 24};
    uint32_t i;
    for (i = 0; i < 7u; i++) {
        seq_at[i] = AT[i];
        seq_len[i] = LEN[i];
        seq_key[i] = i < 6u ? (uint8_t)(eo_croot() + UP[i]) : 0u;
        seq_vo_stage[i] = i < 6u ? V_CATFX : V_PURR;
    }
    seq_n = 7;
    seq_end = 3150;
    eo_drone_set(-1, 0.0f);
    eo_seq_start(0, 0);
}

static void eo_play_blip(void)
{
    seq_at[0] = 0;
    seq_len[0] = 220;
    seq_key[0] = (uint8_t)(SIMON_BASE + 24u);
    seq_n = 1;
    seq_end = 280;
    eo_seq_start(0, 0);
}

static void eo_new_best(uint32_t score)
{
    if (set.mode)
        return;
    if (score > set.best[set.game]) {
        set.best[set.game] = (uint16_t)score;
        set_dirty = 1;
        save_pending = 1;
    }
}

/* ---- Copycat */
static int eo_assist(void)               /* practice, many wrong tries at no-lights level: the cat lights the keys */
{
    return set.mode && eo_tries > EO_ASSIST_AFTER && copycat_level(eo_level())->lit != LIT_ALL;
}

static void copycat_play(uint32_t reps)  /* the root alone, then the melody over it, `reps` times (PLAY replays it once) */
{
    const copycat_level_t *lv = copycat_level(eo_level());
    char n[8];
    uint32_t i;
    uint32_t r, pass = eo_spd(600u) * ans_len + 900u;    /* one hearing, and the pause before the next */
    if (reps < 1u || reps * ans_len > SEQ_MAX)
        reps = 1u;
    for (r = 0; r < reps; r++)
        for (i = 0; i < ans_len; i++) {
            uint32_t k = r * ans_len + i;
            seq_at[k] = (uint16_t)(ROOT_MS + pass * r + eo_spd(600u) * i);
            seq_len[k] = (uint16_t)eo_spd(450u);
            seq_key[k] = ans[i];
            seq_hl[k] = (uint8_t)eo_chromatic((int32_t)ans[i] - (int32_t)eo_croot());
        }
    seq_n = reps * ans_len;
    seq_end = ROOT_MS + pass * (reps - 1u) + eo_spd(600u) * (ans_len - 1u) + eo_spd(450u);
    eo_drone_set(KEY_NOTE0 + (int32_t)eo_croot(), 1.0f);
    eo_seq_start(ROOT_MS, eo_assist() ? LIT_ALL : lv->lit);
    ui.state = ST_LISTEN;
    ui.text = TX_LISTEN;
    ans_pos = 0;
    eo_step_pos = -1;
    note_name(KEY_NOTE0 + (int32_t)eo_croot(), n);
    app_say("ROOT", n);
}

static void copycat_round(void)
{
    int8_t off[COPYCAT_MAX_LEN];
    uint32_t i;
    eo_seed_once();
    ui.result = 0;
    eo_tries = 1;
    ui.kb_show = 0;                      /* the keyboard hides again, and the cat is quiet, as a round starts */
    ui.say[0] = 0;
    eo_cc_root = (uint8_t)copycat_pick_root(eo_level(), set.root, &eo_seed, eo_cc_root);
    ans_len = copycat_generate(eo_level(), eo_croot(), &eo_seed, off);
    for (i = 0; i < ans_len; i++)
        ans[i] = (uint8_t)((int32_t)eo_croot() + off[i]);
    copycat_play(copycat_level(eo_level())->hear);
}

static void eo_win_begin(void)           /* NICE! at once; the success sound once you let go of the last key */
{
    ui.state = ST_WIN;
    ui.text = TX_NICE;
    eo_nice_ix = (eo_nice_ix + 2u) % 5u;
    eo_word_ix = eo_nice_ix;
    eo_win_pending = 1;
    eo_win_at = plat_ms();
    eo_win_due = eo_win_at + 250u;
}

static void eo_win_sound(void)
{
    eo_win_pending = 0;
    if (set.game == GAME_DOGTOR) {
        dogtor_win_sound();
    } else if (set.game == GAME_COPYCAT) {
        eo_drone_set(-1, 0.0f);              /* the root stops as the ding comes */
        play_chime();
    } else {
        eo_play_blip();
    }
}

static void copycat_win(void)
{
    ui.result = 1;
    if (set.mode) {                          /* practice: nothing is counted, and nothing is saved */
        eo_miss_run = 0;
        eo_levelup = 0;
        eo_win_begin();
        return;
    }
    streak++;
    eo_miss_run = 0;
    if (streak % 3u == 0u)               /* the cat comments on a run */
        eo_say(streak % 9u == 0u ? "Are you cheating?" : streak % 6u == 0u ? "Okay. Impressive." : "Purrfect.", 3000u);
    eo_new_best(streak);
    if (set.prog[GAME_COPYCAT] < eo_rounds(GAME_COPYCAT))
        set.prog[GAME_COPYCAT]++;
    if (set.prog[GAME_COPYCAT] >= eo_rounds(GAME_COPYCAT))
        eo_complete(GAME_COPYCAT);
    set_dirty = 1;
    eo_levelup = set.prog[GAME_COPYCAT] >= eo_rounds(GAME_COPYCAT) && set.level[GAME_COPYCAT] < COPYCAT_LEVELS;
    eo_win_begin();
}

static void copycat_miss(void)           /* the right melody, over its root, lit: so you hear what it was */
{
    uint32_t i;
    ui.result = 2;
    eo_miss_run++;
    eo_say_ix++;
    eo_miss_ix = (eo_miss_ix + 3u) % 7u;
    eo_word_ix = eo_miss_ix;
    if (copycat_level(eo_level())->lit != LIT_ALL) {   /* lights off: the keyboard slides in with the answer lit */
        ui.kb_show = 1;
        ui.kb_at = plat_ms();
    }
    if (eo_miss_run >= EO_TIP_AFTER) {   /* again and again: advice, in turn */
        eo_say(eo_tips[eo_tip_ix % 6u], 6000u);
        eo_tip_ix++;
    } else if (copycat_level(eo_level())->lit != LIT_ALL) {
        eo_say(eo_miss_say_dark[eo_say_ix % 3u], 6000u);
    } else {
        eo_say(eo_miss_say_lit[eo_say_ix % 3u], 3000u);
    }
    if (set.mode) {
        eo_tries++;                          /* practice: the same melody again, from scratch */
    } else {
        streak = 0;
        if (set.prog[GAME_COPYCAT]) {        /* a miss costs a quarter of the rounds of progress toward the next level */
            set.prog[GAME_COPYCAT] = set.prog[GAME_COPYCAT] > eo_miss_cost() ? (uint8_t)(set.prog[GAME_COPYCAT] - eo_miss_cost()) : 0u;
            set_dirty = 1;
            save_pending = 1;                /* kept in the quiet, once the cat has finished */
        }
    }
    seq_at[0] = 0;                       /* the fail sound: the cat's sad yowl, high, above the keys */
    seq_len[0] = 850;
    seq_key[0] = (uint8_t)(eo_croot() + 19u);
    seq_vo_stage[0] = V_CATSAD;
    for (i = 0; i < ans_len; i++) {      /* then the right melody, so you hear what it was */
        seq_at[i + 1u] = (uint16_t)(1100u + eo_spd(600u) * i);
        seq_len[i + 1u] = (uint16_t)eo_spd(450u);
        seq_key[i + 1u] = ans[i];
    }
    seq_n = ans_len + 1u;
    seq_end = 1100u + eo_spd(600u) * (ans_len - 1u) + eo_spd(450u);
    eo_drone_set(KEY_NOTE0 + (int32_t)eo_croot(), 0.5f);
    eo_seq_start(0, 1);
    ui.state = ST_MISS;
    ui.text = TX_MISS;
}

/* ---- Simon */
static void simon_play(void);

static void simon_grow(void)             /* one more note, then the whole sequence, lit as it sounds */
{
    uint32_t prev = ans_len ? ans[ans_len - 1u] - SIMON_BASE : 255u;
    eo_seed_once();
    ans[ans_len++] = (uint8_t)(SIMON_BASE + simon_next(eo_level(), &eo_seed, prev));
    simon_play();
}

/* Simone's performance (levels 2 and up, never in practice): before her sequence a PRE-ROLL, in which she swells toward you
 * and the lights dazzle; and sometimes a LAPSE in the middle, in which she stalls, shrinks and either starts over from the top
 * or picks up where she left off. All of it is announced in her voice and none of it changes the answer. */
#define SIM_GAP 3200u                    /* how long a lapse lasts */
#define SIM_STALL 2600u                  /* how much longer she holds a note when she stalls on it */
static uint8_t sim_wishes = 3;           /* the wishes left this game: hear it again, or try the round again after a miss */
static uint8_t sim_good;                 /* rounds won in a row, without a wish or a miss */
static uint8_t sim_angry;                /* the next round is the angry one: played backwards */
static uint8_t sim_back;                 /* this round is to be played backwards (last note first) */
static uint8_t sim_angry_pre;            /* this round's pre-roll is the spirits' anger */
static uint16_t sim_seen;
static uint8_t sim_cool, sim_pre_first;   /* the tricks shown this game (a bit each); the last round had one; the pre-roll is the first one */
static uint8_t sim_hint;                 /* the next sequence is a wish: everything lit, no tricks */
static uint8_t sim_wish_peek;            /* the wish being granted is a peek in your turn (not a second try) */
static uint32_t sim_turn_t0, sim_wish_t0, sim_wish_until, sim_gap_t0, sim_typed_n;
static uint8_t sim_cameo, sim_cameo_last;   /* this pause has Meow in it; and the last one did */
static uint8_t sim_purr_on;              /* Meow's purr is sounding */
static uint8_t sim_push_snd;             /* the push's hum is on */
static uint32_t sim_pre;                 /* the pre-roll's length in ms (0: none) */
static uint32_t sim_gap_at, sim_gap_len; /* the lapse: when it starts in the sequence's ms, and how long (0: none) */
static uint8_t sim_gap_restart;          /* 1: she starts again from the top, 0: she goes on from where she was */
static uint8_t sim_gap_stall;            /* 1: no gap: she holds one note a long time, thinking */
static uint8_t sim_say_n;                /* counts her rounds: it picks which of her lines she says, so they come round slowly */
static uint8_t sim_dark;                 /* 1: this round the older notes sound in the dark */
static uint8_t sim_distract;             /* the round just played had a trick: she is extra complimentary after it */
static uint8_t sim_twice, sim_rule, sim_rule_seen, sim_exp_n, sim_exp[SEQ_MAX], sim_mute_ix;   /* a riddle round: its rule (1..5), and the keys it takes, in order */                /* 1: this round you play each note twice (the double fortune) */
static uint8_t sim_sleepy;               /* 1: this round is sleepy: gray-blue, rippling, with a long reverb */
static uint8_t sim_crystal;              /* 1: this round is in the crystal ball: a blue screen, bubbles and the words only */
static uint32_t sim_up_t0;               /* when the level-up scene began */
static uint8_t sim_alert;                /* her line right now is one that matters: it gets the big card */
static uint32_t sim_fx_rng = 0x2545F491u;
#define SIM_NOFX 6u
#ifdef PLAT_HOST
static uint8_t sim_fx_force = SIM_NOFX;             /* tests: 1 crystal ball, 2 goes on, 3 starts over, 4 stalls (every round that can), 5 only the pre-roll, 6 none (the default here), 7 none but Meow drops in at every pause, 0 chance and debuts */
#define SIM_FORCE sim_fx_force
#else
#define SIM_FORCE 0u
#endif

static uint32_t sim_fx_rand(void)        /* its own little generator, so the sequence itself stays the same */
{
    sim_fx_rng ^= sim_fx_rng << 13;
    sim_fx_rng ^= sim_fx_rng >> 17;
    sim_fx_rng ^= sim_fx_rng << 5;
    return sim_fx_rng;
}

static uint8_t sim_expect(void)          /* the key you should play next: the sequence's next, or its last when it is backwards */
{
    if (sim_twice)
        return sim_exp[ans_pos];
    return sim_back ? ans[ans_len - 1u - ans_pos] : ans[ans_pos];
}

static void sim_build(void)              /* the keys a riddle round takes, from the sequence and the rule */
{
    uint32_t i, n = 0, l = ans_len;
    for (i = 0; i < l; i++) {
        if (sim_rule == 4u && i == 0u)
            continue;
        if (sim_rule == 3u && i != 0u && i != l - 1u)
            continue;
        sim_exp[n++] = ans[i];
        if (sim_rule == 1u || (sim_rule == 5u && i == l - 1u)) {
            sim_exp[n++] = ans[i];
        } else if (sim_rule == 2u && i == 1u) {
            sim_exp[n++] = ans[i];
            sim_exp[n++] = ans[i];
        }
    }
    sim_exp_n = (uint8_t)n;
}

static uint32_t sim_pick_rule(uint32_t lv, uint32_t len)   /* a rule you may meet now: the first you have not yet, else any */
{
    uint32_t r, ok = 0, k, pick = 0;
    for (r = 1; r <= 5u; r++)
        if (simon_rule_ok(r, len, lv) && !((sim_rule_seen >> r) & 1u))
            return r;
    for (r = 1; r <= 5u; r++)
        ok += simon_rule_ok(r, len, lv);
    if (!ok)
        return 0u;
    k = sim_fx_rand() % ok;
    for (r = 1; r <= 5u; r++)
        if (simon_rule_ok(r, len, lv) && k-- == 0u)
            pick = r;
    return pick;
}

static uint32_t sim_need(void)           /* how many keys this round takes: the notes, or double */
{
    return sim_twice ? sim_exp_n : ans_len;
}

static void simon_play(void)             /* the whole sequence, lit as it sounds (also what a practice miss repeats) */
{
    uint32_t i, step, len_ms, base, k = 0, lapse = 0, n = 0, lv = eo_level(), hint = sim_hint, angry = 0, lit;
    sim_hint = 0;
    if (hint) {
        (void)0;                             /* a wish: everything shown, no tricks */
    } else if (sim_angry && !set.mode && !SIM_FORCE) {
        angry = 1;                           /* the spirits are angry: this round is played backwards */
        sim_angry = 0;
        sim_back = 1;
    } else {
        sim_back = 0;
    }
    sim_angry_pre = (uint8_t)angry;
    lit = 255u;
    step = eo_spd(simon_step_ms(ans_len, lv));
    len_ms = eo_spd(simon_note_ms(simon_step_ms(ans_len, lv)));
    sim_pre = 0u;
    sim_gap_len = 0;
    sim_gap_at = 0;
    sim_gap_restart = 0;
    sim_gap_stall = 0;
    sim_crystal = 0;
    sim_sleepy = 0;
    sim_dark = 0;
    sim_mute = 0;
    if (!hint) {
        sim_twice = 0;
        sim_rule = 0;
        sim_say_n++;
    }
    sim_pre_first = 0;
    {   /* her tricks arrive one at a time: each has a debut round (a short one, forced once a game), then it joins the chance pool */
        uint32_t fx = 0, f = SIM_FORCE;
        if (!hint && !angry && !set.mode && f != SIM_NOFX && f != 7u) {
            if (f == 1u) {
                fx = ans_len >= 2u ? SIMON_FX_CRYSTAL : 0u;
            } else if (f >= 2u && f <= 4u) {
                fx = ans_len >= 3u ? SIMON_FX_LAPSE + (f - 2u) : 0u;
            } else if (f == 15u || f == 16u) {
                fx = ans_len >= 2u ? SIMON_FX_EYES : 0u;
            } else if (f >= 11u && f <= 14u) {
                fx = ans_len >= 2u ? SIMON_FX_TWICE : 0u;
            } else if (f == 10u) {
                fx = ans_len >= 2u ? SIMON_FX_DARK : 0u;
            } else if (f == 9u) {
                fx = ans_len >= 2u && ans_len <= 4u ? SIMON_FX_TWICE : 0u;
            } else if (f == 8u) {
                fx = ans_len >= 2u ? SIMON_FX_SLEEP : 0u;
            } else if (f == 5u) {
                fx = SIMON_FX_PRE;
            } else {
                fx = simon_debut(lv, ans_len);
                if (fx && ((sim_seen >> fx) & 1u))
                    fx = 0;
                else if (fx == SIMON_FX_PRE)
                    sim_pre_first = 1;
                if (!fx && !sim_cool) {          /* no trick two rounds running */
                    if (ans_len >= 2u && sim_fx_rand() % 100u < simon_crystal_pct(lv))
                        fx = SIMON_FX_CRYSTAL;
                    else if (ans_len >= 3u && sim_fx_rand() % 100u < simon_lapse_pct(lv))
                        fx = lv >= 8u ? (sim_fx_rand() % 3u == 0u ? SIMON_FX_LAPSE : sim_fx_rand() % 2u ? SIMON_FX_RESTART : SIMON_FX_STALL) : SIMON_FX_LAPSE;   /* (the stall and the restart join in the finale) */
                    else if (ans_len >= 3u && ans_len + 2u >= simon_goal(lv) && sim_fx_rand() % 100u < simon_dark_pct(lv))
                        fx = SIMON_FX_DARK;
                    else if (sim_pick_rule(lv, ans_len) && sim_fx_rand() % 100u < simon_twice_pct(lv))
                        fx = SIMON_FX_TWICE;
                    else if (ans_len >= 2u && sim_fx_rand() % 100u < simon_eyes_pct(lv))
                        fx = SIMON_FX_EYES;
                    else if (ans_len >= 2u && sim_fx_rand() % 100u < simon_sleep_pct(lv))
                        fx = SIMON_FX_SLEEP;
                    else if (sim_fx_rand() % 100u < simon_pre_pct(lv))
                        fx = SIMON_FX_PRE;
                }
                sim_cool = fx ? 1u : 0u;
                if (fx)
                    sim_seen |= (uint16_t)(1u << fx);
            }
        }
        if (fx == SIMON_FX_CRYSTAL) {
            sim_crystal = 1;                 /* the crystal ball: everything slower */
            step = step * 3u / 2u;
            len_ms = len_ms * 3u / 2u;
        } else if (fx == SIMON_FX_DARK) {
            sim_dark = 1;                    /* the lights go out: only the newest notes still light */
            lit = simon_dark_lit(lv);
        } else if (fx == SIMON_FX_EYES) {
            sim_mute = (uint8_t)(f == 16u || (f != 15u && ans_len >= 3u && sim_fx_rand() % 2u) ? 2u : 1u);
            if (ans_len < 3u)
                sim_mute = 1;                /* (one silent note needs a note before and after it) */
            sim_mute_ix = (uint8_t)(sim_mute == 2u ? 1u + sim_fx_rand() % (ans_len - 2u) : 0u);
        } else if (fx == SIMON_FX_TWICE) {
            uint32_t rr = f == 9u ? 1u : (f >= 11u && f <= 14u) ? f - 9u : sim_pick_rule(lv, ans_len);   /* a riddle: its rule */
            if (rr) {
                sim_rule = (uint8_t)rr;
                sim_rule_seen |= (uint8_t)(1u << rr);
                sim_twice = 1;
                sim_build();
            }
        } else if (fx == SIMON_FX_SLEEP) {
            sim_sleepy = 1;                  /* sleepy: slower, dreamier */
            step = step * 3u / 2u;
            len_ms = len_ms * 3u / 2u;
        } else if (fx >= SIMON_FX_LAPSE && fx <= SIMON_FX_STALL) {   /* a lapse after note k (0-based), never the first or the last */
            k = 1u + sim_fx_rand() % (ans_len - 2u);
            lapse = 1;
            sim_gap_restart = (uint8_t)(fx == SIMON_FX_RESTART && k + 1u + ans_len <= SEQ_MAX);
            sim_gap_stall = (uint8_t)(fx == SIMON_FX_STALL);
        }
        if (fx == SIMON_FX_PRE || fx == SIMON_FX_CRYSTAL || (f >= 2u && f <= 4u && fx))
            sim_pre = simon_pre_ms(lv);      /* (the crystal ball always has it: it is the announcement) */
        if (sim_dark || sim_mute)
            sim_pre = 4200u;                 /* two beats of words */
        if (sim_twice && !hint)
            sim_pre = 4200u;                 /* two beats of words */
        if (sim_sleepy)
            sim_pre = 3600u;                 /* two beats of words */
        if (sim_pre_first)
            sim_pre = 3200u;                 /* the first time, time to read what it is */
    }
    if (angry) {
        sim_pre = 4800u;
        step = step * 7u / 5u;               /* backwards is hard: a slower pace, and every key stays lit */
        len_ms = len_ms * 7u / 5u;
        lit = 255u;
    }                     /* the spirits' anger: two beats of words */
    if (!hint)
        sim_distract = (uint8_t)(sim_pre || sim_gap_len || sim_crystal || sim_sleepy || sim_dark || sim_mute || sim_twice || angry);
    base = 400u + sim_pre;
    for (i = 0; i < ans_len; i++) {          /* the notes before the lapse, or all of them when there is none (or a stall) */
        uint32_t at, ln = len_ms;
        if (lapse && !sim_gap_stall && i > k)
            break;
        at = base + step * i + (sim_gap_stall && i > k ? SIM_STALL : 0u);
        if (sim_gap_stall && i == k)
            ln = len_ms + SIM_STALL;         /* the held note */
        seq_at[n] = (uint16_t)at;
        seq_len[n] = (uint16_t)ln;
        seq_key[n] = (hint && sim_back) ? ans[ans_len - 1u - i] : ans[i];   /* a wish in a backwards round plays it in the order you play it */
        seq_hl[n] = (uint8_t)(i + lit >= ans_len);   /* the lights fade as the levels rise */
        seq_mute[n] = (uint8_t)(sim_mute == 1u || (sim_mute == 2u && i == sim_mute_ix));
        seq_end = at + ln;
        n++;
    }
    if (lapse && sim_gap_stall) {
        sim_gap_at = base + step * k;
        sim_gap_len = len_ms + SIM_STALL;
    } else if (lapse) {
        sim_gap_at = base + step * k + len_ms;
        sim_gap_len = SIM_GAP;
        for (i = sim_gap_restart ? 0u : k + 1u; i < ans_len; i++) {     /* then the rest, or the whole thing again */
            uint32_t at = base + step * (k + 1u) + SIM_GAP + step * (sim_gap_restart ? i : i - k - 1u);
            seq_at[n] = (uint16_t)at;
            seq_len[n] = (uint16_t)len_ms;
            seq_key[n] = ans[i];
            seq_hl[n] = (uint8_t)(i + lit >= ans_len);
            seq_mute[n] = 0;
            seq_end = at + len_ms;
            n++;
        }
    }
    seq_n = n;
    eo_drone_set(-1, 0.0f);
    eo_seq_start(0, 2);
    ui.state = ST_LISTEN;
    ui.text = TX_WATCH;
    ans_pos = 0;
    eo_step_pos = -1;
}

static uint32_t sim_in_preroll(void)     /* her pre-roll is on: the swell and the dazzle, before the sequence */
{
    return ui.state == ST_LISTEN && seq_on && sim_pre && seq_pos < sim_pre && !sim_sleepy && !sim_twice && !sim_dark && !sim_mute;
}

static void sim_dazzle_leds(uint32_t *bright, uint32_t *mid, uint32_t *dim)   /* two crests run along the keys, past each other */
{
    uint32_t t = seq_pos, span = NKEYS + 6u, a = (t / 40u) % span, b = span - 1u - a, k;
    *bright = *mid = *dim = 0u;
    for (k = 0; k < NKEYS; k++) {
        int32_t da = (int32_t)a - 3 - (int32_t)k, db = (int32_t)b - 3 - (int32_t)k;
        if (da == 0 || db == 0)
            *bright |= 1u << k;
        else if (da == 1 || da == -1 || db == 1 || db == -1)
            *mid |= 1u << k;
        else if (da == 2 || db == 2 || da == -2 || db == -2)
            *dim |= 1u << k;
    }
}

static int32_t sim_env(uint32_t t, uint32_t from, uint32_t to)   /* 0..8: eased in over 300 ms, held, eased out over the last 300 */
{
    uint32_t a, b;
    if (t < from || t >= to)
        return 0;
    a = t - from;
    b = to - t;
    return (int32_t)((a < b ? a : b) >= 300u ? 8u : (a < b ? a : b) * 8u / 300u);
}

static void simon_over(void)             /* the game ends: a new best is kept now, in the quiet */
{
    eo_new_best(eo_score);
    if (save_pending) {
        save_pending = 0;
        store_now();
    }
    ui.msg_until = 0;
    ui.state = ST_READY;
    ui.text = TX_OVER;
    sim_wish_t0 = plat_ms();
    ui.redraw = 1;
}

static void simon_win(void)
{
    if (ans_len >= SIMON_MAX_LEN) {          /* the longest sequence: the game is won */
        eo_score = ans_len;
        simon_over();
        return;
    }
    if (!set.mode && eo_level() >= SIMON_LEVELS && ++sim_good >= 2u && ans_len <= 2u) {   /* only while sequences are short: backwards is hard */
        sim_good = 0;                        /* too good: you have angered the spirits (the next round is played backwards) */
        sim_angry = 1;
    }
    if (ans_len >= simon_goal(eo_level()))
        eo_complete(GAME_SIMON);
    eo_levelup = ans_len >= simon_goal(eo_level()) && set.level[GAME_SIMON] < SIMON_LEVELS;
    if (eo_levelup) {                        /* the level's goal length graduates you: that is this game's score */
        eo_score = ans_len;
        eo_new_best(eo_score);
    }
    eo_win_begin();
}

static void simon_miss(void)             /* the key you should have played sounds and lights, then the game ends */
{
    if (set.mode) {                          /* practice: the sequence again, and try once more */
        app_say("TRY AGAIN", "");
        simon_play();
        return;
    }
    eo_score = ans_len - 1u;
    sim_good = 0;
    sim_angry = 0;
    seq_at[0] = 0;
    seq_len[0] = 700;
    seq_key[0] = sim_expect();
    seq_n = 1;
    seq_end = 800;
    eo_drone_set(-1, 0.0f);
    eo_seq_start(0, 1);
    ui.state = ST_MISS;
    ui.text = TX_MISS;
}

static void sim_wish(void)               /* SELECT: a wish. In your turn: hear it again. After a miss: try the round again. */
{
    if (set.mode || (ui.state != ST_WISH && ui.state != ST_TURN))
        return;
    if (!sim_wishes) {
        if (ui.state == ST_TURN)
            app_say("NO WISHES", "LEFT");
        return;
    }
    sim_wishes--;
    sim_wish_peek = ui.state == ST_TURN;
    sim_good = 0;
    sim_hint = 1;                            /* the sequence again, everything lit, nothing odd */
    simon_play();
    sim_wish_until = plat_ms() + 2600u;
    ui.redraw = 1;
}

/* ---- Chord Dogtor */
static uint8_t dt_q_now;                 /* this round's chord quality */
static uint8_t dt_choice_now;            /* ... and which choice (A..D) is right */
static uint8_t dt_root_now;              /* ... and its root key */
static uint32_t dt_chord_until, dt_chord_at;   /* the chord sounds from dt_chord_at to dt_chord_until (ms); 0 = no chord scheduled */
static uint32_t dt_beep_next, dt_line_ix, dt_idle_at, dt_pressed_ix, dt_card_until;   /* a title card fills the screen until then */
static uint8_t dt_line_set, dt_line_n;   /* which dialogue (set 0..) and which line of it is on screen */
static const char *const dt_say[6][4][2] = {
    {   /* ready */
        {"Hello, I'm Dr. Diggie,", "your Chord Dogtor."}, {"Scrubbing in. Nurse,", "the chart, please."},
        {"Ignore the cat in room 2.", "He copies everything."}, {"Chart. Vitals. Scalpel.", "In that order."}},
    {   /* listening */
        {"Quiet in the O.R.", "I need to hear this."}, {"Something's off. I can", "feel it in my paws."},
        {"It's not lupus.", "It's never lupus."}, {"Run the tones. Stat.", ""}},
    {   /* your turn */
        {"What's your diagnosis,", "doctor?"}, {"Don't look at Copycat's", "chart. He copies."},
        {"Take your time.", "The patient can wait."}, {"I don't need a second", "opinion. Yours will do."}},
    {   /* right */
        {"Good catch, doctor.", "Textbook diagnosis."}, {"The patient is feeling", "much better. In tune."},
        {"Page the board:", "we have a prodigy."}, {"Note for the board: I got", "it first. Copycat meowed."}},
    {   /* wrong */
        {"Hmm. That's not what", "the chart says."}, {"A second opinion", "is needed."},
        {"Back to the lab.", "Something's a half-step off."}, {"Not quite, doctor.", "Recheck the vitals."}},
    {   /* level up, and bored */
        {"The chief wants to see you.", "Bring your stethoscope."}, {"Promoted to Attending.", "Pager's on the counter."},
        {"Doctor, we don't have", "time for rounds."}, {"Copycat is napping on", "the patient's chart."}}};
enum { DS_READY, DS_LISTEN, DS_TURN, DS_WIN, DS_MISS, DS_UP };
/* the plot: each level is an episode, a right answer is the next beat, the level-up card is the cliffhanger. A miss does not
 * move the story. (Beat 8 of the last level is the finale; for the others, the cliffhanger stands in for it.) */
static const char *const dt_story[DOGTOR_LEVELS][8][2] = {
    {{"Good catch. The patient", "is stable. For now."}, {"But the chart has a name:", "Dr. Copycat's."},
     {"Correct. Why does he know", "Copycat's cat?"}, {"Good. But the lab results", "came back... wrong."},
     {"A clean diagnosis. And a", "note taped to his coat."}, {"The note says: DON'T", "TRUST THE THIRD."},
     {"Good. But who sent", "the note?"}, {"Correct. The patient", "opens his eyes..."}},
    {{"Right. But the third", "option is no relief."}, {"Good. A shadow crosses", "the O.R. window."},
     {"Correct. The power", "flickers. Hold steady."}, {"Good. But the heart", "monitor just skipped."},
     {"Textbook. Someone moved", "the patient's chart."}, {"Right. And I found", "pawprints. Cat-sized."},
     {"Good. The blood work says", "he's not a patient."}, {"Correct. He's... the", "founder?!"}},
    {{"Good. But now there are", "four suspects."}, {"Right. The augmented one", "never resolves. Suspicious."},
     {"Correct. He was in the", "records room all night."}, {"Good. But the records", "are written in Braille."},
     {"Right. The founder had", "a twin. Named Fermata."}, {"Correct. Fermata holds", "everything. Even grudges."},
     {"Good. And somebody just", "paged me from my phone."}, {"Right. The call is coming", "from inside the clinic."}},
    {{"Good. But every room has", "a different key now."}, {"Right. The hospital is", "modulating. Hold on."},
     {"Correct. The twin left a", "map in the pharmacy."}, {"Good. The map is upside", "down. Like a seventh."},
     {"Right. X marks the spot.", "It's under my desk."}, {"Correct. A trapdoor.", "Nobody told me about it."},
     {"Good. Below: an old", "recording studio."}, {"Right. The tape is", "labelled: DIGGIE, AGE 3."}},
    {{"Good. The tape has seventh", "chords. Warm. Unresolved."}, {"Right. The voice is mine.", "But I was a puppy."},
     {"Correct. The twin took", "the second half."}, {"Good. And Copycat has", "been copying the tape."},
     {"Right. So that's why he", "meows in perfect pitch."}, {"Correct. A betrayal.", "In six-eight time."},
     {"Good. But the real twist", "is behind me."}, {"Right. Turn around.", "Slowly. Dr. Copycat?"}},
    {{"Good. Suspended. Like my", "disbelief."}, {"Right. Copycat says it", "was all a loop."},
     {"Correct. He wasn't the", "villain. He was the echo."}, {"Good. But who wrote the", "note on my coat?"},
     {"Right. It was me. From", "the future. Obviously."}, {"Correct. The founder", "retires. I get the clinic."},
     {"Good. Copycat gets the", "second chair. Meow."}, {"Right. Final diagnosis:", "the patient is just fine."}}};
static const char *const dt_cliff[DOGTOR_LEVELS - 1u][2] = {
    {"He whispers: \"Diggie?\"", "...Who is he?!"}, {"The lights go out.", "A dog barks. Not mine."},
    {"I recognize that bark.", "It's... my own."}, {"The tape says: DIGGIE,", "AGE 3. Play it."},
    {"Dr. Copycat holds the", "other half of the tape."}};
enum { DS_STORY = 6, DS_CLIFF, DS_COFFEE };
#define DT_CUPS_START 3u                 /* coffees per level: each one buys a walk through the chord's notes for this round */
#define DT_CUPS_MAX 5u
static uint8_t dt_vamp_on;               /* the ostinato's note is sounding now */
static uint8_t dt_cups, dt_cups_lv, dt_cup_spent, dt_step_n;   /* coffees left; the level they are for; spent this round; the note last heard */
static uint8_t dt_line_lv;               /* the story's level, for the line on screen */


static void dogtor_say(uint32_t set_ix, uint32_t pick, uint32_t card_ms)   /* his words; as a title card for card_ms */
{
    dt_line_set = (uint8_t)set_ix;
    dt_line_n = (uint8_t)(set_ix >= DS_STORY ? pick : pick % 4u);
    dt_line_lv = (uint8_t)eo_level();
    dt_card_until = card_ms ? plat_ms() + card_ms : 0u;
    ui.redraw = 1;
}

static void dogtor_chord(int on)         /* the chord sounds (or stops); the pedal bass stays */
{
    uint32_t i, n = 0;
    for (i = 0; i < CHORD_VOICES; i++)
        eo_chord_note[i] = -1;
    if (!on)
        return;
    for (i = 0; i < 4u && dt_q_iv[dt_q_now][i] >= 0; i++)
        eo_chord_note[n++] = KEY_NOTE0 + (int32_t)dt_root_now + dt_q_iv[dt_q_now][i];
}

static void dogtor_hear(uint32_t card)   /* the hum, then the chord over it (a title card first, for the first round) */
{
    uint32_t now = plat_ms();
    dogtor_chord(0);
    eo_drone_set(KEY_NOTE0 + (int32_t)ROOT_MIN - 12, 0.28f);   /* the machine hum: one fixed low C, very low (it is not the chord's root: this game is about the chord, not a key) */
    dt_chord_at = now + (card ? 1700u : 900u);
    dt_chord_until = dt_chord_at + 1800u;
    ui.state = ST_LISTEN;
    ui.text = TX_LISTEN;
    ans_pos = 0;
    dt_idle_at = now;
    if (card)
        dogtor_say(DS_LISTEN, dt_line_ix++, 1500u);
}

static void dogtor_round(uint32_t card)
{
    const dogtor_level_t *lv = dogtor_level(eo_level());
    eo_seed_once();
    ui.result = 0;
    eo_tries = 1;
    if (dt_cups_lv != eo_level()) {      /* a new level: a fresh pot */
        dt_cups_lv = (uint8_t)eo_level();
        dt_cups = DT_CUPS_START;
    }
    dt_cup_spent = 0;
    dt_step_n = 0;
    eo_seed = eo_seed * 1664525u + 1013904223u;
    dt_choice_now = (uint8_t)((eo_seed >> 16) % lv->n);
    dt_q_now = lv->q[dt_choice_now];
    dt_key_now = dt_answer_key[dt_choice_now];
    eo_seed = eo_seed * 1664525u + 1013904223u;
    dt_root_now = lv->moves ? (uint8_t)(ROOT_MIN + (eo_seed >> 16) % (ROOT_MAX - ROOT_MIN + 1u)) : set.root;
    ans_len = 1;
    ans[0] = dt_key_now;
    dogtor_hear(card);
}

static void dogtor_stop(void)
{
    uint32_t i;
    for (i = 0; i < CHORD_VOICES; i++)
        eo_chord_note[i] = -1;
    dt_chord_until = 0;
    dt_chord_at = 0;
    eo_drone_set(-1, 0.0f);
}

static void dogtor_step(int32_t d)       /* SELECT while you decide: the chord's notes, one at a time (the first costs a coffee) */
{
    uint32_t n = 0, ix;
    char t[12], b[8];
    int32_t k;
    while (n < 4u && dt_q_iv[dt_q_now][n] >= 0)
        n++;
    if (!dt_cup_spent) {
        if (!dt_cups) {
            app_say("NO COFFEE", "");
            return;
        }
        dt_cups--;                       /* one cup for the whole round */
        dt_cup_spent = 1;
        dt_step_n = d > 0 ? (uint8_t)(n - 1u) : 0u;   /* so the first notch lands on the root, or the top note going down */
        ui.redraw = 1;
    }
    ix = (uint32_t)(((int32_t)dt_step_n + d % (int32_t)n + (int32_t)n) % (int32_t)n);
    dt_step_n = (uint8_t)ix;
    k = (int32_t)dt_root_now + dt_q_iv[dt_q_now][ix];
    seq_on = 0;
    voice_note = -1;
    seq_lit = 0;
    eo_soft_env = 0;                     /* not through the ostinato's reverb and slow fade */
    dt_vamp_on = 0;
    seq_at[0] = 0;
    seq_len[0] = 520;
    seq_key[0] = (uint8_t)k;
    seq_vo_stage[0] = V_WARM;
    seq_n = 1;
    seq_end = 600;
    eo_seq_start(0, 0);
    t[0] = 0;
    int_text((int32_t)ix + 1, b);
    text_cat(t, b);
    text_cat(t, " OF ");
    int_text((int32_t)n, b);
    text_cat(t, b);
    app_say("NOTE", t);
}

static void dogtor_win(void)
{
    ui.result = 1;
    dogtor_chord(0);
    dt_chord_until = 0;
    dt_chord_at = 0;
    if (set.mode) {
        dogtor_say(DS_WIN, dt_line_ix++, 0u);
        eo_miss_run = 0;
        eo_levelup = 0;
        eo_win_begin();
        return;
    }
    streak++;
    eo_miss_run = 0;
    eo_new_best(streak);
    if (set.prog[GAME_DOGTOR] < eo_rounds(GAME_DOGTOR))
        set.prog[GAME_DOGTOR]++;
    dogtor_say(DS_STORY, set.prog[GAME_DOGTOR] ? set.prog[GAME_DOGTOR] - 1u : 0u, 0u);   /* the next beat of the plot */
    if (set.prog[GAME_DOGTOR] >= eo_rounds(GAME_DOGTOR))
        eo_complete(GAME_DOGTOR);
    set_dirty = 1;
    eo_levelup = set.prog[GAME_DOGTOR] >= eo_rounds(GAME_DOGTOR) && set.level[GAME_DOGTOR] < DOGTOR_LEVELS;
    eo_win_begin();
}

static void dogtor_miss(uint32_t pressed)
{
    ui.result = 2;
    dt_pressed_ix = pressed;
    eo_miss_run++;
    dogtor_say(DS_MISS, dt_line_ix++, 0u);
    if (!set.mode && eo_miss_run == 2u && dt_cups < DT_CUPS_MAX) {   /* two misses running: the dog hands you a coffee */
        dt_cups++;
        dogtor_say(DS_COFFEE, 0u, 0u);
    }
    if (set.mode) {                      /* practice: the same chord again, nothing counted */
        eo_tries++;
        dogtor_hear(0u);
        return;
    }
    streak = 0;
    if (set.prog[GAME_DOGTOR]) {
        set.prog[GAME_DOGTOR] = set.prog[GAME_DOGTOR] > eo_rounds(GAME_DOGTOR) / 4u ? (uint8_t)(set.prog[GAME_DOGTOR] - eo_rounds(GAME_DOGTOR) / 4u) : 0u;
        set_dirty = 1;
        save_pending = 1;
    }
    dogtor_chord(1);                     /* the chord again while the right answer is lit: so you hear what it was */
    dt_chord_until = plat_ms() + 1800u;
    dt_chord_at = 0;
    seq_at[0] = 0;                       /* and a whimper: three high, short whines, each sagging, the last one trailing off */
    seq_len[0] = 260;
    seq_key[0] = (uint8_t)(dt_root_now + 19u);
    seq_vo_stage[0] = V_CATSAD;
    seq_at[1] = 330;
    seq_len[1] = 240;
    seq_key[1] = (uint8_t)(dt_root_now + 18u);
    seq_vo_stage[1] = V_CATSAD;
    seq_at[2] = 640;
    seq_len[2] = 600;
    seq_key[2] = (uint8_t)(dt_root_now + 17u);
    seq_vo_stage[2] = V_CATSAD;
    seq_n = 3;
    seq_end = 1300;
    eo_seq_start(0, 0);
    ui.state = ST_MISS;
    ui.text = TX_MISS;
}

static void dogtor_win_sound(void)       /* a soft bell, then a happy howl: ding, awoooo */
{
    seq_at[0] = 0;
    seq_len[0] = 300;
    seq_key[0] = (uint8_t)(dt_root_now + 24u);
    seq_vo_stage[0] = V_PING;
    seq_at[1] = 380;
    seq_len[1] = 1150;
    seq_key[1] = (uint8_t)(dt_root_now + 12u);
    seq_vo_stage[1] = V_HOWL;
    seq_n = 2;
    seq_end = 1600;
    eo_drone_set(KEY_NOTE0 + (int32_t)ROOT_MIN - 12, 0.28f);
    eo_seq_start(0, 0);
}

static uint32_t dt_vamp_t0;
static void dogtor_vamp_tick(uint32_t now)   /* whenever the chord is not sounding: C, G, C, G, plucked, in a loop; fixed, whatever the chord */
{
    static const uint8_t OFS[4] = {0, 7, 12, 7};      /* no third, no seventh: it cannot hint at the chord's quality */
    int want = (ui.state == ST_TURN || ui.state == ST_GAP) && !seq_on && !dt_chord_until && !dt_chord_at;
    uint32_t t = now - dt_vamp_t0, slot = t / 300u;
    if (want && (t % 300u) < 160u) {
        voice_fx = V_TRI;
        voice_vel = 0.2f;
        eo_soft_env = 1;
        voice_note = KEY_NOTE0 + (int32_t)ROOT_MIN + OFS[slot & 3u];
        dt_vamp_on = 1;
    } else if (dt_vamp_on) {             /* between plucks, or when the chord, a sound or the answer takes over */
        dt_vamp_on = 0;
        if (!seq_on)
            voice_note = -1;             /* the pluck keeps its own waveform while it fades: switching voices here made a blip */
    }
    if (!want && !dt_vamp_on) {
        if (seq_on) {
            eo_soft_env = 0;             /* another sound (the bell, the howl, a chord note) is playing: no reverb, quick envelopes */
        } else if (voice_note < 0 && voice_level < 0.0004f) {
            voice_fx = 0xFFu;            /* the fade is over: the chosen voice and the quick envelopes are back */
            eo_soft_env = 0;
        }
    }
    if (!want)
        dt_vamp_t0 = now;                /* each time it comes back, it starts from the root */
}

static void dogtor_tick(uint32_t now)    /* the chord's timing, the heart monitor, the bored dog */
{
    dogtor_vamp_tick(now);
    int on = ui.state == ST_LISTEN || ui.state == ST_TURN || ui.state == ST_WIN || ui.state == ST_MISS || ui.state == ST_GAP;
    if (dt_chord_at && now >= dt_chord_at) {
        dt_chord_at = 0;
        dogtor_chord(1);
    }
    if (dt_chord_until && now >= dt_chord_until) {
        dt_chord_until = 0;
        dogtor_chord(0);
        if (ui.state == ST_LISTEN) {
            ui.state = ST_TURN;
            ui.text = TX_TURN;
            ans_pos = 0;
            dt_idle_at = now;
            app_say("YOUR TURN", "");
        }
    }
    if (on && now >= dt_beep_next) {     /* a steady beep; a little slower after a miss */
        eo_beep_trig++;
        dt_beep_next = now + (eo_miss_run ? 1500u : 1100u);
    }
    if (!on)
        dt_beep_next = now + 400u;
    if (dt_card_until && now >= dt_card_until) {    /* the title card is over */
        dt_card_until = 0;
        ui.redraw = 1;
    }
}

/* ---- both */
static void eo_enter_levelup(void)       /* ten rounds right: the next level, kept now (the sound is over: it is quiet) */
{
    char n[12];
    set.level[set.game]++;
    set.prog[set.game] = 0;
    set_dirty = 1;
    save_pending = 0;
    store_now();
    eo_levelup = 0;
    ans_len = 0;
    ans_pos = 0;
    ui.state = ST_LEVELUP;
    ui.text = TX_GOOD;
    ui.up_until = set.game == GAME_COPYCAT ? plat_ms() + EO_CELEBRATE_MS : 0u;   /* the cat celebrates first */
    sim_up_t0 = plat_ms();
    if (set.game == GAME_COPYCAT)
        play_levelup_song();
    if (set.game == GAME_SIMON && !set.mode && eo_level() >= 2u && eo_level() <= SIMON_LEVELS)
        sim_enter_up();                  /* (her level-up is a little show, like her introduction) */
    int_text((int32_t)eo_level(), n);
    app_say("LEVEL", n);
    if (set.game == GAME_DOGTOR)
        dogtor_say(DS_CLIFF, eo_level() >= 2u ? eo_level() - 2u : 0u, 3200u);   /* the cliffhanger of the episode just won */
}

static void eo_start_game(void)
{
    ans_len = 0;
    streak = 0;
    eo_levelup = 0;
    if (set.game == GAME_SIMON) {
        sim_wishes = 3;
        sim_seen = 0;
        sim_rule_seen = 0;
        sim_cool = 0;
        sim_good = 0;
        sim_angry = 0;
        sim_back = 0;
        simon_grow();
    }
    else if (set.game == GAME_DOGTOR)
        dogtor_round(1u);
    else
        copycat_round();
}

#define SIM_KX 6                         /* the left edge of the first white key */
#define SIM_KP 46                        /* the pitch of the white keys: C D E F G, 44 wide */
#define SIM_NB 18u
static struct { uint32_t t0; int16_t x; uint8_t seed, sz, w; } sim_bub[SIM_NB];   /* the bubbles your notes send up (t0 = 0: free) */
static uint8_t sim_bub_next;
static void sim_bub_spawn(uint32_t key)  /* a note you play: a few bubbles rise from the bottom of the screen, from its key */
{
    uint32_t i, n = sim_crystal ? 6u : 3u, now = plat_ms(), ix = simon_word_ix(key - SIMON_BASE);
    int32_t kx = SIM_KX + (key == 7u ? 0 : key == 9u ? 1 : key == 11u ? 2 : 4) * SIM_KP + 22;
    for (i = 0; i < n; i++) {
        uint32_t r = sim_fx_rand();
        sim_bub[sim_bub_next].t0 = now + i * 90u;
        sim_bub[sim_bub_next].x = (int16_t)(kx - 16 + (int32_t)(r % 33u));
        sim_bub[sim_bub_next].seed = (uint8_t)(r >> 8);
        sim_bub[sim_bub_next].sz = (uint8_t)(5u + ((r >> 16) % 3u) * 3u);
        sim_bub[sim_bub_next].w = (uint8_t)ix;
        sim_bub_next = (uint8_t)((sim_bub_next + 1u) % SIM_NB);
    }
}

static void judge(uint32_t key)          /* a key pressed in YOUR TURN */
{
    if (set.game == GAME_SIMON)
        sim_bub_spawn(key);
    ui.msg_until = 0;                        /* the header's YOUR TURN is over: the screen says what happened */
    if (set.game == GAME_DOGTOR) {           /* only the answer keys count; any other key is just a key */
        uint32_t i;
        for (i = 0; i < dogtor_level(eo_level())->n; i++)
            if (key == dt_answer_key[i])
                break;
        if (i >= dogtor_level(eo_level())->n)
            return;
        if (key == ans[0])
            dogtor_win();
        else
            dogtor_miss(i);
        ui.redraw = 1;
        return;
    }
    if (key != (set.game == GAME_SIMON ? sim_expect() : ans[ans_pos])) {
        if (set.game == GAME_SIMON)
            simon_miss();
        else
            copycat_miss();
        ui.redraw = 1;
        return;
    }
    ans_pos++;
    ui.redraw = 1;
    if (ans_pos >= (set.game == GAME_SIMON ? sim_need() : ans_len)) {
        if (set.game == GAME_SIMON)
            simon_win();
        else
            copycat_win();
    }
}

static void stop_game(void)              /* HOME: stops, and keeps your progress */
{
    uint32_t was = ui.state;
    seq_on = 0;
    seq_lit = 0;
    voice_note = -1;
    eo_drone_set(-1, 0.0f);
    dogtor_stop();
    dogtor_say(DS_READY, 0, set.game == GAME_DOGTOR ? 2200u : 0u);
    eo_win_pending = 0;
    eo_levelup = 0;
    streak = 0;
    ans_len = 0;
    ui.result = 0;
    ui.kb_show = 0;
    ui.say[0] = 0;
    ui.state = ST_READY;
    ui.text = TX_READY;
    ui.redraw = 1;
    if (was != ST_READY && set_dirty)
        store_now();
}

static void root_step(int32_t d)
{
    int32_t r = clamp((int32_t)set.root + d, (int32_t)ROOT_MIN, (int32_t)ROOT_MAX);
    char n[8];
    if (r == (int32_t)set.root)
        return;
    set.root = (uint8_t)r;
    set_dirty = 1;
    note_name(KEY_NOTE0 + r, n);
    app_say("ROOT", n);
}

static void level_step(int32_t d)
{
    int32_t l = clamp((int32_t)eo_level() + d, 1, (int32_t)eo_levels[set.game]);
    char n[12], m[24];
    if (l > (int32_t)eo_open(set.game)) {    /* locked: say what unlocks it, and stay at the highest open level */
        m[0] = 0;
        text_cat(m, "FINISH LEVEL ");
        int_text(l - (int32_t)EO_UNLOCK, n);
        text_cat(m, n);
        app_say("LOCKED", m);
        l = (int32_t)eo_open(set.game);
    }
    if (l == (int32_t)eo_level())
        return;
    set.level[set.game] = (uint8_t)l;
    set.prog[set.game] = 0;
    eo_miss_run = 0;
    set_dirty = 1;
    ans_len = 0;
    ui.text = TX_READY;
    int_text(l, n);
    app_say("LEVEL", n);
}

static void eo_voice_sync(void);
static void voice_step(int32_t d)        /* K2: the melody voice, with a short note to hear it */
{
    int32_t v = clamp((int32_t)set.voice + d, 0, (int32_t)VOICES - 1);
    if (v == (int32_t)set.voice)
        return;
    set.voice = (uint8_t)v;
    eo_voice_sync();
    set_dirty = 1;
    if (ui.state == ST_READY) {              /* a short note to hear it (not in the middle of a round) */
        seq_at[0] = 0;
        seq_len[0] = 450;
        seq_key[0] = (uint8_t)SIMON_BASE;
        seq_n = 1;
        seq_end = 550;
        eo_seq_start(0, 0);
    }
    app_say("VOICE", eo_voice_name[v]);
}

static void speed_step(int32_t d)        /* K3: how fast the melody plays back */
{
    int32_t v = clamp((int32_t)set.speed + d, (int32_t)SPEED_MIN, (int32_t)SPEED_MAX);
    char n[12];
    if (v == (int32_t)set.speed)
        return;
    set.speed = (uint8_t)v;
    set_dirty = 1;
    int_text(v * 5, n);
    text_cat(n, "%");
    app_say("SPEED", n);
}

static void mode_step(int32_t d)         /* K4: challenge, or practice (try the same melody until it is right) */
{
    uint8_t m = d > 0 ? 1u : 0u;
    if (m == set.mode)
        return;
    set.mode = m;
    set_dirty = 1;
    app_say(m ? "PRACTICE" : "CHALLENGE", "");
}

static void melody_step(int32_t d)       /* SELECT during your turn: hear the melody one note per notch, and wrap around */
{
    int32_t n = (int32_t)ans_len, p = eo_step_pos;
    char t[16], b[8];
    if (n < 1)
        return;
    p = p < 0 ? (d > 0 ? 0 : n - 1) : (p + d % n + n) % n;
    eo_step_pos = p;
    {
        uint32_t gap = (seq_on || voice_note >= 0) ? 60u : 0u;   /* a note still sounding is cut off first: no overlap */
        seq_on = 0;                      /* stop the old note before the arrays change under it */
        voice_note = -1;
        seq_lit = 0;
        seq_at[0] = (uint16_t)gap;
        seq_len[0] = (uint16_t)eo_spd(900u);
        seq_key[0] = ans[p];
        seq_hl[0] = 0;
        seq_n = 1;
        seq_end = gap + eo_spd(900u) + 40u;
    }
    eo_seq_start(0, set.game == GAME_SIMON || copycat_level(eo_level())->lit == LIT_ALL ? 1u : 0u);
    t[0] = 0;
    int_text(p + 1, b);
    text_cat(t, b);
    text_cat(t, " OF ");
    int_text(n, b);
    text_cat(t, b);
    app_say("NOTE", t);
}

static void eo_voice_sync(void)          /* the cat meows; Simone is a goldfish, so she gets a bell of her own */
{
    eo_voice = set.game == GAME_SIMON ? (uint8_t)V_BELL : set.voice;
}

static void eo_game_step(int32_t d)
{
    int32_t g = clamp((int32_t)set.game + d, 0, (int32_t)GAMES - 1);
    if (g == (int32_t)set.game)
        return;
    set.game = (uint8_t)g;
    eo_voice_sync();
    set_dirty = 1;
    ans_len = 0;
    ui.text = TX_READY;
    app_say(eo_game_name[g], "");
    if (g == GAME_DOGTOR)
        dogtor_say(DS_READY, 0, 2200u);
}

static void settings_load(void)          /* the saved settings, or the defaults when there are none or they are wrong */
{
    uint32_t g, ok;
    ok = plat_store_load(OBJ_SETTINGS, &set, sizeof set) == (int)sizeof set && set.magic == SET_MAGIC &&
         set.game < GAMES && set.root >= ROOT_MIN && set.root <= ROOT_MAX;
    for (g = 0; ok && g < GAMES; g++)
        if (set.level[g] < 1u || set.level[g] > eo_levels[g] || set.prog[g] > (g == GAME_COPYCAT ? copycat_level(set.level[g])->rounds : g == GAME_DOGTOR ? DOGTOR_ROUNDS : EO_ROUNDS_UP) || set.top[g] > eo_levels[g] ||
            (set.level[g] > set.top[g] + EO_UNLOCK && !(set.pad & 1u)))
            ok = 0;
    if (set.voice >= VOICES)
        ok = 0;
    if (!ok) {
        set.magic = SET_MAGIC;
        set.game = GAME_COPYCAT;
        set.root = (uint8_t)ROOT_MIN;    /* C4 */
        for (g = 0; g < GAMES; g++) {
            set.level[g] = 1;
            set.prog[g] = 0;
            set.top[g] = 0;
            set.best[g] = 0;
        }
        set.voice = V_MEOW;
        set.speed = SPEED_DEF;
        set.mode = 0;
        set.pad = 0;
    }
    if (set.speed < SPEED_MIN || set.speed > SPEED_MAX)
        set.speed = SPEED_DEF;
    if (set.mode > 1u)
        set.mode = 0;
    eo_voice_sync();
}

void app_init(void)                      /* boot: the settings, and a quiet start (the simulator's reboot keeps RAM) */
{
    settings_load();
    eo_tones_init();
    {
        uint32_t i;
        for (i = 0; i < SEQ_MAX; i++)
            seq_vo[i] = seq_vo_stage[i] = 0xFFu;
    }
    seq_on = 0;
    seq_lit = 0;
    seq_done = 0;
    voice_note = -1;
    eo_drone_note = -1;
    ui.ctl_page = 0;
    ui.state = ST_MOVIE;                 /* every start: the movie, then the game select (the cat introduces itself when picked) */
    mv_t0 = plat_ms();
    mv_m0 = mv_t0 + MV_MUSIC_MS;
    mv_stage = 0;
    mv_wave_t0 = 0;
    eo_cat_seen = 0;
    eo_sim_seen = 0;
    ui.text = TX_READY;
    ui.kb_show = 0;
    ui.say[0] = 0;
    ui.vol_until = 0;
    ui.keys = 0;
    ui.btn = 0;
    ans_len = 0;
    streak = 0;
    save_pending = 0;
    eo_win_pending = 0;
    eo_levelup = 0;
#ifdef PLAT_HOST
    eo_seed = 0x2545F491u;               /* the simulator's rounds are the same every run: its tests know them */
#else
    eo_seed = 0;
#endif
}

static void eo_debug_tap(uint32_t new_btn, uint32_t now)   /* GLO three times in a row: debug mode, every level open */
{
    static uint32_t taps, last;
    uint32_t g;
    if (!(new_btn & (1u << B_GLO)))
        return;
    taps = (taps && now - last < 900u) ? taps + 1u : 1u;
    last = now;
    if (taps < 3u)
        return;
    taps = 0;
    set.pad ^= 1u;
    if (!(set.pad & 1u)) {               /* off: back inside the real unlock, so the saved levels stay valid */
        for (g = 0; g < GAMES; g++) {
            uint32_t m = set.top[g] + EO_UNLOCK;
            if (set.level[g] > m) {
                set.level[g] = (uint8_t)m;
                set.prog[g] = 0;
            }
        }
    }
    set_dirty = 1;
    if (ui.state == ST_MOVIE || ui.state == ST_SELECT || ui.state == ST_SIMONE) {
        sel_blip((set.pad & 1u) ? 30u : 6u, V_TRI);
    } else {
        app_say("DEBUG", (set.pad & 1u) ? "ALL LEVELS OPEN" : "OFF");
    }
    ui.redraw = 1;
}

static void input(void)
{
    uint32_t keys = plat_keys() | eo_midi_keys, btn = plat_buttons(), k;
    eo_debug_tap(btn & ~ui.btn, plat_ms());
    if (ui.state == ST_MOVIE || ui.state == ST_SELECT) {   /* the movie: any key skips it; the select: PRESETS move, PLAY starts */
        uint32_t new_keys = keys & ~ui.keys, new_btn = btn & ~ui.btn;
        for (k = 0; k < NE; k++) {
            int32_t e = plat_enc((int)k);
            if (e && ui.state == ST_SELECT && (k == EN_PRESET || k == EN_SELECT || k == EN_K1))
                sel_move(e);
        }
        if (ui.state == ST_MOVIE) {
            if (new_keys || new_btn)
                mv_end();
        } else if (new_btn & (1u << B_PLAY)) {
            sel_go();
        }
        ui.keys = keys;
        ui.btn = btn;
        lights();
        return;
    }
    if (ui.state == ST_SIMONE) {             /* Simone's show: a key finishes the line, then goes on; HOME skips it all */
        uint32_t new_keys = keys & ~ui.keys, new_btn = btn & ~ui.btn;
        for (k = 0; k < NE; k++)
            (void)plat_enc((int)k);
        if (new_btn & (1u << B_HOME))
            sim_end();
        else if (new_keys || new_btn)
            sim_key(plat_ms());
        ui.keys = keys;
        ui.btn = btn;
        lights();
        return;
    }
    if (ui.state == ST_INTRO || ui.state == ST_CONTROLS) {   /* the cat's talk: any key goes on, HOME skips to the game */
        uint32_t new_keys = keys & ~ui.keys, new_btn = btn & ~ui.btn;
        for (k = 0; k < NE; k++)
            (void)plat_enc((int)k);          /* the knobs do nothing yet, and none piles up */
        if (new_btn & (1u << B_HOME)) {
            ui.state = ST_READY;
        } else if (new_keys || new_btn) {
            if (ui.state == ST_INTRO) {
                ui.state = ST_CONTROLS;
                ui.ctl_page = 0;
            } else if (ui.ctl_page < 2u) {
                ui.ctl_page++;               /* the next of the three controls screens */
            } else {
                ui.state = ST_READY;
            }
        }
        if (ui.state == ST_READY)
            ui.text = TX_READY;
        if (new_keys || new_btn)
            ui.redraw = 1;
        ui.keys = keys;
        ui.btn = btn;
        lights();
        return;
    }
    for (k = 0; k < NKEYS; k++) {            /* the keys play, except while a sequence is being heard */
        uint32_t bit = 1u << k;
        int32_t note = KEY_NOTE0 + (int32_t)k;
        if ((keys & bit) && !(ui.keys & bit)) {
            if (!seq_on && set.game != GAME_DOGTOR) {
                voice_fx = 0xFFu;
                voice_vel = 0.8f;
                voice_note = note + (set.game == GAME_SIMON ? 12 : 0);
            }
            if (ui.state == ST_TURN && !seq_on) {
                judge(k);                    /* a miss starts the answer, which takes the voice over */
                if (ui.state != ST_TURN)
                    break;
            }
        } else if (!(keys & bit) && (ui.keys & bit) && !seq_on && voice_note == note + (set.game == GAME_SIMON ? 12 : 0) && set.game != GAME_DOGTOR) {
            voice_note = -1;
        }
    }
    for (k = 0; k < NB; k++) {               /* a button pressed */
        uint32_t bit = 1u << k;
        if (!(btn & bit) || (ui.btn & bit))
            continue;
        if (k == B_PLAY) {
            if (ui.state == ST_READY)
                eo_start_game();
            else if (ui.state == ST_WISH)
                simon_over();                /* no wish: the game ends */
            else if (ui.state == ST_TURN && set.game == GAME_COPYCAT) {
                if (eo_assist() && !ui.kb_show) {
                    eo_say("Sssss... let me hit the lights for you.", 6000u);
                    ui.kb_show = 1;
                    ui.kb_at = plat_ms();
                }
                copycat_play(1u);            /* hear it again: you start the melody over */
            }
            else if (ui.state == ST_TURN && set.game == GAME_DOGTOR)
                dogtor_hear(0u);             /* the chord again */
            else if (ui.state == ST_LEVELUP)
                eo_start_game();             /* on from the GOOD JOB card, at the new level */
        } else if (k == B_SEL && ui.state == ST_READY) {
            ui.state = ST_CONTROLS;          /* the controls again, a screen at a time; any key goes on */
            ui.ctl_page = 0;
            ui.redraw = 1;
        } else if (k == B_HOME) {
            if (ui.state == ST_READY)
                sel_enter();             /* HOME from the ready screen: back to the game select */
            else
                stop_game();
        } else if (k == B_SAVE && !seq_on) {
            save_button();
        }
    }
    for (k = 0; k < NE; k++) {               /* every encoder is read, so none piles up */
        int32_t e = plat_enc((int)k);
        if (!e)
            continue;
        if (k == EN_K2)                      /* voice, speed and mode can change in the middle of a round */
            voice_step(e);
        else if (k == EN_K3)
            speed_step(e);
        else if (k == EN_K4)
            mode_step(e);
        else if (k == EN_SELECT && ui.state == ST_TURN && set.game == GAME_DOGTOR)
            dogtor_step(e);
        else if (k == EN_SELECT && set.game == GAME_SIMON && !set.mode && (ui.state == ST_TURN || ui.state == ST_WISH))
            sim_wish();
        else if (k == EN_SELECT && ui.state == ST_TURN)
            melody_step(e);
        else if (ui.state == ST_READY) {
            if (k == EN_PRESET)
                eo_game_step(e);
            else if (k == EN_SELECT && set.game == GAME_COPYCAT && !eo_moves())
                root_step(e);
            else if (k == EN_K1)
                level_step(e);
        }
    }
    if (keys != ui.keys || btn != ui.btn)
        ui.redraw = 1;
    ui.keys = keys;
    ui.btn = btn;
    lights();
}

void app_input_only(void) { input(); }

/* ----------------------------------------------------------------- screen --- */
static void ctext(int32_t y, const felucca_font_t *f, const char *s, uint16_t c)   /* centred on the screen */
{
    cv_text((240 - text_w(f, s)) / 2, y, f, s, c);
}

static void key_cell(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t k, uint16_t off, uint32_t bright,
                     uint32_t mid, uint32_t dim)
{
    uint32_t bit = 1u << k;
    cv_rect(x, y, w, h, (bright & bit) ? C_HI : (mid & bit) ? C_AMB : (dim & bit) ? C_GRAY : off);
}

/* ------------------------------------------------------------------- the cat --- */
/* an original orange tabby with headphones (eo_cat.h, from tests/gen_cat.py). The head is a bitmap; the face is drawn
 * here so it can change. Poses: IDLE (blinks), DANCE (while the melody plays), SING (while you play), WIN, FAIL, and
 * LEVELUP (the celebration). The cat's picture is 100 x 90 pixels, drawn at three times the bitmap's size. */
#define EO_RGB(r, g, b) ((uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3)))
#include "eo_dog.h"
#define CAT_X 0
#define CAT_Y 4                          /* in the middle band; the screen's y is 36 more */
#define CAT_W 100
#define CAT_H 90
enum { POSE_IDLE, POSE_DANCE, POSE_SING, POSE_WIN, POSE_FAIL, POSE_LEVELUP, POSE_TALK };
static int32_t cat_sc = 3, cat_mx = 8, cat_my = 12;   /* the cat's size (pixels per bitmap pixel) and the margins round it */

static uint32_t eo_pose(uint32_t now)
{
    switch (ui.state) {
    case ST_INTRO:
    case ST_CONTROLS:
        return POSE_TALK;
    case ST_LEVELUP:
        return now < ui.up_until ? POSE_LEVELUP : POSE_WIN;
    case ST_LISTEN:
        return POSE_DANCE;
    case ST_WIN:
        return POSE_WIN;
    case ST_MISS:
        return POSE_FAIL;
    case ST_GAP:
        return ui.result == 1u ? POSE_WIN : ui.result == 2u ? POSE_FAIL : POSE_IDLE;
    default:                             /* ready, your turn: it sings along with the keys you play */
        return voice_note >= 0 ? POSE_SING : POSE_IDLE;
    }
}

static uint32_t eo_frame(uint32_t now, uint32_t pose)   /* the animation's frame: changes when the picture should */
{
    switch (pose) {
    case POSE_DANCE:
        return now / 220u;
    case POSE_SING:
        return now / 110u;
    case POSE_WIN:
        return now / 130u;
    case POSE_FAIL:
        return now / 260u;
    case POSE_LEVELUP:
        return now / 110u;
    case POSE_TALK:
        return now / 170u;
    default:
        return (now % 3200u) < 160u;     /* a blink now and then */
    }
}

static uint16_t eo_cat_col(char c)
{
    switch (c) {
    case 'k': return EO_RGB(96, 48, 28);
    case 'o': return EO_RGB(246, 152, 56);
    case 'd': return EO_RGB(184, 100, 28);
    case 'c': return EO_RGB(248, 234, 214);
    case 'e': return EO_RGB(240, 176, 160);
    case 'p': return EO_RGB(238, 130, 222);
    case 'q': return EO_RGB(176, 84, 168);
    default: return EO_RGB(120, 60, 70);     /* n: the nose */
    }
}

static void cat_note(int32_t x, int32_t y, uint16_t c)   /* a music note, 10 x 12 pixels */
{
    cv_rect(x + 4, y, 2, 10, c);
    cv_rect(x + 4, y, 6, 2, c);
    cv_rect(x + 8, y + 2, 2, 3, c);
    cv_rect(x, y + 8, 6, 4, c);
}

static void cat_spark(int32_t x, int32_t y, uint16_t c)  /* a little plus, 7 x 7 */
{
    cv_rect(x + 3, y, 1, 7, c);
    cv_rect(x, y + 3, 7, 1, c);
}

static void cat_paint(int32_t ox, int32_t oy, uint32_t pose, uint32_t fr)
{
    static const int8_t SWAY_X[4] = {-3, 0, 3, 0}, SWAY_Y[4] = {0, -3, 0, -3};
    static const int8_t HOP_W[4] = {0, -6, -9, -6}, HOP_L[4] = {0, -9, -12, -9};
    const uint16_t EYE = EO_RGB(60, 30, 16), MOUTH = EO_RGB(60, 24, 24), TONGUE = EO_RGB(230, 90, 110);
    const uint16_t WHITE = EO_RGB(255, 255, 255), GOLD = EO_RGB(255, 210, 60), TEAR = EO_RGB(120, 190, 255);
    int32_t bx = ox + cat_mx, by = oy + cat_my, x, y, k, f4 = (int32_t)(fr & 3u);
#define SCL(v) ((v) * cat_sc / 3)
    int open_mouth = (fr & 1u) != 0u;
    switch (pose) {
    case POSE_DANCE:
        bx += SWAY_X[f4];
        by += SWAY_Y[f4];
        break;
    case POSE_WIN:
        by += HOP_W[f4];
        break;
    case POSE_LEVELUP:
        by += HOP_L[f4];
        break;
    case POSE_FAIL:
        by += 3;
        break;
    default:
        break;
    }
#define FP(lx, ly, lw, lh, col) cv_rect(bx + (lx) * cat_sc, by + (ly) * cat_sc, (lw) * cat_sc, (lh) * cat_sc, (col))
    for (y = 0; y < EO_CAT_H; y++)       /* the head: runs of one colour */
        for (x = 0; x < EO_CAT_W;) {
            char c = EO_CAT[y][x];
            int32_t e = x + 1;
            while (e < EO_CAT_W && EO_CAT[y][e] == c)
                e++;
            if (c != '.')
                FP(x, y, e - x, 1, eo_cat_col(c));
            x = e;
        }
    /* the eyes */
    if (pose == POSE_FAIL) {             /* shut tight, with a tear */
        FP(8, 14, 3, 1, EYE);
        FP(17, 14, 3, 1, EYE);
        FP(9, 15 + (int32_t)(fr & 1u), 1, 2, TEAR);
    } else if (pose == POSE_IDLE || pose == POSE_TALK) {
        if (pose == POSE_IDLE && fr) {   /* a blink */
            FP(9, 14, 2, 1, EYE);
            FP(18, 14, 2, 1, EYE);
        } else {
            FP(9, 13, 2, 3, EYE);
            FP(18, 13, 2, 3, EYE);
            FP(9, 13, 1, 1, WHITE);
            FP(18, 13, 1, 1, WHITE);
        }
    } else {                             /* happy: an arch over each eye */
        FP(8, 15, 1, 1, EYE);
        FP(9, 14, 2, 1, EYE);
        FP(11, 15, 1, 1, EYE);
        FP(17, 15, 1, 1, EYE);
        FP(18, 14, 2, 1, EYE);
        FP(20, 15, 1, 1, EYE);
    }
    if (pose == POSE_LEVELUP) {          /* cool shades for the celebration: black, with the glints */
        const uint16_t SH = EO_RGB(8, 8, 12);
        FP(5, 12, 19, 1, SH);            /* the frame across, and the arms to the headphones */
        FP(6, 13, 7, 3, SH);
        FP(15, 13, 7, 3, SH);
        FP(7, 16, 5, 1, SH);
        FP(16, 16, 5, 1, SH);
        FP(8, 17, 3, 1, SH);
        FP(17, 17, 3, 1, SH);
        FP(13, 13, 2, 1, SH);            /* the bridge */
        FP(7, 13, 1, 1, WHITE);          /* glints, in a checker */
        FP(9, 13, 1, 1, WHITE);
        FP(8, 14, 1, 1, WHITE);
        FP(10, 14, 1, 1, WHITE);
        FP(16, 13, 1, 1, WHITE);
        FP(18, 13, 1, 1, WHITE);
        FP(17, 14, 1, 1, WHITE);
        FP(19, 14, 1, 1, WHITE);
    }
    /* the mouth */
    if (pose == POSE_SING || pose == POSE_LEVELUP || pose == POSE_TALK || (pose == POSE_DANCE && open_mouth)) {
        if (open_mouth || pose == POSE_LEVELUP) {
            FP(12, 19, 4, 3, MOUTH);
            FP(13, 21, 2, 1, TONGUE);
        } else {
            FP(13, 19, 2, 2, MOUTH);
        }
    } else if (pose == POSE_WIN || pose == POSE_DANCE) {
        FP(11, 19, 1, 1, MOUTH);
        FP(12, 20, 1, 1, MOUTH);
        FP(13, 21, 2, 1, MOUTH);
        FP(15, 20, 1, 1, MOUTH);
        FP(16, 19, 1, 1, MOUTH);
    } else if (pose == POSE_FAIL) {
        FP(12, 21, 1, 1, MOUTH);
        FP(13, 20, 2, 1, MOUTH);
        FP(15, 21, 1, 1, MOUTH);
    } else {
        FP(12, 19, 1, 1, MOUTH);
        FP(13, 20, 2, 1, MOUTH);
        FP(15, 19, 1, 1, MOUTH);
    }
#undef FP
    /* the extras: notes float up while it dances or sings; sparkles and confetti for a win */
    if (pose == POSE_DANCE || pose == POSE_SING) {
        cat_note(ox + SCL(0), oy + SCL(36 - 6 * f4), (fr & 1u) ? EO_RGB(238, 130, 222) : EO_RGB(250, 110, 100));
        cat_note(ox + SCL(90), oy + SCL(24 + 6 * ((f4 + 2) & 3)), (fr & 1u) ? EO_RGB(250, 110, 100) : EO_RGB(238, 130, 222));
    } else if (pose == POSE_WIN || pose == POSE_LEVELUP) {
        static const int8_t SP[5][2] = {{2, 14}, {90, 10}, {4, 64}, {88, 60}, {46, 0}};
        for (k = 0; k < (pose == POSE_LEVELUP ? 5 : 4); k++)
            if (((fr + (uint32_t)k) & 1u) == 0u)
                cat_spark(ox + SCL(SP[k][0]), oy + SCL(SP[k][1]), k & 1 ? WHITE : GOLD);
        if (pose == POSE_LEVELUP) {      /* the song: notes flying out of its mouth, up and away */
            cat_note(ox + SCL(20 + 12 * (int32_t)(fr & 3u)), oy + SCL(70 - 14 * (int32_t)(fr & 3u)), EO_RGB(255, 210, 60));
            cat_note(ox + SCL(66 - 10 * (int32_t)(fr & 3u)), oy + SCL(64 - 12 * (int32_t)((fr + 2u) & 3u)), EO_RGB(238, 130, 222));
        }
        if (pose == POSE_LEVELUP)        /* confetti: little squares falling, from a fixed pattern of columns */
            for (k = 0; k < 10; k++) {
                int32_t cx = (k * 37 + 11) % 96, cy = (int32_t)((fr * 9u + (uint32_t)k * 23u) % 86u);
                static const uint16_t CF[4] = {EO_RGB(250, 110, 100), EO_RGB(120, 190, 255), EO_RGB(255, 210, 60),
                                               EO_RGB(238, 130, 222)};
                cv_rect(ox + SCL(cx), oy + SCL(cy), 3, 3, CF[k & 3]);
            }
    } else if (pose == POSE_FAIL) {      /* a little sweat drop that slides down */
        cv_rect(ox + SCL(80), oy + SCL(24 + 6 * (int32_t)(fr & 3u)), 3, 5, TEAR);
    }
}

static int eo_round_screen(void)         /* Copycat's clean round screen: the cat, YOUR TURN and the note dashes */
{
    return set.game == GAME_COPYCAT && (ui.state == ST_LISTEN || ui.state == ST_TURN || ui.state == ST_WIN ||
                                        ui.state == ST_MISS || ui.state == ST_GAP);
}
#define RCAT_X 0                         /* the round screen's cat: centred, big (4 pixels per bitmap pixel) */
#define RCAT_Y 24
#define RCAT_W 240
#define RCAT_H 122

static void draw_cat_only(uint32_t now)  /* one animation frame: only the cat's own picture is redrawn */
{
    uint32_t pose = eo_pose(now);
    if (eo_round_screen()) {
        cat_sc = 4;
        cat_mx = 64;
        cat_my = 12;
        cv_begin(RCAT_W, RCAT_H, C_BLACK);
        cat_paint(0, 0, pose, eo_frame(now, pose));
        cv_blit(RCAT_X, RCAT_Y);
        cat_sc = 3;
        cat_mx = 8;
        return;
    }
    cv_begin(CAT_W, CAT_H, C_BLACK);
    cat_paint(0, 0, pose, eo_frame(now, pose));
    cv_blit(CAT_X, 36 + CAT_Y);
}

static void rtext(int32_t y, const felucca_font_t *f, const char *s, uint16_t c)   /* centred in the right column */
{
    int32_t x = 104 + (136 - text_w(f, s)) / 2;
    cv_text(x < 104 ? 104 : x, y, f, s, c);
}

/* the contour crutch: how many of the melody's notes have been heard so far this round (in LISTEN), and which one sounds */
static uint32_t eo_heard(int32_t *cur)
{
    uint32_t i, p = seq_pos, h = 0;
    *cur = -1;
    if (ui.state != ST_LISTEN || !ans_len)
        return 0u;
    for (i = 0; i < seq_n && i < SEQ_MAX; i++) {
        if (p >= seq_at[i] && i < ans_len)
            h = i + 1u;
        if (seq_on && p >= seq_at[i] && p < (uint32_t)seq_at[i] + seq_len[i])
            *cur = (int32_t)(i % ans_len);
    }
    return seq_n > ans_len && p >= seq_at[ans_len] ? ans_len : h;   /* the second hearing: all of it is there */
}

/* word-wrapped text in a column; returns the y below the last line */
static uint8_t wrap_dry;                 /* measuring only: wrap_text draws nothing */
static int32_t wrap_text(int32_t x, int32_t y, int32_t w, const felucca_font_t *f, const char *s, uint16_t c, int32_t lh)
{
    while (*s == ' ')
        s++;
    while (*s) {
        char line[64];
        uint32_t len = 0;
        const char *p = s, *end = s;
        for (;;) {                       /* add words while the line still fits */
            const char *e = p;
            char cand[64];
            uint32_t m = 0, k;
            while (*e && *e != ' ')
                e++;
            for (k = 0; k < len && m < 62u; k++)
                cand[m++] = line[k];
            if (len && m < 62u)
                cand[m++] = ' ';
            for (; p < e && m < 62u; p++)
                cand[m++] = *p;
            cand[m] = 0;
            if (len && text_w(f, cand) > w)
                break;                   /* the word goes to the next line */
            for (k = 0; k <= m; k++)
                line[k] = cand[k];
            len = m;
            end = p;
            while (*end == ' ')
                end++;
            p = end;
            if (!*p)
                break;
        }
        if (!wrap_dry)
            cv_text(x, y, f, line, c);
        y += lh;
        s = end;
    }
    return y;
}

/* the first screens, on every start: the cat explains the game. Any key goes on; HOME skips to the game. */
static void intro_paint(int32_t oy, uint32_t now)
{
    int32_t y;
    cat_sc = 3;
    cat_mx = 8;
    cat_my = 12;
    cat_paint(8, 8 + oy, POSE_TALK, eo_frame(now, POSE_TALK));
    cv_text(110, 22 + oy, &FONT_M, "Oh. A human.", C_WHITE);
    y = wrap_text(110, 54 + oy, 126, &FONT_S, "I'm Meow. I have perfect pitch. You... probably don't.", C_AMB, 17);
    y = (y > 112 + oy ? y : 112 + oy) + 6;
    y = wrap_text(10, y, 220, &FONT_S, "I play a tune. You play it back. Simple.", C_WHITE, 17);
    y = wrap_text(10, y + 2, 220, &FONT_S, "Relax. I'll only judge a little.", C_WHITE, 17);
    y = wrap_text(10, y + 6, 220, &FONT_XS, "In a hurry? HOME skips my speech. Rude, but fine.", C_GRAY, 15);
    if ((now / 500u) & 1u)
        cv_text((240 - text_w(&FONT_B, "PRESS ANY KEY")) / 2, 218 + oy, &FONT_B, "PRESS ANY KEY", C_HI);
}

/* the controls, three screens, each with the cat's line and what the controls do, in its voice */
typedef struct { const char *name, *text; } eo_ctl_t;
static const char *const EO_CTL_SAY[3] = {"Pay attention. I'm only saying this once.", "Four knobs. Try to keep up.",
                                          "Almost done. Don't fall asleep."};
static const eo_ctl_t EO_CTL0[] = {
    {"PLAY", "Start the game. Hit it mid-round and I'll sing it again. Sigh."},
    {"SELECT", "Twist the knob to hear my song, one note at a time. Cheating? Nyaah, I won't tell. Before a round starts, it picks my root note. Meow."},
    {"HOME", "Run away. I won't judge. (I will.)"},
};
static const eo_ctl_t EO_CTL1[] = {
    {"KNOB 1", "Pick a level. No skipping, Bub!"},
    {"KNOB 2", "Pick a voice. Meow is best."},
    {"KNOB 3", "How fast I sing. Slow is no shame."},
    {"KNOB 4", "Pick a game mode. Practice: I'll be nice, try again till you get my song right. Challenge: mess up and I remember."},
};
static const eo_ctl_t EO_CTL2[] = {
    {"PRESETS", "Pick a game. Choose wisely. Choose Copycat."},
    {"SAVE", "Remember me. Please."},
    {"VOLUME", "Louder? Quieter? Come on, birdbrain."},
};

/* the items' height at a font (names in a column, texts beside them); draws too unless wrap_dry */
static int32_t controls_items(const eo_ctl_t *it, uint32_t n, int32_t y, const felucca_font_t *f, int32_t lh)
{
    uint32_t i;
    for (i = 0; i < n; i++) {
        int32_t y2;
        if (!wrap_dry) {
            char nm[16];
            uint32_t k = 0;
            const char *p2 = it[i].name;
            while (*p2 && *p2 != '\n' && k < 15u)
                nm[k++] = *p2++;
            nm[k] = 0;
            cv_text(6, y, f, nm, C_AMB);
            if (*p2 == '\n')                 /* a name of two lines: SELECT, then KNOB */
                cv_text(6, y + lh, f, p2 + 1, C_AMB);
        }
        y2 = wrap_text(70, y, 166, f, it[i].text, C_WHITE, lh);
        y = y2 + 7;
    }
    return y;
}

static void controls_paint(int32_t oy, uint32_t now)
{
    const eo_ctl_t *it = ui.ctl_page == 0u ? EO_CTL0 : ui.ctl_page == 1u ? EO_CTL1 : EO_CTL2;
    uint32_t n = ui.ctl_page == 0u ? sizeof EO_CTL0 / sizeof EO_CTL0[0] : ui.ctl_page == 1u ? sizeof EO_CTL1 / sizeof EO_CTL1[0]
                                                                                              : sizeof EO_CTL2 / sizeof EO_CTL2[0];
    const felucca_font_t *f = &FONT_S;
    int32_t lh = 17;
    cat_sc = 2;
    cat_mx = 0;
    cat_my = 0;
    cat_paint(6, 4 + oy, POSE_TALK, eo_frame(now, POSE_TALK));
    cat_sc = 3;
    cat_mx = 8;
    cat_my = 12;
    wrap_text(70, 12 + oy, 166, &FONT_S, EO_CTL_SAY[ui.ctl_page], C_WHITE, 17);
    wrap_dry = 1;                        /* a bigger font if it fits, the smaller if not */
    if (controls_items(it, n, 0, f, lh) > 150) {
        f = &FONT_XS;
        lh = 15;
    }
    wrap_dry = 0;
    controls_items(it, n, 66 + oy, f, lh);
    if ((now / 500u) & 1u)
        cv_text((240 - text_w(&FONT_B, "PRESS ANY KEY")) / 2, 220 + oy, &FONT_B, "PRESS ANY KEY", C_HI);
}

static void draw_narration(uint32_t now)
{
    uint32_t b;
    for (b = 0; b < 2u; b++) {           /* two bands, the whole screen */
        cv_begin(240, 120, C_BLACK);
        if (ui.state == ST_INTRO)
            intro_paint(-120 * (int32_t)b, now);
        else
            controls_paint(-120 * (int32_t)b, now);
        cv_blit(0, 120 * b);
    }
}

/* the ready screen's facts, along the foot: the notes to choose from, the tune, what is new, and what is switched on */
static void cc_facts(void)
{
    char t[64], n[12];
    const copycat_level_t *lv = copycat_level(eo_level());
    t[0] = 0;
    text_cat(t, eo_cc_pool(lv->n));
    cv_text(14, 0, &FONT_XS, t, C_GRAY);
    t[0] = 0;
    text_cat(t, "Tunes are ");
    int_text((int32_t)lv->len, n);
    text_cat(t, n);
    text_cat(t, " notes long");
    cv_text(14, 14, &FONT_XS, t, C_GRAY);
    t[0] = 0;
    if (lv->intro)
        text_cat(t, "A new note in every tune");
    else if (lv->min_leaps)
        text_cat(t, lv->max_leaps > 1u ? "1 to 2 leaps in every tune" : "A leap in every tune");
    if (lv->chroma != CHROMA_NONE) {
        if (t[0])
            text_cat(t, ", ");
        text_cat(t, lv->chroma_max > 1u ? "1 to 2 chromatic notes" : "1 chromatic note");
    }
    if (!t[0])
        text_cat(t, lv->start_root ? "Tunes start on the root" : "Steps and thirds");
    cv_text(14, 28, &FONT_XS, t, C_WHITE);
    t[0] = 0;
    text_cat(t, lv->lit == LIT_ALL ? "Lights on" : lv->lit == LIT_CHROMATIC ? "Chroma lit" : "Lights off");
    text_cat(t, lv->drone ? "  -  Drone on" : "  -  Drone off");
    if (lv->contour)
        text_cat(t, "  -  Shape shown");
    else if (lv->start_root)
        text_cat(t, "  -  Starts on root");
    cv_text(14, 42, &FONT_XS, t, C_DIM);
}

/* Chord Dogtor's screen: the dog at 3x on a clinical teal, his words along the top, the answers along the bottom. Each
 * of the screen's three bands paints the whole dog at its own offset; the canvas clips. */
#define DT_TEAL EO_RGB(70, 110, 120)
#define DT_DARK EO_RGB(14, 28, 34)
#define DT_Y 12
static void vol_overlay(int32_t y, uint32_t now, uint32_t pct);
static void dog_paint(int32_t ox, int32_t oy, uint32_t pose)
{
    int32_t x, y;
    for (y = 0; y < EO_DOG_H; y++) {
        if (oy + y * 3 > 150 || oy + y * 3 + 3 < -4)    /* not in this band: skip the row */
            continue;
        for (x = 0; x < EO_DOG_W;) {
            char c = EO_DOG[pose][y][x];
            int32_t e = x + 1;
            while (e < EO_DOG_W && EO_DOG[pose][y][e] == c)
                e++;
            if (c != '.')
                cv_rect(ox + x * 3, oy + y * 3, (e - x) * 3, 3, eo_dog_col(c));
            x = e;
        }
    }
}

static uint32_t dt_pose(uint32_t now)
{
    if (ui.state == ST_WIN && (eo_win_pending || (seq_on && seq_pos < 380u)))
        return DOG_LISTEN;               /* the bell first: he reacts as the howl starts */
    if (ui.state == ST_LEVELUP || (ui.state == ST_WIN) || (ui.state == ST_GAP && ui.result == 1))
        return DOG_WIN;
    if (ui.state == ST_MISS || (ui.state == ST_GAP && ui.result == 2))
        return eo_miss_run == 1u ? DOG_SHOCK : DOG_WORRY;
    if (ui.state == ST_LISTEN)
        return DOG_LISTEN;
    if (ui.state == ST_TURN)     /* alert while you decide; a long wait: furrowed, then a yawn */
        return now - dt_idle_at > 14000u ? DOG_YAWN : now - dt_idle_at > 8000u ? DOG_THINK : DOG_LISTEN;
    return DOG_IDLE;
}

static void dt_chip(int32_t x, int32_t y, uint32_t i, uint32_t sel, uint32_t ok)
{
    char k[2];
    uint16_t bg = ok ? C_AMB : DT_DARK, fg = ok ? C_BLACK : C_WHITE;
    cv_rect(x, y, 55, 30, ok ? C_AMB : sel ? C_GRAY : C_WHITE);
    cv_rect(x + 1, y + 1, 53, 28, bg);
    k[0] = (char)('A' + i);
    k[1] = 0;
    cv_text(x + 27 - text_w(&FONT_B, k) / 2, y + 1, &FONT_B, k, ok ? C_BLACK : C_AMB);
    cv_text(x + 27 - text_w(&FONT_XS, dt_q_name[dogtor_level(eo_level())->q[i]]) / 2, y + 17, &FONT_XS,
            dt_q_name[dogtor_level(eo_level())->q[i]], fg);
}

static void dt_band(int32_t band_y, uint32_t now, uint32_t pose)   /* the dog, then the words that belong to this band */
{
    int32_t bob = (int32_t)((now / 600u) & 1u) * 3;
    uint32_t i, n = dogtor_level(eo_level())->n;
    char t[40], b[12];
    dog_paint(48, DT_Y + bob - band_y, pose);
    if (band_y == 0) {                       /* the header: whose game this is */
        cv_text(6, 4, &FONT_S, "CHORD DOGTOR", C_WHITE);
        {   /* top right: a coffee cup and how many are left (steam while there are some) */
            char cb[8];
            uint16_t cc = dt_cups ? C_WHITE : C_GRAY;
            cv_rect(188, 8, 11, 10, cc);
            cv_rect(199, 10, 3, 6, cc);
            cv_rect(200, 12, 1, 2, DT_TEAL);
            cv_rect(186, 18, 15, 2, C_GRAY);
            if (dt_cups) {
                int32_t sh = (int32_t)((now / 500u) & 1u);
                cv_rect(191 + sh, 2, 1, 4, C_GRAY);
                cv_rect(195 - sh, 3, 1, 4, C_GRAY);
            }
            cb[0] = 'x';
            int_text((int32_t)dt_cups, cb + 1);
            cv_text(206, 4, &FONT_B, cb, dt_cups ? C_AMB : C_GRAY);
        }
    } else if (band_y == 36) {               /* the margins: the level, and how far to the next */
        t[0] = 0;
        text_cat(t, "LV ");
        int_text((int32_t)eo_level(), b);
        text_cat(t, b);
        cv_text(4, 60, &FONT_B, t, C_WHITE);
        t[0] = 0;
        int_text((int32_t)set.prog[GAME_DOGTOR], b);
        text_cat(t, b);
        text_cat(t, "/");
        int_text((int32_t)DOGTOR_ROUNDS, b);
        text_cat(t, b);
        cv_text(236 - text_w(&FONT_B, t), 60, &FONT_B, t, C_AMB);
        t[0] = 0;
        text_cat(t, "STREAK ");
        int_text((int32_t)streak, b);
        text_cat(t, b);
        cv_text(236 - text_w(&FONT_XS, t), 82, &FONT_XS, t, C_WHITE);
    } else {                                 /* the foot: the answers, or what to press */
        int32_t y0 = 28;
        if (ui.state == ST_READY || ui.state == ST_LEVELUP) {
            cv_rect(4, y0, 232, 30, C_WHITE);
            cv_rect(5, y0 + 1, 230, 28, DT_DARK);
            t[0] = 0;
            text_cat(t, ui.state == ST_LEVELUP ? "NEW LEVEL: " : "LEVEL ");
            int_text((int32_t)eo_level(), b);
            if (ui.state == ST_READY) {
                text_cat(t, b);
                text_cat(t, ": ");
            }
            text_cat(t, dogtor_level(eo_level())->name);
            cv_text(10, y0 + 2, &FONT_S, t, C_AMB);
            cv_text(10, y0 + 16, &FONT_XS, ui.state == ST_LEVELUP ? "PLAY CONTINUE     HOME STOP" : "PLAY START   K1 LEVEL   PRESETS GAME",
                    C_GRAY);
        } else {
            int32_t x0 = (240 - ((int32_t)n * 59 - 4)) / 2;
            for (i = 0; i < n; i++)
                dt_chip(x0 + (int32_t)i * 59, y0, i, (ui.state == ST_MISS || ui.result == 2) && i == dt_pressed_ix,
                        (ui.state == ST_WIN || ui.state == ST_MISS || ui.state == ST_GAP) && i == dt_choice_now);
        }
    }
}

static void dt_card_band(int32_t band_y)   /* a silent film's title card: black, a double frame, his words */
{
    const char *l1, *l2, *tag = "~ DR. DIGGIE ~";
    int32_t y;
    if (dt_line_set == DS_STORY) {
        uint32_t lv = dt_line_lv >= 1u && dt_line_lv <= DOGTOR_LEVELS ? dt_line_lv : 1u;
        l1 = dt_story[lv - 1u][dt_line_n & 7u][0];
        l2 = dt_story[lv - 1u][dt_line_n & 7u][1];
    } else if (dt_line_set == DS_CLIFF) {
        l1 = dt_cliff[dt_line_n % (DOGTOR_LEVELS - 1u)][0];
        l2 = dt_cliff[dt_line_n % (DOGTOR_LEVELS - 1u)][1];
        tag = "TO BE CONTINUED...";
    } else if (dt_line_set == DS_COFFEE) {
        l1 = "Rough shift, doctor.";
        l2 = "Here. Have a coffee.";
        tag = "COFFEE +1";
    } else {
        l1 = dt_say[dt_line_set][dt_line_n][0];
        l2 = dt_say[dt_line_set][dt_line_n][1];
    }
    cv_rect(6, 6 - band_y, 228, 228, C_WHITE);
    cv_rect(8, 8 - band_y, 224, 224, C_BLACK);
    cv_rect(12, 12 - band_y, 216, 216, C_GRAY);
    cv_rect(13, 13 - band_y, 214, 214, C_BLACK);
    y = 78 - band_y;
    cv_text(120 - text_w(&FONT_B, l1) / 2, y, &FONT_B, l1, C_WHITE);
    cv_text(120 - text_w(&FONT_B, l2) / 2, y + 26, &FONT_B, l2, C_WHITE);
    if (tag[0] == '~') {                     /* the plain tag also says how many coffees are left */
        char ct[40], cb[8];
        ct[0] = 0;
        text_cat(ct, tag);
        text_cat(ct, "   COFFEE x");
        int_text((int32_t)dt_cups, cb);
        text_cat(ct, cb);
        cv_text(120 - text_w(&FONT_XS, ct) / 2, 176 - band_y, &FONT_XS, ct, C_AMB);
    } else {
        cv_text(120 - text_w(&FONT_XS, tag) / 2, 176 - band_y, &FONT_XS, tag, C_AMB);
    }
}

static void draw_dogtor(uint32_t now, uint32_t pct)
{
    uint32_t pose = dt_pose(now);
    if (dt_card_until && now < dt_card_until) {
        cv_begin(240, 36, C_BLACK);
        dt_card_band(0);
        cv_blit(0, 0);
        cv_begin(240, 144, C_BLACK);
        dt_card_band(36);
        cv_blit(0, 36);
        cv_begin(240, 60, C_BLACK);
        dt_card_band(180);
        cv_blit(0, 180);
        return;
    }
    cv_begin(240, 36, DT_TEAL);
    dt_band(0, now, pose);
    cv_blit(0, 0);
    cv_begin(240, 144, DT_TEAL);
    dt_band(36, now, pose);
    cv_blit(0, 36);
    cv_begin(240, 60, DT_TEAL);
    dt_band(180, now, pose);
    vol_overlay(2, now, pct);
    cv_blit(0, 180);
}

/* Copycat's middle band: the cat on the left, the words on the right, hints along the bottom */
static void cc_band(uint32_t now)
{
    char t[48], n[12];
    uint32_t i, tx = eo_text(), pose = eo_pose(now);
    const copycat_level_t *lv = copycat_level(eo_level());
    cat_paint(CAT_X, CAT_Y, pose, eo_frame(now, pose));
    if (ui.state == ST_LEVELUP) {
        const char *const *ln = eo_up_text[GAME_COPYCAT][eo_level()];
        t[0] = 0;
        text_cat(t, "LEVEL ");
        int_text((int32_t)eo_level(), n);
        text_cat(t, n);
        if (now < ui.up_until) {         /* the celebration: flashing words, then the card */
            rtext(10, &FONT_M, "LEVEL UP!", (eo_frame(now, pose) & 1u) ? C_HI : C_AMB);
            rtext(44, &FONT_L, t, C_WHITE);
            t[0] = 0;
            text_cat(t, "OPEN: LEVELS 1 TO ");
            int_text((int32_t)eo_open(GAME_COPYCAT), n);
            text_cat(t, n);
            ctext(112, &FONT_XS, t, C_GRAY);
        } else {
            rtext(6, &FONT_M, TX[TX_GOOD], C_HI);
            rtext(36, &FONT_L, t, C_WHITE);
            rtext(68, &FONT_XS, "PRESS PLAY", C_DIM);
            rtext(82, &FONT_XS, "TO CONTINUE", C_DIM);
            for (i = 0; i < 3u && ln[i]; i++)
                ctext(98 + (int32_t)i * 15, &FONT_XS, ln[i], i ? C_GRAY : C_WHITE);
        }
        return;
    }
    if (ui.state == ST_READY) {          /* the ready screen: one thing to do, then what this level is */
        rtext(22, &FONT_M, "PRESS PLAY", C_HI);
        rtext(44, &FONT_M, "TO START.", C_HI);
        rtext(68, &FONT_S, "Meow.", C_WHITE);
        t[0] = 0;
        if (set.mode)
            text_cat(t, "PRACTICE");
        if (set.speed != SPEED_DEF) {
            if (t[0])
                text_cat(t, "  ");
            int_text((int32_t)set.speed * 5, n);
            text_cat(t, n);
            text_cat(t, "%");
        }
        rtext(84, &FONT_XS, t, C_DIM);
        ctext(98, &FONT_XS, "Press SEL to see controls again", C_DIM);
        cv_rect(14, 114, 212, 1, C_LINE);
        cv_text(14, 120, &FONT_B, eo_cc_name[eo_level() < COPYCAT_LEVELS ? eo_level() : COPYCAT_LEVELS], C_HI);
        return;
    }
    t[0] = 0;
    text_cat(t, "COPYCAT   LEVEL ");
    int_text((int32_t)eo_level(), n);
    text_cat(t, n);
    rtext(2, &FONT_XS, t, C_GRAY);
    t[0] = 0;
    text_cat(t, lv->lit == LIT_ALL ? "LIGHTS ON" : lv->lit == LIT_CHROMATIC ? "CHROMATIC LIT" : "NO LIGHTS");
    if (set.speed != SPEED_DEF) {
        text_cat(t, "  ");
        int_text((int32_t)set.speed * 5, n);
        text_cat(t, n);
        text_cat(t, "%");
    }
    rtext(17, &FONT_XS, t, C_GRAY);
    rtext(33, &FONT_M, TX[tx], (tx == TX_ROOT || tx == TX_LISTEN || tx == TX_NICE) ? C_HI : C_WHITE);
    if (ans_len && ui.state != ST_READY && lv->contour) {   /* the melody's shape, rising out of the cat's mouth */
        int32_t pitch = 136 / (int32_t)ans_len > 24 ? 24 : 136 / (int32_t)ans_len, cur;
        int32_t x = 104, lo = 127, hi = 0;
        uint32_t heard = eo_heard(&cur), all = ui.state != ST_LISTEN;
        for (i = 0; i < ans_len; i++) {
            lo = ans[i] < lo ? ans[i] : lo;
            hi = ans[i] > hi ? ans[i] : hi;
        }
        for (i = 0; i < ans_len; i++) {
            int32_t y = hi > lo ? 66 - (ans[i] - lo) * 12 / (hi - lo) : 60;
            uint16_t c = C_LINE;
            if (!all && i >= heard)
                continue;                /* not heard yet: the notes come out one at a time */
            if ((int32_t)i == cur)
                c = C_AMB;
            else if ((ui.state == ST_TURN || ui.state == ST_WIN) && i < ans_pos)
                c = C_HI;
            else if (all)
                c = C_GRAY;
            else
                c = C_WHITE;
            cv_rect(x + (int32_t)i * pitch, y, pitch - 6, 5, c);
            if (i && (all || i < heard)) {   /* a line from the last note, so it reads as a shape */
                int32_t py = hi > lo ? 66 - (ans[i - 1u] - lo) * 12 / (hi - lo) : 60;
                cv_rect(x + (int32_t)i * pitch - 6, (py < y ? py : y) + 2, 6, 1, C_LINE);
            }
        }
    } else if (ans_len && ui.state != ST_READY) {   /* one box per note: filled as you play them right */
        int32_t pitch = 136 / (int32_t)ans_len > 18 ? 18 : 136 / (int32_t)ans_len, w = (int32_t)ans_len * pitch - 4;
        int32_t x = 104 + (136 - w) / 2;
        for (i = 0; i < ans_len; i++)
            cv_rect(x + (int32_t)i * pitch, 62, pitch - 4, 10,
                    (ui.state == ST_TURN || ui.state == ST_WIN) && i < ans_pos ? C_HI : C_LINE);
    }
    t[0] = 0;
    if (set.mode) {                      /* practice: which try at this melody */
        text_cat(t, "PRACTICE  TRY ");
        int_text((int32_t)eo_tries, n);
        text_cat(t, n);
    } else {
        text_cat(t, "STREAK ");
        int_text((int32_t)streak, n);
        text_cat(t, n);
        text_cat(t, "   BEST ");
        int_text(set.best[GAME_COPYCAT], n);
        text_cat(t, n);
    }
    rtext(76, &FONT_S, t, C_AMB);
    t[0] = 0;                            /* the root, and the way to the next level */
    if (eo_moves()) {
        text_cat(t, "ROOT MOVES   ");
    } else {
        note_name(KEY_NOTE0 + (int32_t)set.root, n);
        text_cat(t, "ROOT ");
        text_cat(t, n);
        text_cat(t, "   ");
    }
    if (set.mode) {
        text_cat(t, "NOTHING COUNTS");
    } else if (eo_level() >= COPYCAT_LEVELS) {
        text_cat(t, "TOP LEVEL");
    } else {
        text_cat(t, "LEVEL UP ");
        int_text(set.prog[GAME_COPYCAT], n);
        text_cat(t, n);
        text_cat(t, "/");
        int_text((int32_t)eo_rounds(GAME_COPYCAT), n);
        text_cat(t, n);
    }
    ctext(98, &FONT_XS, t, C_DIM);
    if (ui.state == ST_TURN) {
        ctext(113, &FONT_XS, "PLAY AGAIN  SELECT STEP  HOME STOP", C_DIM);
    } else {
        ctext(113, &FONT_XS, "HOME STOP", C_DIM);
    }
}


static void vol_overlay(int32_t y, uint32_t now, uint32_t pct)   /* the volume, for a moment after it changes */
{
    char t[12];
    if (now >= ui.vol_until)
        return;
    cv_text(178, y, &FONT_XS, "VOL", C_GRAY);
    int_text((int32_t)pct, t);
    text_cat(t, "%");
    cv_text(204, y, &FONT_XS, t, C_HI);
}

static void keyboard_paint(uint32_t bright, uint32_t mid, uint32_t dim)   /* the keys, black above white, in LED levels */
{
    static const uint8_t WK[16] = PLAT_WHITE_KEYS, BK[11] = PLAT_BLACK_KEYS;
    static const uint8_t AT[11] = {0, 1, 2, 4, 5, 7, 8, 9, 11, 12, 14};   /* the white key each black key follows */
    uint32_t i;
    for (i = 0; i < 16u; i++)
        key_cell(4 + (int32_t)i * 14, 14, 13, 14, WK[i], C_LINE, bright, mid, dim);
    for (i = 0; i < 11u; i++)
        key_cell(4 + (int32_t)AT[i] * 14 + 8, 0, 11, 12, BK[i], C_LINE, bright, mid, dim);
}

/* Copycat during a round: the mode and level, the cat (the main thing), what it says, YOUR TURN with the note dashes,
 * and the streak. The root and the way to the next level show only in the pause between rounds; the keyboard only
 * slides in after a miss with the lights off, with the answer lit. */
static void draw_round(uint32_t now, uint32_t pct)
{
    char t[48], n[12];
    uint32_t i, tx = eo_text(), bright, mid, dim;
    const copycat_level_t *lv = copycat_level(eo_level());
    int32_t kh = 0, lh;
    key_levels(&bright, &mid, &dim);
    cv_begin(240, 24, C_BLACK);          /* the mode and level, or a message for a moment (voice, speed, mode changed) */
    if (now < ui.msg_until && !eo_streq(ui.msg[0], "YOUR TURN") && !eo_streq(ui.msg[0], "LISTEN") &&
        !eo_streq(ui.msg[0], "ROOT") && !eo_streq(ui.msg[0], "LEVEL") && !eo_streq(ui.msg[0], "TRY AGAIN")) {
        int32_t x = cv_text(10, 4, &FONT_B, ui.msg[0], C_WHITE);
        cv_text(x + 8, 4, &FONT_B, ui.msg[1], C_HI);
    } else {
        t[0] = 0;
        text_cat(t, set.mode ? "PRACTICE" : "CHALLENGE");
        text_cat(t, "   LEVEL ");
        int_text((int32_t)eo_level(), n);
        text_cat(t, n);
        text_cat(t, lv->lit == LIT_ALL ? "   LIGHTS ON" : lv->lit == LIT_CHROMATIC ? "   CHROMATIC LIT" : "   NO LIGHTS");
        if (set.speed != SPEED_DEF) {
            text_cat(t, "   ");
            int_text((int32_t)set.speed * 5, n);
            text_cat(t, n);
            text_cat(t, "%");
        }
        ctext(5, &FONT_XS, t, C_GRAY);
    }
    cv_blit(0, 0);
    draw_cat_only(now);
    if (ui.kb_show) {                    /* the keyboard slides up from the bottom edge */
        uint32_t el = now - ui.kb_at;
        kh = el >= 300u ? 30 : (int32_t)(el * 30u / 300u);
    }
    lh = 94 - kh;
    cv_begin(240, (uint32_t)(lh > 1 ? lh : 1), C_BLACK);
    if (ui.say[0] && now < ui.say_until)     /* the cat speaks */
        ctext(text_w(&FONT_S, ui.say) <= 236 ? 2 : 4, text_w(&FONT_S, ui.say) <= 236 ? &FONT_S : &FONT_XS, ui.say, C_WHITE);
    ctext(20, &FONT_M, eo_word(tx), (tx == TX_ROOT || tx == TX_LISTEN || tx == TX_NICE) ? C_HI : C_WHITE);
    if (ans_len) {
        if (lv->contour) {               /* the melody's shape: each note a dash, high or low, appearing as it is heard */
            int32_t pitch = 224 / (int32_t)ans_len > 26 ? 26 : 224 / (int32_t)ans_len, cur;
            int32_t x0 = (240 - (int32_t)ans_len * pitch) / 2, lo = 127, hi = 0;
            uint32_t heard = eo_heard(&cur), all = ui.state != ST_LISTEN;
            for (i = 0; i < ans_len; i++) {
                lo = ans[i] < lo ? ans[i] : lo;
                hi = ans[i] > hi ? ans[i] : hi;
            }
            for (i = 0; i < ans_len; i++) {
                int32_t y = hi > lo ? 58 - (ans[i] - lo) * 12 / (hi - lo) : 52;
                uint16_t c;
                if (!all && i >= heard)
                    continue;
                if ((int32_t)i == cur)
                    c = C_AMB;
                else if ((ui.state == ST_TURN || ui.state == ST_WIN) && i < ans_pos)
                    c = C_HI;
                else
                    c = all ? C_GRAY : C_WHITE;
                cv_rect(x0 + (int32_t)i * pitch, y, pitch - 8, 5, c);
                if (i && (all || i < heard)) {
                    int32_t py = hi > lo ? 58 - (ans[i - 1u] - lo) * 12 / (hi - lo) : 52;
                    cv_rect(x0 + (int32_t)i * pitch - 8, (py < y ? py : y) + 2, 8, 1, C_LINE);
                }
            }
        } else {                         /* one box per note: filled as you play them right */
            int32_t pitch = 224 / (int32_t)ans_len > 20 ? 20 : 224 / (int32_t)ans_len, w = (int32_t)ans_len * pitch - 4;
            int32_t x = (240 - w) / 2;
            for (i = 0; i < ans_len; i++)
                cv_rect(x + (int32_t)i * pitch, 50, pitch - 4, 10,
                        (ui.state == ST_TURN || ui.state == ST_WIN) && i < ans_pos ? C_HI : C_LINE);
        }
    }
    t[0] = 0;
    if (set.mode) {
        text_cat(t, "PRACTICE   TRY ");
        int_text((int32_t)eo_tries, n);
        text_cat(t, n);
    } else {
        text_cat(t, "STREAK ");
        int_text((int32_t)streak, n);
        text_cat(t, n);
        text_cat(t, "    BEST ");
        int_text(set.best[GAME_COPYCAT], n);
        text_cat(t, n);
    }
    ctext(64, &FONT_S, t, C_AMB);
    if (ui.state == ST_GAP) {            /* in the pause between rounds: the root, and the way to the next level */
        t[0] = 0;
        if (eo_moves()) {
            text_cat(t, "ROOT MOVES");
        } else {
            note_name(KEY_NOTE0 + (int32_t)set.root, n);
            text_cat(t, "ROOT ");
            text_cat(t, n);
        }
        text_cat(t, "      ");
        if (set.mode) {
            text_cat(t, "NOTHING COUNTS");
        } else if (eo_level() >= COPYCAT_LEVELS) {
            text_cat(t, "TOP LEVEL");
        } else {
            text_cat(t, "LEVEL UP ");
            int_text(set.prog[GAME_COPYCAT], n);
            text_cat(t, n);
            text_cat(t, "/");
            int_text((int32_t)eo_rounds(GAME_COPYCAT), n);
            text_cat(t, n);
        }
        ctext(79, &FONT_XS, t, C_GRAY);
    }
    vol_overlay(79, now, pct);
    cv_blit(0, 146);
    if (kh > 0) {
        cv_begin(240, (uint32_t)kh, C_BLACK);
        keyboard_paint(bright, mid, dim);
        cv_blit(0, 240 - (uint32_t)kh);
    }
}


/* ------------------------------------------------- the power-on movie and the game select --- */
/* Power on: ASilagy presents, the title (sound effects only), then the intro music over a montage of the cast, cut to
 * the music's hits, and the title again, waiting for a key. Any key skips to the game select. The picture is drawn in
 * the three bands from one scene function in screen coordinates (mv_oy is the band's top). The cast is placeholders
 * (coloured blobs) except the cat and the dog, until their art arrives. */
typedef struct { const char *game, *cap, *quip; uint8_t r, g, b; } eo_char_t;
static const eo_char_t eo_chars[12] = {      /* in the order of the montage */
    {"COPYCAT", "THE CRITIC", "He will grade your ears.", 246, 152, 56},
    {"CHORD DOGTOR", "THE CLERIC", "Please be okay. Please.", 70, 130, 140},
    {"SIMONE SAYS", "THE CLAIRVOYANT", "Saw this coming. Forgot why.", 255, 180, 50},
    {"PITCH PLEASE", "THE LISTENER", "...", 150, 150, 165},
    {"NAME THAT INTERVAL", "THE SURVEYOR", "Here for the snacks.", 110, 200, 90},
    {"CHORD DETECTIVE", "THE INQUISITOR", "Big Major is watching.", 120, 110, 120},
    {"BEEP SWEEP", "THE SEEKER", "Everything is energy.", 240, 110, 70},
    {"TUNA TUNER", "THE HEAVY", "Sorry, little guy.", 140, 90, 50},
    {"WRONG NOTE", "THE NPC", "Note four was wrong.", 185, 175, 155},
    {"TWEET SHEET", "THE ENSEMBLE", "Woof.", 90, 160, 230},
    {"TRANSPOSE ECHO", "THE G.O.A.T.", "Hydrate!", 235, 235, 225},
    {"SCALE RACER", "THE SWIFT", "Gotta go gotta go.", 220, 70, 70},
};
static const uint16_t MV_B[13] = {0, 820, 1650, 2070, 2380, 2800, 3310, 3730, 3940, 4560, 4980, 5390, 5600};   /* the music's hits */
static int32_t mv_oy, mv_h;
static const int8_t sel_game[6] = {GAME_COPYCAT, GAME_DOGTOR, GAME_SIMON, -1, -1, -1};

static void mrect(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c) { cv_rect(x, y - mv_oy, w, h, c); }
static void mtext(int32_t x, int32_t y, const felucca_font_t *f, const char *s, uint16_t c) { cv_text(x, y - mv_oy, f, s, c); }
static void mctext(int32_t y, const felucca_font_t *f, const char *s, uint16_t c) { mtext((240 - text_w(f, s)) / 2, y, f, s, c); }

static uint32_t eo_strlen(const char *s)               /* (the firmware has no libc) */
{
    uint32_t n = 0;
    while (s[n])
        n++;
    return n;
}

static void mgame(int32_t y, const felucca_font_t *f, const char *s, uint16_t c)   /* a game's name, centred; the font has no e-acute, so SIMONE gets a drawn accent */
{
    int32_t x = (240 - text_w(f, s)) / 2;
    mtext(x, y, f, s, c);
    if (s[0] == 'S' && s[1] == 'I' && s[2] == 'M' && s[3] == 'O' && s[4] == 'N' && s[5] == 'E') {
        int32_t ex = x + text_w(f, "SIMON"), ew = text_w(f, "E");
        mrect(ex + ew / 2, y - 1, 3, 2, c);
        mrect(ex + ew / 2 - 2, y + 1, 3, 2, c);
    }
}

static uint16_t mv_scale(uint16_t c, uint32_t n)         /* a colour at n/8 of its brightness */
{
    uint32_t r = (c >> 11) & 31u, g = (c >> 5) & 63u, b = c & 31u;
    return (uint16_t)(((r * n / 8u) << 11) | ((g * n / 8u) << 5) | (b * n / 8u));
}

static void mdisc(int32_t cx, int32_t cy, int32_t r, uint16_t c)
{
    int32_t dy;
    for (dy = -r; dy <= r; dy++) {
        int32_t y = cy + dy - mv_oy, dx;
        if (y < 0 || y >= mv_h)
            continue;
        dx = (int32_t)fm_sqrtf((float)(r * r - dy * dy));
        cv_rect(cx - dx, y, 2 * dx + 1, 1, c);
    }
}

/* a picture drawn from a pixel reader: the colour (RGB565) at x, y, or -1 for clear; scaled by num/den (nearest) */
typedef int32_t (*spr_px_t)(const void *s, int32_t x, int32_t y);
static const uint16_t *spr_pal;          /* the palette of the cast sprite being drawn */
static int32_t cast_px(const void *s, int32_t x, int32_t y)
{
    uint32_t i = (uint32_t)y * EO_CAST_W + (uint32_t)x, b = ((const uint8_t *)s)[i >> 1], v = (i & 1u) ? b & 15u : b >> 4;
    return v ? (int32_t)spr_pal[v] : -1;
}
static int32_t dog_px(const void *s, int32_t x, int32_t y)
{
    char c = EO_DOG[(uint32_t)(uintptr_t)s][y][x];
    return c == '.' ? -1 : (int32_t)eo_dog_col(c);
}
static uint8_t spr_flip;                 /* paint the sprite mirrored (facing the other way) */
static void spr_paint(spr_px_t px, const void *s, int32_t w, int32_t h, int32_t ox, int32_t oy, int32_t num, int32_t den)
{
    int32_t dh = h * num / den, dy;
    for (dy = 0; dy < dh; dy++) {
        int32_t y = oy + dy - mv_oy, sy = dy * den / num, x = 0;
        if (y < 0 || y >= mv_h)
            continue;
        while (x < w) {
            int32_t c = px(s, spr_flip ? w - 1 - x : x, sy), e = x + 1;
            while (e < w && px(s, spr_flip ? w - 1 - e : e, sy) == c)
                e++;
            if (c >= 0)
                cv_rect(ox + x * num / den, y, e * num / den - x * num / den, 1, (uint16_t)c);
            x = e;
        }
    }
}
static void cast_paint(uint32_t who, uint32_t pose, int32_t ox, int32_t oy, int32_t num, int32_t den)
{
    spr_pal = EO_CAST_PAL[who];
    spr_paint(cast_px, EO_CAST[who][pose], EO_CAST_W, EO_CAST_H, ox, oy, num, den);
}
static void dog_paint_sc(uint32_t pose, int32_t ox, int32_t oy, int32_t num, int32_t den)
{
    spr_paint(dog_px, (const void *)(uintptr_t)pose, EO_DOG_W, EO_DOG_H, ox, oy, num, den);
}

static void mv_montage(uint32_t tm)
{
    uint32_t s = 0, age;
    int32_t k, sh, drop, bob;
    const eo_char_t *c;
    uint16_t a;
    while (s < 11u && tm >= MV_B[s + 1u])
        s++;
    age = tm - MV_B[s];
    c = &eo_chars[s];
    a = EO_RGB(c->r, c->g, c->b);
    sh = (int32_t)(tm / 7u % 48u);
    for (k = -2; k < 11; k++)
        mrect(k * 24 + sh, 0, 24, 240, (k & 1) ? mv_scale(a, 3u) : mv_scale(a, 2u));
    drop = age < 160u ? (int32_t)((160u - age) * (160u - age) / 400u) : 0;
    bob = ((tm / 155u) & 1u) ? 3 : 0;
    if (s == 0u) {
        cast_paint(EO_CAST_CAT, age >= 120u ? 1u : 0u, 24, 38 + bob - drop, 2, 1);
    } else if (s == 1u) {
        dog_paint_sc(((tm / 200u) & 1u) ? DOG_WIN : DOG_LISTEN, 72, 40 + bob - drop, 2, 1);
    } else {                             /* the rest: idle first, then its action (the NPC is slow with his thumb) */
        uint32_t pose = age >= (s == 8u ? 400u : 140u) ? 1u : 0u;
        cast_paint(s - 2u, pose, 24, 38 + bob - drop, 2, 1);
    }
    {
        int32_t cy = 192 + (age < 130u ? (int32_t)(130u - age) / 4 : 0);
        mrect(0, 8, 240, 24, C_BLACK);
        mgame(12, &FONT_S, c->game, a);
        mrect(0, cy - 6, 240, 38, C_BLACK);
        mctext(cy, &FONT_B, c->cap, C_WHITE);
    }
    if (age < 30u)
        mrect(0, 0, 240, 240, C_WHITE);
}

static void art_paint(const uint8_t *rle, const uint16_t *row, const uint16_t *pal, int32_t ox, int32_t oy, uint32_t fade)
{
    int32_t y;
    for (y = 0; y < 240; y++) {          /* a 240 x 240 picture in colour runs; black (0) is the background already */
        int32_t py = oy + y - mv_oy, x = 0;
        const uint8_t *p = rle + row[y], *e = rle + row[y + 1];
        if (py < 0 || py >= mv_h)
            continue;
        while (p < e) {
            if (p[0]) {
                uint16_t c = pal[p[0]];
                cv_rect(ox + x, py, p[1], 1, fade < 8u ? mv_scale(c, fade) : c);
            }
            x += p[1];
            p += 2;
        }
    }
}

/* a dissolve to black: 8 x 8 blocks go dark in a dither order as p goes 0..16 (cheap: a few runs of black per block row) */
static void mv_dissolve(uint32_t p)
{
    static const uint8_t BAYER[16] = {0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5};
    int32_t by, bx;
    for (by = 0; by < 30; by++) {
        if (by * 8 + 8 <= mv_oy || by * 8 >= mv_oy + mv_h)
            continue;                    /* not in this band */
        bx = 0;
        while (bx < 30) {
            int32_t s;
            if (BAYER[(by & 3) * 4 + (bx & 3)] >= p) {
                bx++;
                continue;
            }
            s = bx;
            while (bx < 30 && BAYER[(by & 3) * 4 + (bx & 3)] < p)
                bx++;
            mrect(s * 8, by * 8, (bx - s) * 8, 8, C_BLACK);
        }
    }
}

static void mv_roster(uint32_t tm)                       /* the build-up to the last stab: the whole cast, one by one */
{
    uint32_t tr = tm - MV_ROSTER, i;
    int32_t k, sh = (int32_t)(tm / 3u % 48u);
    for (k = -2; k < 11; k++)
        mrect(k * 24 + sh, 0, 24, 240, (k & 1) ? EO_RGB(30, 24, 52) : EO_RGB(20, 16, 36));
    for (i = 0; i < 12u; i++) {
        uint32_t at = i * 150u, age, pose;
        int32_t x = (int32_t)(i & 3u) * 60 + 2, y = 24 + (int32_t)(i >> 2) * 62, bob = ((tr / 155u) & 1u) ? 2 : 0;
        const eo_char_t *c = &eo_chars[i];
        if (tr < at)
            continue;
        age = tr - at;
        mrect(x, y, 56, 54, age < 30u ? C_WHITE : mv_scale(EO_RGB(c->r, c->g, c->b), 3u));
        if (age < 30u)
            continue;
        pose = age >= (i == 8u ? 400u : 140u) ? 1u : 0u;
        if (i == 0u)
            cast_paint(EO_CAST_CAT, pose, x + 1, y + 7 + bob, 9, 16);
        else if (i == 1u)
            dog_paint_sc(((tr / 200u) & 1u) ? DOG_WIN : DOG_LISTEN, x + 13, y + 7 + bob, 5, 8);   /* 48 x 64 -> 30 x 40 */
        else
            cast_paint(i - 2u, pose, x + 1, y + 7 + bob, 9, 16);
    }
}

static void mv_finale(uint32_t tm)                       /* the title lands on the last stab, then waits for a key */
{
    uint32_t tf = tm - MV_FINALE;
    int32_t sh = tf < 300u ? (int32_t)(((tf / 40u) & 1u) ? 3 : -3) * (int32_t)(300u - tf) / 300 : 0;
    if (tf < 60u) {
        mrect(0, 0, 240, 240, C_WHITE);
        return;
    }
    art_paint(EO_TITLE_RLE, EO_TITLE_ROW, EO_TITLE_PAL, sh, -sh / 2, 8u);
    if (tf > 400u && ((tm / 400u) & 1u))
        mctext(212, &FONT_B, "PRESS PLAY", C_WHITE);
}

static void mv_scene(uint32_t now)
{
    uint32_t t = now - mv_t0;
    if (t < MV_PRES_MS) {                /* ASilagy presents: up, still, then a fast block dissolve to black */
        uint32_t a = t < 500u ? t * 8u / 500u : 8u;
        art_paint(EO_PRES_RLE, EO_PRES_ROW, EO_PRES_PAL, 0, 0, a);
        if (t >= 1400u)
            mv_dissolve((t - 1400u) * 16u / (MV_PRES_MS - 1400u));
    } else if (t < MV_MUSIC_MS) {        /* a beat of black before the music */
    } else {
        uint32_t tm = now - mv_m0;
        if (tm < MV_ROSTER)
            mv_montage(tm);
        else if (tm < MV_FINALE)
            mv_roster(tm);
        else
            mv_finale(tm);
    }
}

static void mv_bands(void (*f)(uint32_t), uint32_t now)
{
    static const uint8_t Y[3] = {0, 36, 180}, H[3] = {36, 144, 60};
    uint32_t i;
    for (i = 0; i < 3u; i++) {
        mv_oy = Y[i];
        mv_h = H[i];
        cv_begin(240, H[i], C_BLACK);
        f(now);
        cv_blit(0, Y[i]);
    }
}

static void mv_sfx(uint32_t i, uint32_t at, uint32_t len, uint32_t key, uint8_t vo)
{
    seq_at[i] = (uint16_t)at;
    seq_len[i] = (uint16_t)len;
    seq_key[i] = (uint8_t)key;
    seq_vo_stage[i] = vo;
    seq_hl[i] = 0;
}

static void movie_tick(uint32_t now)                     /* the music starts with the montage; the title screens are silent */
{
    if (mv_stage == 0u && now >= mv_m0) {
        mv_stage = 1;
        mv_wave_t0 = now + MV_WAVE_AT;      /* the wave starts with the arpeggio, 5.72 s into the music */
        eo_smp_trig++;
    }
}

static void sel_enter(void)
{
    ui.state = ST_SELECT;
    sel_cur = set.game == GAME_COPYCAT ? 0u : set.game == GAME_DOGTOR ? 1u : 2u;
    ui.redraw = 1;
}

static void mv_end(void)                                 /* skipped, or done: the music and the effects stop */
{
    uint32_t i;
    eo_smp_halt++;
    mv_wave_t0 = 0;
    seq_on = 0;
    seq_lit = 0;
    voice_note = -1;
    for (i = 0; i < CHORD_VOICES; i++)
        eo_chord_note[i] = -1;
    sel_enter();
}

static void sel_blip(uint32_t key, uint8_t vo)
{
    seq_on = 0;
    mv_sfx(0, 0, 70, key, vo);
    seq_n = 1;
    seq_end = 90;
    eo_seq_start(0, 0);
}

static void sel_move(int32_t d)
{
    int32_t c = clamp((int32_t)sel_cur + d, 0, 5);
    if (c == (int32_t)sel_cur)
        return;
    sel_cur = (uint8_t)c;
    sel_blip(12u + (uint32_t)c * 2u, V_TRI);
    ui.redraw = 1;
}

/* ---------------------------------------------------------------- Simone's introduction --- */
/* A one-time show (per power-on) before Simone Says: a fortune teller who speaks in rhyme until her memory trips her up.
 * The text types a letter at a time with a bubbly blub for each (its pitch from the theme's scale); the down-triangle
 * says the card is done, and a key goes on. A synth theme in the double harmonic scale loops under it; it slows to a stop like a record on the "uh" of card 3, stays silent through cards 4 and 5, and returns on card 6. '~' is e-acute (the font has none; the accent is drawn). */
typedef struct { const char *l[3]; int16_t ev_at; int8_t ev; } sim_card_t;   /* ev: 1 theme starts, -1 the record stops, at this typed letter */
#define SIM_FULL 10u                     /* the first ten cards are the show; the last two are a repeat visit's */
#define SIM_CARDS 12u
static const sim_card_t sim_cards[SIM_CARDS] = {
    {{"Come closer, seeker, hear my call,", "for I see all. I am Simon~.", "I know it all."}, 0, 1},
    {{"Gaze into my crystal bowl,", "into my eyes,", "where your fate and future lies."}, 0, 0},
    {{"I looked three seconds on, and saw", "the fate of every... uh. Hm.", ""}, 56, -1},          /* the music drops out on "uh" */
    {{"It was right here.", "It was a... thing.", "It had a... shape."}, 0, 0},
    {{"Aw shucks... I was doing so well...", "What was I saying?", ""}, 0, 0},
    {{"No matter! Spirits speak in four:", "Drip. Water. Splash. Splish.", ""}, 0, 1},
    {{"Repeat each word, then one word more,", "in order, as I said before.", ""}, 0, 0},
    {{"If I should lose my place, don't fret.", "Remind me. Wait... have we met?", ""}, 0, 0},
    {{"The spirits wait. The hour is near.", "Do... Did you want something, dear?", ""}, 0, 0},
    {{"Now splash on in and make a wish,", "for I am Simon~, the all-knowing", "forgetful fish."}, 0, 0},
    {{"Come closer, seeker, hear my...", "", ""}, 0, 0},               /* a second visit: she starts the speech again */
    {{"Oh. Did I do this part already?", "...Yes? Okay. Carry on.", ""}, 0, 0},
};
static const uint8_t SIM_SCALE[8] = {0, 1, 4, 5, 7, 8, 11, 12};   /* the double harmonic scale, from A */
static const sim_card_t *sim_set = sim_cards;           /* the cards being shown: her introduction, or a level-up's */
static uint8_t sim_up_mode;              /* the cards are a level-up's: the end starts the new level */
static const sim_card_t sim_up_cards[SIMON_LEVELS - 1u][3] = {
    {
        {{"Level two! You glow, you do.", "I knew you'd pass. ...I knew. I do.", ""}, 0, 1},
        {{"Your eyes grow heavy, dear. Sleep is near.", "I had a dark premonition... a...", "a tabby... uh. Hm."}, 87, -1},
        {{"No matter! Drowsy or bright,", "we'll play till the stars say good night.", ""}, 0, 1},
    },
    {
        {{"Level three! Oh, you're a gem.", "The spirits clap. I clap for them.", ""}, 0, 1},
        {{"The crystal ball may call. Don't fall.", "I had a dark premonition... a...", "a van... a stranger... uh. Hm."}, 95, -1},
        {{"Blue and bubbly, words alone.", "Hold your breath. Make them your own.", ""}, 0, 1},
    },
    {
        {{"Level four! Come, ask for more.", "A star pupil, I adore!", ""}, 0, 1},
        {{"I may lose my place. Don't lose your pace.", "I had a dark premonition... a...", "a ghost... in a bowl... uh. Hm. That's me."}, 100, -1},
        {{"If I forget, don't fret.", "Just wait, and I'll remember yet.", ""}, 0, 1},
    },
    {
        {{"Level five! You're so alive!", "The spirits hum. They say you thrive.", ""}, 0, 1},
        {{"Some notes are shy. Use your eye.", "I had a dark premonition... a...", "a raccoon... in a trench coat... uh. Hm."}, 100, -1},
        {{"Read the words, my sweet.", "Drip, water, splash, repeat.", ""}, 0, 1},
    },
    {
        {{"Level six! Oh, what a mix!", "You've got the spirits doing tricks.", ""}, 0, 1},
        {{"My riddles say how to play today.", "I had a dark premonition... a...", "a dog... in scrubs... uh. Hm."}, 89, -1},
        {{"Listen close, my dear.", "The riddle makes it clear.", ""}, 0, 1},
    },
    {
        {{"Level seven! Pure heaven!", "Even the crystal says you're clever.", ""}, 0, 1},
        {{"I'll dim the lights. Your ears take flight.", "I had a dark premonition... a...", "a speaker... that talks... uh. Hm."}, 104, -1},
        {{"Close your eyes. Don't be shy.", "Your ears will see them passing by.", ""}, 0, 1},
    },
    {
        {{"Level eight! The final gate!", "Oh darling, you were worth the wait.", ""}, 0, 1},
        {{"All my tricks, in any weather.", "I had a dark premonition... a...", "a low, dark root... uh. Hm. Coming."}, 84, -1},
        {{"Do too well, and the spirits yell!", "But you'll be fine. ...Probably. Farewell.", ""}, 0, 1},
    },
};
static uint8_t sim_card, sim_hi, sim_music, sim_ev_done;   /* the card now, and the one the show ends after */
static uint32_t sim_t0, sim_stop_t;
static uint16_t sim_typed_last;

static uint32_t sim_len(const sim_card_t *c)             /* the letters in a card: its lines joined by one pause character */
{
    uint32_t i, n = 0;
    for (i = 0; i < 3u; i++)
        if (c->l[i][0])
            n += (uint32_t)eo_strlen(c->l[i]) + (i < 2u && c->l[i + 1u][0] ? 1u : 0u);
    return n;
}

static char sim_at(const sim_card_t *c, uint32_t k)      /* the k-th letter of the card (a line break is a space) */
{
    uint32_t i, n;
    for (i = 0; i < 3u; i++) {
        n = (uint32_t)eo_strlen(c->l[i]);
        if (k < n)
            return c->l[i][k];
        k -= n;
        if (i < 2u && c->l[i + 1u][0]) {
            if (k == 0u)
                return ' ';
            k--;
        }
    }
    return 0;
}

static uint32_t sim_typed(const sim_card_t *c, uint32_t el)   /* how many letters are out after el ms: a stop at each pause */
{
    uint32_t k = 0, t = 0, n = sim_len(c);
    while (k < n) {
        char ch = sim_at(c, k), nx = sim_at(c, k + 1u), pv = k ? sim_at(c, k - 1u) : 0;
        uint32_t d = 40u;
        if (ch == '.')
            d = (pv == '.' || nx == '.') ? 170u : 260u;
        else if (ch == ',' || ch == ':')
            d = 200u;
        else if (ch == '!' || ch == '?')
            d = 280u;
        if (t + d > el)
            break;
        t += d;
        k++;
    }
    return k;
}

static void sim_theme_start(uint32_t spin)                        /* the loop: a droning A, a hollow fifth, a bell playing the scale */
{
    uint32_t i;
    static const uint8_t arp[24] = {0, 1, 4, 5, 7, 5, 4, 1,   7, 8, 11, 12, 11, 8, 7, 5,   4, 5, 7, 8, 7, 5, 4, 1};
    seq_on = 0;
    for (i = 0; i < 24u; i++)
        mv_sfx(i, i * 300u, 270u, 16u + arp[i], V_BELL);
    seq_n = 24;
    seq_end = 24u * 300u + 300u;
    eo_seq_start(0, 0);
    eo_drone_set(45, 0.9f);
    eo_chord_note[0] = 57;
    eo_chord_note[1] = 64;
    if (spin)
        eo_rec_up++;
    else
        eo_rec_go++;
    sim_music = 1;
}

static void sim_music_off(void)
{
    seq_on = 0;
    seq_lit = 0;
    voice_note = -1;
    voice_fx = 0xFFu;
    eo_drone_note = -1;
    eo_chord_note[0] = eo_chord_note[1] = -1;
    eo_rec_go++;
    sim_music = 0;
}

static void sim_enter(uint32_t again)
{
    sim_set = sim_cards;
    sim_up_mode = 0;
    sim_card = again ? SIM_FULL : 0u;
    sim_hi = again ? SIM_CARDS : SIM_FULL;
    sim_t0 = plat_ms();
    sim_typed_last = 0;
    sim_ev_done = 0;
    sim_music = 0;
    ui.state = ST_SIMONE;
    ui.redraw = 1;
}

static void sim_enter_up(void)           /* a level is done: she tells you what is new, over the theme, a card at a time */
{
    uint32_t ly = eo_level();
    sim_set = sim_up_cards[ly - 2u];
    sim_up_mode = 1;
    sim_card = 0;
    sim_hi = 3;
    sim_t0 = plat_ms();
    sim_typed_last = 0;
    sim_ev_done = 0;
    sim_music = 0;
    ui.state = ST_SIMONE;
    ui.redraw = 1;
}

static void sim_end(void)
{
    sim_music_off();
    if (sim_up_mode) {                   /* on to the new level */
        sim_up_mode = 0;
        sim_set = sim_cards;
        eo_start_game();
        return;
    }
    ans_len = 0;
    ui.text = TX_READY;
    ui.state = ST_READY;
    ui.say[0] = 0;
    ui.redraw = 1;
}

static void sim_tick(uint32_t now)                       /* the typing's blubs, the card's music cue, the theme's loop */
{
    const sim_card_t *c = &sim_set[sim_card];
    uint32_t n = sim_typed(c, now - sim_t0);
    if (n != sim_typed_last) {
        if (n > sim_typed_last && sim_at(c, n - 1u) != ' ') {
            eo_blub_note = 57 + 12 + SIM_SCALE[(n * 3u + sim_card) % 8u] + ((n & 4u) ? 12 : 0);
            eo_blub_trig++;
        }
        sim_typed_last = (uint16_t)n;
    }
    if (c->ev && !sim_ev_done && n >= (uint32_t)c->ev_at && (sim_card > 0u || now - sim_t0 > 480u)) {   /* (card 1 lets the choosing chime finish first) */
        sim_ev_done = 1;
        if (c->ev > 0) {
            sim_theme_start(sim_card > 0u);              /* card 1 starts at full speed; card 6 spins up from low */
        } else if (sim_music == 1) {
            sim_music = 2;                               /* the record slows to a stop */
            sim_stop_t = now;
            eo_rec_stop++;
        }
    }
    if (sim_music == 2 && now - sim_stop_t > 800u) {     /* silent now; keep it so until card 6 */
        sim_music_off();
        sim_music = 3;
    }
    if (sim_music == 1 && !seq_on)
        sim_theme_start(0u);
}

static void sim_next(uint32_t now)
{
    if (sim_card + 1u >= sim_hi) {
        sim_end();
        return;
    }
    sim_card++;
    sim_t0 = now;
    sim_typed_last = 0;
    sim_ev_done = 0;
    ui.redraw = 1;
}

static void sim_key(uint32_t now)                        /* a key: finish the line, or go on to the next card */
{
    const sim_card_t *c = &sim_set[sim_card];
    if (sim_typed(c, now - sim_t0) < sim_len(c))
        sim_t0 = now - 60000u;
    else
        sim_next(now);
}

static void sim_line(int32_t y, const char *s, uint32_t n, uint16_t col)   /* one line, typed up to n letters; '~' is e-acute */
{
    char full[48], part[48];
    uint32_t i, len = (uint32_t)eo_strlen(s);
    int32_t x0;
    for (i = 0; i < len && i < 47u; i++)
        full[i] = s[i] == '~' ? 'e' : s[i];
    full[i] = 0;
    x0 = 10;                                             /* every line from the same left edge, like a typewriter */
    if (n > len)
        n = len;
    for (i = 0; i < n && i < 47u; i++)
        part[i] = full[i];
    part[i] = 0;
    mtext(x0, y, &FONT_S, part, col);
    for (i = 0; i < n; i++)
        if (s[i] == '~') {
            char pre[48];
            uint32_t j;
            int32_t ex;
            for (j = 0; j < i; j++)
                pre[j] = full[j];
            pre[j] = 0;
            ex = x0 + text_w(&FONT_S, pre) + text_w(&FONT_S, "e") / 2;
            mrect(ex, y + 1, 3, 2, col);
            mrect(ex - 2, y + 3, 3, 2, col);
        }
}

static void sim_scene(uint32_t now)
{
    const sim_card_t *c = &sim_set[sim_card];
    uint32_t el = now - sim_t0, n = sim_typed(c, el), tot = sim_len(c), i, used = 0;
    int32_t bob = ((now / 500u) & 1u) ? 2 : 0;
    mrect(0, 0, 240, 150, EO_RGB(22, 10, 40));
    for (i = 0; i < 14u; i++) {                          /* stars, twinkling */
        int32_t sx = (int32_t)((i * 53u + 17u) % 236u), sy = (int32_t)((i * 37u + 9u) % 140u);
        if (((now / 260u) + i) % 6u)
            mrect(sx, sy, 2, 2, EO_RGB(120, 100, 170));
    }
    cast_paint(0u, (n < tot && ((now / 120u) & 1u)) ? 1u : 0u, 24, 6 + bob, 2, 1);   /* the goldfish, mouth moving while she talks */
    mrect(0, 150, 240, 90, C_BLACK);
    mrect(0, 150, 240, 2, EO_RGB(120, 100, 170));
    for (i = 0; i < 3u; i++) {
        uint32_t len = (uint32_t)eo_strlen(c->l[i]);
        if (!len)
            continue;
        sim_line(158 + (int32_t)i * 19, c->l[i], n > used ? n - used : 0u, C_WHITE);
        used += len + (i < 2u && c->l[i + 1u][0] ? 1u : 0u);
    }
    if (n >= tot && ((now / 450u) & 1u)) {               /* the card is done: a solid down-triangle says go on */
        int32_t r;
        for (r = 0; r < 6; r++)
            mrect(224 + r, 222 + r, 12 - 2 * r, 1, C_AMB);
    }
}

static void sel_go(void)
{
    int32_t g = sel_game[sel_cur];
    if (g < 0) {                                         /* not made yet: a small wah-wah */
        seq_on = 0;
        mv_sfx(0, 0, 140, 12, V_TRI);
        mv_sfx(1, 150, 260, 7, V_TRI);
        seq_n = 2;
        seq_end = 420;
        eo_seq_start(0, 0);
        return;
    }
    eo_drone_set(-1, 0.0f);
    seq_on = 0;                                          /* the choosing chime: a rising fifth, G3 then D4, in the soft triangle voice */
    mv_sfx(0, 0, 130, 2, V_TRI);
    mv_sfx(1, 120, 330, 9, V_TRI);
    seq_n = 2;
    seq_end = 470;
    eo_seq_start(0, 0);
    if (g != (int32_t)set.game) {
        set.game = (uint8_t)g;
        set_dirty = 1;
    }
    eo_voice_sync();
    ans_len = 0;
    ui.text = TX_READY;
    ui.state = ST_READY;
    ui.say[0] = 0;
    if (g == GAME_DOGTOR)
        dogtor_say(DS_READY, 0, 2200u);
    if (g == GAME_SIMON) {                               /* Simone introduces herself; after the first time she starts to, and remembers */
        sim_enter(eo_sim_seen);
        eo_sim_seen = 1;
        return;
    }
    if (g == GAME_COPYCAT && !eo_cat_seen) {             /* the cat introduces itself, once a power-on */
        eo_cat_seen = 1;
        ui.state = ST_INTRO;
        ui.ctl_page = 0;
    }
    ui.redraw = 1;
}

#define SEL_BPM8 188u                                    /* the select's pulse: eighth notes at 94 bpm (319 ms) */
static const char *const sel_desc[6][2] = {
    {"Meow sings a melody. You play it back.", "Be careful, he's judgmental!"},
    {"Help Dr. Diggie Diagnose the Chord", "Major, minor, or something worse?"},
    {"Simone says. You play it back.", "She's a goldfish. She forgot already."},
    {"Still warming up.", "A new game is on the way."},
    {"Still warming up.", "A new game is on the way."},
    {"Still warming up.", "A new game is on the way."},
};

static void sel_arrow(int32_t x, int32_t cy, int left, uint16_t c)
{
    int32_t r;
    for (r = -8; r <= 8; r++) {
        int32_t a = r < 0 ? -r : r;
        mrect(left ? x + a : x, cy + r, 10 - a, 1, c);
    }
}

static void sel_scene(uint32_t now)                      /* one game at a time, full screen */
{
    uint32_t i;
    uint32_t qph = now % 638u;                           /* a quarter note at 94 bpm: the bounce, eased, no frame flips */
    int32_t k, sh = (int32_t)(now / 5u % 48u), bob = (int32_t)((638u - qph) * (638u - qph) * 5u / (638u * 638u)), cur = sel_game[sel_cur];
    const eo_char_t *c = &eo_chars[sel_cur];
    uint16_t a = cur >= 0 ? EO_RGB(c->r, c->g, c->b) : EO_RGB(120, 120, 150);
    for (k = -2; k < 11; k++)                            /* the pinstripes, in the game's colour */
        mrect(k * 24 + sh, 0, 24, 240, (k & 1) ? mv_scale(a, 3u) : mv_scale(a, 2u));
    if (cur == GAME_COPYCAT)
        cast_paint(EO_CAST_CAT, 0u, 24, 30 + bob, 2, 1);
    else if (cur == GAME_DOGTOR)
        dog_paint_sc(DOG_LISTEN, 72, 30 + bob, 2, 1);
    else if (cur == GAME_SIMON)                          /* Simone is the goldfish */
        cast_paint(0u, 0u, 24, 30 + bob, 2, 1);
    else
        mctext(70 + bob, &FONT_L, "?", C_WHITE);
    mrect(0, 0, 240, 28, C_BLACK);                       /* the header: the game */
    mgame(5, &FONT_B, cur >= 0 ? c->game : "COMING SOON", a);
    if (set.pad & 1u)
        mtext(4, 8, &FONT_XS, "DEBUG", C_AMB);
    mrect(0, 176, 240, 64, C_BLACK);                     /* the footer: who, what, where */
    for (i = 0; i < 6u; i++)                             /* the page dots */
        mrect(120 - 33 + (int32_t)i * 12, 180, 9, 5, i == sel_cur ? C_WHITE : sel_game[i] >= 0 ? C_GRAY : C_LINE);
    mctext(190, &FONT_S, cur >= 0 ? c->cap : "THE MYSTERY", C_AMB);
    mctext(207, &FONT_XS, sel_desc[sel_cur][0], C_WHITE);
    mctext(222, &FONT_XS, sel_desc[sel_cur][1], C_GRAY);
    {                                                    /* the arrows: turn PRESETS for another game */
        int32_t nudge = bob / 2;
        sel_arrow(6 - nudge, 100, 1, sel_cur > 0u ? C_WHITE : C_LINE);
        sel_arrow(224 + nudge, 100, 0, sel_cur < 5u ? C_WHITE : C_LINE);
    }
}

/* ---- Simone Says: the screen. The sequence is a row of word bubbles (the mnemonic: DRIP WATER SPLASH SPLISH, one per
 * note); on your turn they pop oldest first, a goldfish's memory, and you play it back before it is gone. */
static const char *const SIM_WORD[4] = {"DRIP", "WATER", "SPLASH", "SPLISH"};
static const uint16_t SIM_COL[4] = {EO_RGB(90, 170, 255), EO_RGB(80, 220, 170), EO_RGB(255, 214, 80), EO_RGB(255, 120, 180)};
static uint32_t sim_gap_n(uint32_t now)                  /* the letters of her pause line that are out so far */
{
    uint32_t t = now - sim_gap_t0;
    if (t < 250u)
        return 0;
    return sim_cameo && t >= 3200u ? (t - 3200u) / 50u + 1u : (t - 250u) / (sim_cameo ? 50u : 55u) + 1u;
}
static void sim_game_tick(uint32_t now)                  /* a frame about every 33 ms, for the goldfish's bob */
{
    static uint32_t last_fr;
    if (now / 33u != last_fr) {
        last_fr = now / 33u;
        ui.redraw = 1;
        if (ui.state == ST_GAP) {            /* her pause line types itself out, a bubble a letter */
            uint32_t n = sim_gap_n(now);
            if (n < sim_typed_n)
                sim_typed_n = 0;             /* (the second beat of a cameo starts the typing again) */
            if (n > sim_typed_n) {
                eo_blub_note = 57 + 12 + SIM_SCALE[(n * 3u) % 8u] + ((n & 4u) ? 12 : 0);
                eo_blub_trig++;
            }
            sim_typed_n = n;
        }
        if (sim_in_preroll()) {
            uint32_t ph = seq_pos % 900u, tri = ph < 450u ? ph : 900u - ph;      /* the push has a sound: a low hum that climbs as she nears */
            lights();                        /* the dazzle moves with no input */
            eo_drone_set(KEY_NOTE0 + (int32_t)ROOT_MIN - 12 + (int32_t)(tri / 45u), 0.35f);
            sim_push_snd = 1;
        } else if (sim_push_snd) {
            sim_push_snd = 0;
            eo_drone_set(-1, 0.0f);
        }
        {   /* a lapse has its own sounds: the intro's record slowing to a stop on "...uh.", a drooping blub on "Where was I?", a bright blub when she remembers */
            static uint8_t fi;
            static const uint16_t ft_t[2] = {0, 1800};
            static const uint8_t ft_n[2] = {74, 67};
            if (ui.state == ST_LISTEN && seq_on && sim_gap_len && seq_pos >= sim_gap_at) {
                uint32_t d = seq_pos - sim_gap_at;
                if (sim_gap_stall) {
                    uint32_t want = d >= ft_t[1] ? 2u : 1u;
                    while (fi < want) {
                        eo_blub_note = ft_n[fi];
                        eo_blub_trig++;
                        fi++;
                    }
                } else {
                    if (fi < 1u) {                   /* the music (a low drone and a fifth) starts... */
                        eo_rec_go++;
                        eo_drone_set(45, 0.7f);
                        eo_chord_note[0] = 57;
                        eo_chord_note[1] = 64;
                        fi = 1;
                    }
                    if (fi < 2u && d >= 150u) {      /* ...and slows down like a record, in a long groan */
                        eo_rec_stop++;
                        fi = 2;
                    }
                    if (fi < 3u && d >= 1000u) {     /* silent again */
                        eo_drone_set(-1, 0.0f);
                        eo_chord_note[0] = eo_chord_note[1] = -1;
                        eo_rec_go++;
                        fi = 3;
                    }
                    if (fi < 4u && d >= 1300u) {     /* "Where was I?" */
                        eo_blub_note = 62;
                        eo_blub_trig++;
                        fi = 4;
                    }
                }
                if (seq_pos >= sim_gap_at + sim_gap_len && fi < 8u) {     /* she remembers */
                    eo_blub_note = 88;
                    eo_blub_trig++;
                    fi = 8u;
                }
            } else {
                fi = 0;
            }
        }
        {   /* Meow's cameo purrs while he sits by the bowl (the second beat) */
            uint32_t pt = now - sim_gap_t0;
            uint8_t want = (uint8_t)(ui.state == ST_GAP && sim_cameo && pt >= 3200u && pt < 6000u && !seq_on);
            if (want) {
                voice_fx = V_PURR;
                voice_note = KEY_NOTE0 + (int32_t)ROOT_MIN - 12;
                sim_purr_on = 1;
            } else if (sim_purr_on) {
                sim_purr_on = 0;
                voice_note = -1;
                voice_fx = 0xFFu;
            }
        }
    }
}

/* the screen: Simone owns the top left, big; her quips are the top right (she speaks in rhyme until her memory trips her);
 * the keys are pushed to the bottom: C D E G big, in their colours, with the unused keys between them dim for parity. The
 * key that sounds (or that you hold) fills with its colour and its word sits on it. The numbers live in the header. The keys
 * run from the middle band down into the foot, so they are painted twice, each time with its own top edge. */
static void sim_key_label(int32_t x, int32_t w, int32_t y, const char *s, uint16_t c)
{
    cv_text(x + (w - text_w(&FONT_XS, s)) / 2, y, &FONT_XS, s, c);
}

static uint16_t sim_hue(uint32_t p)       /* a rainbow, p 0..255 */
{
    uint32_t r, g, bl, ph = (p & 255u) * 6u, seg = ph >> 8, f = ph & 255u;
    r = seg == 0u ? 255u : seg == 1u ? 255u - f : seg == 2u ? 0u : seg == 3u ? 0u : seg == 4u ? f : 255u;
    g = seg == 0u ? f : seg == 1u ? 255u : seg == 2u ? 255u : seg == 3u ? 255u - f : 0u;
    bl = seg == 2u ? f : seg == 3u ? 255u : seg == 4u ? 255u : 0u;
    return EO_RGB(r, g, bl);
}

static void sim_keys(int32_t y0)         /* y0: where the keys' top edge is, in this band (negative: above it) */
{
    static const uint8_t WH[5] = {7, 9, 11, 12, 14};         /* C D E F G */
    static const uint8_t BA[3] = {0, 1, 3};                  /* the white key each follows */
    uint32_t i, bright, mid, dim;
    key_levels(&bright, &mid, &dim);
    for (i = 0; i < 5u; i++) {                               /* the white keys */
        int32_t x = SIM_KX + (int32_t)i * SIM_KP;
        uint32_t k = WH[i], ix = simon_word_ix(k - SIMON_BASE);
        int play = (k != 12u);                               /* F is not in play */
        int lit = play && (int)((bright >> k) & 1u);
        uint16_t c = !play ? C_LINE : lit ? SIM_COL[ix] : mv_scale(SIM_COL[ix], 2u);
        if (sim_in_preroll()) {              /* the dazzle: a rainbow rolls over every key, one at a time brighter */
            uint32_t t = seq_pos, on = ((t / 110u) + i) % 5u == 2u;
            c = mv_scale(sim_hue((t / 5u + i * 50u) & 255u), on ? 8u : 3u);
            lit = (int)on;
        }
        cv_rect(x, y0, 44, 90, c);
        if (lit)
            cv_rect(x, y0, 44, 3, C_WHITE);                  /* a bright rim: it is the one */
        if (play)
            sim_key_label(x, 44, y0 + 74, SIM_WORD[ix], lit ? C_BLACK : mv_scale(SIM_COL[ix], 5u));
    }
    for (i = 0; i < 3u; i++)                                 /* the black keys: never in play, so dark */
        {   /* a real black key: from the top edge down, a near-black body with a thin lighter edge so it never reads as a hole */
            int32_t bx = SIM_KX + ((int32_t)BA[i] + 1) * SIM_KP - 15;
            cv_rect(bx - 1, y0, 28, 55, C_GRAY);
            cv_rect(bx, y0, 26, 54, EO_RGB(10, 10, 16));
        }
}

static const char *const sim_praise[8] = {      /* after a round with a trick: she is delighted with you */
    "You saw right through my trick! My, aren't you quick!", "Brilliant, darling! Even the spirits gasped. ...I knew. Obviously.",
    "The spirits swoon! You're in tune!", "Through every wobble, you did not hobble. Marvelous!",
    "Stunning, my sweet! Not a single missed beat!", "I tried to trick you, dear. You laughed. I'm impressed. And a bit scared.",
    "Oh, you magnificent creature. The crystal bows to you.", "A flawless feat! You are a treat. Come, take a seat!"};
static const char *const sim_gap_say[8] = {
    "Darling, I see a star. Oh... it's you! There you are!", "The cards adore you, dear. My fee is near.",
    "The crystal clears... I see... me! Hello!",             "Oh sweet one, you have the gift. Let's lift!",
    "My palm says the next note is... a note.",              "Behold, my love, a longer chain! Do not complain.",
    "I'm getting a message from the spirits. It says... blub.", "Stay close, darling. I have a feeling. ...Or a snack. Same thing."};
static char sim_buf[96];
static const char *sim_full;            /* the whole of a line being typed: the card is sized for it, so it never grows or changes font as it types */
static const char *sim_typed_text(const char *full, uint32_t n)   /* the first n letters of a line */
{
    uint32_t i;
    for (i = 0; i < n && full[i] && i < 94u; i++)
        sim_buf[i] = full[i];
    sim_buf[i] = 0;
    return sim_buf;
}
static const char *sim_quip(uint32_t tx)                     /* what she says: rhyme, until she loses it */
{
    uint32_t lv = eo_level(), now = plat_ms();
    sim_alert = 0;
    sim_full = 0;
    if (ui.state == ST_WISH || (ui.state == ST_READY && ui.text == TX_OVER)) {   /* you have angered the spirits */
        static const char *const talk[6] = {"No fear, darling. I foresaw this. ...Just now.",
                                            "The spirits scowl, my sweet. Still, you're a treat.",
                                            "Calm waters, sweet one. Now... who are you again?",
                                            "Defeat, my dear, is the future drawing near.",
                                            "The spirits are cross. I'd tell you why. ...I'd tell you why.",
                                            "Chin up, my love. Tomorrow's stars are kind. ...Did I say that before?"};
        uint32_t t = now - sim_wish_t0, beat;
        if (t < 3600u) {                     /* the shout, once */
            sim_alert = 2;
            return "YOU'VE ANGERED THE SPIRITS";
        }
        beat = (t - 3600u) / 4200u;          /* then she talks: mostly plain words, the instruction every other beat */
        if (beat & 1u) {
            if (ui.state == ST_WISH) {
                sim_alert = 1;
                return "SELECT: MAKE A WISH. PLAY: GIVE UP.";
            }
            sim_alert = 1;
            return "Oh, the spirits cry! Press PLAY to try.";
        }
        return talk[(beat / 2u + sim_say_n) % 6u];
    }
    if (ui.state == ST_GAP) {                /* the pause: a line typed out a letter at a time (Meow, now and then) */
        uint32_t n = sim_gap_n(now), t = now - sim_gap_t0;
        if (sim_cameo) {
            sim_full = t < 3200u ? "AHHHH!! MEOW DON'T EAT ME!" : "...wait, who's Meow?";
            return sim_typed_text(t < 3200u ? "AHHHH!! MEOW DON'T EAT ME!" : "...wait, who's Meow?", n);
        }
        {
            const char *g = sim_distract ? sim_praise[sim_say_n & 7u] : sim_gap_say[(ans_len + sim_say_n) & 7u];
            sim_full = g;
            return sim_typed_text(g, n);
        }
    }
    if (ui.state == ST_LISTEN && seq_on) {
        static const char *const fortune[6] = {"Come close, my dear. The spirits are near.", "I see a door. And... one door more.",
                                               "The cards foretell: you'll do quite well, dear.", "A tall dark stranger! ...Oh. It's you, gorgeous.",
                                               "I see a long road... and a short road... and snacks.", "Your aura is... teal! Or a dolphin. Teal."};
        uint32_t g0 = sim_gap_at, g1 = sim_gap_at + sim_gap_len;
        if (now < sim_wish_until) {          /* a wish is being granted */
            sim_alert = 1;
            return sim_wish_peek ? "Listen well. The wish is a spell." : "Your wish is granted. Try again, enchanted.";
        }
        if (sim_angry_pre && sim_pre && seq_pos < sim_pre) {
            sim_alert = 2;
            return seq_pos < sim_pre / 2u ? "YOU'VE ANGERED THE SPIRITS" : "NOW PLAY IT BACKWARDS!";
        }
        if (sim_dark && sim_pre && seq_pos < sim_pre) {
            sim_alert = 1;
            return seq_pos < sim_pre / 2u ? "I'll dim the lights, my darling dear..." : "...but you will hear! TRUST YOUR EARS!";
        }
        if (sim_twice && sim_pre && seq_pos < sim_pre) {
            static const char *const ra[6] = {0, "I double your fortune, the future is nice...", "The omens are nice. I'll say it twice...",
                                              "Ignore the middle, that's the riddle...", "The first note's shy. Let it pass by...", "The last note loves a bow, you know..."};
            static const char *const rb[6] = {0, "...but only if you REPEAT EACH NOTE TWICE!", "...the SECOND note must be played THRICE!",
                                              "...play only the FIRST and the LAST. Be fast!", "...and play all the REST. Do your best!", "...so play it TWICE before you go!"};
            sim_alert = 1;
            return seq_pos < sim_pre / 2u ? ra[sim_rule] : rb[sim_rule];
        }
        if (sim_mute && sim_pre && seq_pos < sim_pre) {
            sim_alert = 1;
            if (sim_mute == 1u)
                return seq_pos < sim_pre / 2u ? "You've used your ears, my dear..." : "...now use your EYES! Watch the words appear!";
            return seq_pos < sim_pre / 2u ? "One note is shy. It makes no sound..." : "...so watch the WORD that floats around!";
        }
        if (sim_sleepy && sim_pre && seq_pos < sim_pre) {
            sim_alert = 1;
            return seq_pos < sim_pre / 2u ? "Your eyes grow heavy, my dear..." : "...sleep... is... near...";
        }
        if (sim_crystal && sim_pre && seq_pos < sim_pre) {
            sim_alert = 1;
            return "Into the crystal ball you fall!";
        }
        if (sim_gap_len && seq_pos >= g0 && seq_pos < g1 + 2000u) {      /* a lapse: the lines that matter, on the big card */
            sim_alert = 1;
            if (sim_gap_stall)
                return seq_pos < g1 ? (seq_pos < g0 + 1800u ? "Hmmmm..." : "On the tip of my fin...") : "Got it!";
            if (seq_pos < g1)
                return seq_pos < g0 + 1200u ? "...uh." : "Where was I?";
            return sim_gap_restart ? "From the top!" : "Ah. Right. Onward.";
        }
        if (sim_pre && seq_pos < sim_pre) {
            if (sim_pre_first) {             /* the first time she does it, she says what it is: a distraction */
                sim_alert = 1;
                return "I'll dazzle you, my dear. Watch the keys, never fear!";
            }
            {
                static const char *const eyes[3] = {"Look deep into my eyes... and be wise.", "Closer, darling... closer... deeper...",
                                                    "You are getting sleepy... no. Wrong game."};
                return eyes[sim_say_n % 3u];
            }
        }
        if (sim_mute)
            return sim_mute == 1u ? "Read the words, darling. Read them." : "Ears... and eyes, my sweet. Both.";
        if (sim_dark)
            return "Eyes closed, ears open, darling. Or the other way round.";
        if (sim_sleepy)
            return "Zzz... listen, darling... zzz...";
        if (lv >= 2u && ans_len >= 3u && !sim_crystal)
            return fortune[(ans_len + lv + sim_say_n) % 6u];     /* one fortune a round, held: read the keys, not her */
    }
    if (ui.state == ST_TURN && sim_twice) {  /* a riddle: say the rule again at the start of your turn */
        static const char *const rt[6] = {0, "EACH NOTE TWICE!", "2ND NOTE THRICE!", "FIRST AND LAST ONLY!", "SKIP THE FIRST NOTE!", "LAST NOTE TWICE!"};
        static const char *const rx[6] = {0, "Each note twice, my dear. Do it nice!", "Note two, three times. Mind the rhymes!",
                                          "Just the ends, my friends. The middle bends.", "The first is shy, my dear. Let it disappear.",
                                          "Last note twice, my dear. Make it clear."};
        if (now - sim_turn_t0 < 3000u) {
            sim_alert = 1;
            return rt[sim_rule];
        }
        return rx[sim_rule];
    }
    if (ui.state == ST_TURN && sim_back) {   /* the angry round: you play it backwards */
        if (now - sim_turn_t0 < 3600u) {
            sim_alert = 2;
            return "BACKWARDS! LAST NOTE FIRST!";
        }
        return "Backwards, dear: last note first. Do your worst!";
    }
    if (ui.state == ST_READY) {
        static const char *const rd[8][3] = {
            {"Press PLAY, darling. Your future's calling.", "Come closer, seeker. The bowl is warm... and so are you.", "Press PLAY, my dear. I see it clear. ...Or is it near?"},
            {"Press PLAY, my love. The stars above approve.", "The spirits hum. They say you're someone. Press PLAY, someone.", "Faster now, darling. Hold on to your fins."},
            {"Shh. The crystal hums. Sometimes... it lends a hand.", "Press PLAY, my sweet. The water's neat.", "Third time's the charm, darling. ...Is it the third?"},
            {"Mind my memory, darling. It's a leaky bowl.", "Press PLAY. If I forget, don't fret.", "Your turn to shine, my dear. I'll try to stay here."},
            {"Watch my words, my sweet. Your eyes can eat.", "Press PLAY. Some notes may hide away.", "Drip, water, splash... Press PLAY, I'll dash."},
            {"Riddles, darling! I speak in rhyme. ...Always. Mostly.", "Listen well, my sweet. The riddle's in the beat.", "Press PLAY. A rule is coming your way."},
            {"The lights may dim. Don't be grim.", "Trust your ears, my dear. The lights may disappear.", "Press PLAY, my sweet. Let's feel the beat."},
            {"Everything at once, my darling. Don't panic. ...I'm panicking.", "The finale, my dear. The end is near.", "One last PLAY, my love. The spirits are above."}};
        return rd[(lv < 1u ? 1u : lv > 8u ? 8u : lv) - 1u][sim_say_n % 3u];
    }
    if (tx == TX_WATCH) {
        static const char *const w[5] = {"Come closer, darling. Watch and hear. The fates are near.", "Mmm, listen, love. The spirits sing... a little thing.",
                                         "Eyes on the keys, my dear. Ears wide. The future's inside.", "Hush now, hush. The waters rush. ...What rush?",
                                         "Watch my fin, darling. Now begin."};
        return w[sim_say_n % 5u];
    }
    if (tx == TX_NICE) {
        static const char *const n[4] = {"Splendid, darling! I knew it. ...Knew what?", "Ooh, you glow! The spirits know.",
                                         "Magnificent! A perfect run. ...Of what, my sweet? Fun.", "I felt that in my fins. Delicious. Again!"};
        return sim_distract ? sim_praise[(sim_say_n + 3u) & 7u] : n[sim_say_n & 3u];
    }
    if (tx == TX_MISS) {
        static const char *const m[3] = {"Oh, my sweet. The song disappeared. ...How weird!", "Alas, my dear. The note's not here. ...Was it ever near?",
                                         "A sour note. The spirits gloat. ...I'm so sorry. Press PLAY."};
        return m[sim_say_n % 3u];
    }
    {
        static const char *const y[5] = {"Now you play, my lovely. Don't stray.", "Your turn, my sweet. Let's hear the beat.",
                                         "Go on, darling. Show the spirits you're charming.", "Don't think, just feel. Make it real!",
                                         "Play it back, my dear. ...Play what? Oh. That."};
        return y[sim_say_n % 5u];
    }
}

static uint32_t sim_any_lit(void)
{
    uint32_t i, bright, mid, dim;
    key_levels(&bright, &mid, &dim);
    for (i = 0; i < 4u; i++)
        if ((bright >> (SIMON_BASE + simon_level(1)->off[i])) & 1u)
            return 1;
    return 0;
}

static void sim_sprite(uint32_t now, int32_t band_top, int32_t band_h)   /* her, big, over the keys: once per band */
{
    int32_t num = 20, ox, oy, sway = 0, hop = 0, pose = (int32_t)sim_any_lit();
    if (ui.state == ST_LEVELUP) {            /* she is delighted with you: bouncing, eyes wide */
        uint32_t ph = (now / 110u) % 8u;
        hop = (int32_t)(ph < 4u ? ph : 8u - ph) * 4;
        pose = 1;
    }
    if (ui.state == ST_LISTEN && seq_on) {
        if (sim_pre && seq_pos < sim_pre && !sim_sleepy && !sim_twice && !sim_dark && !sim_mute) {  /* the push: she swells toward you and falls back, over and over, eyes wide */
            uint32_t ph = seq_pos % 900u;
            int32_t tri = ph < 450u ? (int32_t)ph : (int32_t)(900u - ph);       /* 0..450..0 */
            num = 20 + sim_env(seq_pos, 0u, sim_pre) * tri * 14 / 450 / 8;
            pose = 1;
            sway = (int32_t)(seq_pos % 900u < 450u ? seq_pos % 450u : 450u - seq_pos % 450u) / 45 - 5;   /* and rocks side to side */
            sway = sway * sim_env(seq_pos, 0u, sim_pre) / 8;
        } else if (sim_gap_len && !sim_gap_stall && seq_pos >= sim_gap_at && seq_pos < sim_gap_at + sim_gap_len) {
            num = 20 - sim_env(seq_pos, sim_gap_at, sim_gap_at + sim_gap_len) * 7 / 8;     /* the lapse: she shrinks back */
        }
    }
    ox = 80 - 6 * num;                       /* the bowl stays centred on the same spot as she swells */
    oy = 76 - (9 * num) / 2;
    mv_oy = band_top;                        /* the sprite painter clips to the band, in the middle band's coordinates */
    mv_h = band_h;
    cast_paint(0u, (uint32_t)pose, ox + sway, oy - hop + (int32_t)((now / 700u) & 1u), num, 8);
}

static int32_t sim_eyes_word(void)       /* a silent note is sounding: its word (0..3), to show big; else -1 */
{
    uint32_t i;
    if (set.game != GAME_SIMON || ui.state != ST_LISTEN || !seq_on || !sim_mute || (sim_pre && seq_pos < sim_pre))
        return -1;
    for (i = 0; i < seq_n; i++)
        if (seq_pos >= seq_at[i] && seq_pos < (uint32_t)seq_at[i] + seq_len[i] && seq_mute[i])
            return (int32_t)simon_word_ix((uint32_t)seq_key[i] - SIMON_BASE);
    return -1;
}

static uint32_t sim_crystal_on(void)     /* the crystal ball is showing: her pre-roll is over and her sequence is playing */
{
    return set.game == GAME_SIMON && ui.state == ST_LISTEN && seq_on && sim_crystal && !(sim_pre && seq_pos < sim_pre);
}

#define SIM_BLUE EO_RGB(18, 52, 160)
static uint8_t sim_mask[32][112];        /* the word's coverage, drawn once at 1x, then painted twice the size, rippling */
static uint32_t sim_mask_w;
static int32_t sim_crystal_word(void)    /* the word that sounds now (0..3), or -1 */
{
    uint32_t i, p = seq_pos;
    int32_t act = -1;
    for (i = 0; i < seq_n; i++)
        if (p >= seq_at[i] && p < (uint32_t)seq_at[i] + seq_len[i])
            act = (int32_t)i;
    return act < 0 ? -1 : (int32_t)simon_word_ix((uint32_t)seq_key[act] - SIMON_BASE);
}
static void sim_crystal_mask(uint32_t w)   /* the canvas is black: draw the word white, keep what it covers */
{
    uint32_t x, y, tw = (uint32_t)text_w(&FONT_L, SIM_WORD[w]);
    tw = tw > 112u ? 112u : tw;
    cv_text(0, 0, &FONT_L, SIM_WORD[w], C_WHITE);
    for (y = 0; y < 32u; y++)
        for (x = 0; x < tw; x++)
            sim_mask[y][x] = (uint8_t)((swap16(cv_px[y * cv_w + x]) >> 5) & 63u);
    sim_mask_w = tw;
}
static void sim_blend(int32_t x, int32_t y, uint16_t c, uint32_t a)   /* a 0..63 over what is there */
{
    uint32_t bg, r, g, b;
    if (x < 0 || x >= (int32_t)cv_w || y < 0 || y >= (int32_t)cv_h || !a)
        return;
    bg = swap16(cv_px[(uint32_t)y * cv_w + (uint32_t)x]);
    r = ((uint32_t)(c >> 11) * a + (bg >> 11) * (63u - a)) / 63u;
    g = ((uint32_t)((c >> 5) & 63u) * a + ((bg >> 5) & 63u) * (63u - a)) / 63u;
    b = ((uint32_t)(c & 31u) * a + (bg & 31u) * (63u - a)) / 63u;
    cv_px[(uint32_t)y * cv_w + (uint32_t)x] = swap16((r << 11) | (g << 5) | b);
}
static void sim_crystal_paint(uint32_t now, uint32_t w)   /* the word, doubled, drifting and rippling as if seen through water */
{
    int32_t dy, sx, x0 = (240 - 2 * (int32_t)sim_mask_w) / 2, y0 = 36 + (int32_t)(4.0f * fm_sinf((float)now * 0.003f));
    for (dy = 0; dy < 64; dy++) {
        float ph = 0.2f * (float)dy + (float)now * 0.004f, wv = fm_sinf(ph);
        int32_t off = (int32_t)(3.0f * wv);
        for (sx = 0; sx < (int32_t)sim_mask_w; sx++) {
            uint32_t a = sim_mask[dy / 2][sx];
            if (!a)
                continue;
            sim_blend(x0 + sx * 2 + off + 5, y0 + dy + 5, EO_RGB(6, 20, 90), a * 3u / 4u);    /* the wobbling shadow below it */
        }
    }
    for (dy = 0; dy < 64; dy++) {
        float wv = fm_sinf(0.2f * (float)dy + (float)now * 0.004f);
        int32_t off = (int32_t)(3.0f * wv);
        uint16_t col = wv > 0.92f ? C_WHITE : SIM_COL[w];          /* a glint runs down the word */
        for (sx = 0; sx < (int32_t)sim_mask_w; sx++) {
            uint32_t a = sim_mask[dy / 2][sx];
            if (!a)
                continue;
            sim_blend(x0 + sx * 2 + off, y0 + dy, col, a);
            sim_blend(x0 + sx * 2 + off + 1, y0 + dy, col, a);
        }
    }
}
static void sim_crystal_layer(uint32_t now, int32_t y0, int32_t word)   /* the whole screen, one band of it: bubbles rising and the word */
{
    uint32_t i;
    for (i = 0; i < 10u; i++) {
        int32_t sz = 6 + (int32_t)(i % 3u) * 4;
        int32_t x = 12 + (int32_t)((i * 29u + (i * i * 13u) % 19u) % 210u);
        int32_t y = 250 - (int32_t)((now * (28u + (i % 4u) * 9u) / 1000u + i * 53u) % 280u) - y0;
        cv_rect(x, y, sz, sz, EO_RGB(130, 180, 255));
        cv_rect(x + 1, y + 1, sz - 2, sz - 2, EO_RGB(46, 96, 214));
        cv_rect(x + 2, y + 2, 2, 2, EO_RGB(200, 225, 255));
    }
    if (y0 == 36 && word >= 0)
        sim_crystal_paint(now, (uint32_t)word);
}

/* the level-up scene: she bounces over the keys and says what is new in her own verse, one beat at a time */
static const char *const sim_up_say[SIMON_LEVELS + 1u][3] = {
    {0, 0, 0},
    {0, 0, 0},
    {"Well done, my dear! The future's clear.", "Now faster, faster! Pure disaster.", "I might forget. Don't fret!"},
    {"Splendid, darling! You listened well. A lovely spell.", "Sometimes I stall. The crystal ball may call.", "Late in a level, I dim the lights."},
    {"A star pupil, I swear! Or luck. Who cares?", "Your ears must lead, my love. Indeed.", "Do too well, and the spirits yell!"},
};
static void sim_up_band(uint32_t now)
{
    uint32_t lv = eo_level(), beat = (now - sim_up_t0) / 4400u, ly;
    const char *q;
    int32_t yb;
    if (beat > 2u)
        beat = 2u;
    ly = lv > SIMON_LEVELS ? SIMON_LEVELS : lv;
    q = sim_up_say[ly][beat] ? sim_up_say[ly][beat] : "Splendid!";
    sim_keys(114);
    sim_sprite(now, 0, 144);
    wrap_dry = 1;
    yb = wrap_text(142, 12, 90, &FONT_B, q, C_WHITE, 20);
    wrap_dry = 0;
    cv_rect(134, 4, 106, yb - 2, C_HI);
    cv_rect(137, 7, 100, yb - 8, C_BLACK);
    wrap_text(142, 12, 90, &FONT_B, q, C_WHITE, 20);
    if (beat >= 2u && ((now / 500u) & 1u))
        cv_text(142, yb + 4, &FONT_XS, "PLAY: ONWARD", C_HI);
}

static void sim_star(int32_t x, int32_t y, uint16_t c)   /* a wish: a little plus-shaped star */
{
    cv_rect(x + 3, y, 3, 9, c);
    cv_rect(x, y + 3, 9, 3, c);
    cv_rect(x + 2, y + 2, 5, 5, c);
}

static void sim_band(uint32_t now)
{
    uint32_t tx = eo_text();
    const char *q;
    sim_keys(114);
    sim_sprite(now, 0, 144);
    if (ui.state == ST_GAP && sim_cameo) {   /* Meow drops in, and leaves again */
        uint32_t t = now - sim_gap_t0;
        int32_t in = (int32_t)(t < 600u ? t : 600u), out = t > 5900u ? (int32_t)(t - 5900u) : 0;
        mv_oy = 0;
        mv_h = 144;
        spr_flip = 1;                        /* he faces the bowl */
        cast_paint(EO_CAST_CAT, 1u, 240 - in * 170 / 600 + out * 170 / 700, 4, 16, 8);
        spr_flip = 0;   /* as big as the bowl: he looms */
    }
    q = sim_quip(tx);
    if (sim_alert) {                         /* this one matters: a big card across the screen, its border flashing (red: the spirits are angry) */
        uint16_t bc = sim_alert == 2u ? ((now / 220u) & 1u ? EO_RGB(255, 60, 60) : C_HI) : ((now / 220u) & 1u ? C_HI : C_AMB);
        cv_rect(10, 4, 220, 100, bc);
        cv_rect(14, 8, 212, 92, C_BLACK);
        if (eo_strlen(q) > 27u)
            wrap_text(22, 16, 196, &FONT_B, q, C_WHITE, 24);
        else
            wrap_text(22, 12, 196, &FONT_L, q, C_WHITE, 28);
        return;
    }
    {
        int32_t yb;
        const felucca_font_t *fn = &FONT_B;
        int32_t lh = 20;
        wrap_dry = 1;
        yb = wrap_text(142, 12, 90, &FONT_B, sim_full ? sim_full : q, C_WHITE, 20);
        if (yb > 126) {                                       /* a long line: the small font, so none of it is cut off */
            fn = &FONT_S;
            lh = 15;
            yb = wrap_text(142, 12, 90, fn, sim_full ? sim_full : q, C_WHITE, lh);
        }
        wrap_dry = 0;
        cv_rect(136, 6, 102, yb - 6, C_BLACK);                /* her words sit on a black card over her bowl */
        wrap_text(142, 12, 90, fn, q, (tx == TX_NICE || tx == TX_WATCH || ui.state == ST_GAP) ? C_HI : C_WHITE, lh);
    }
}

static uint8_t sim_dream;                /* this frame is the sleepy screen: gray-blue, rippling like a sine wave running down it */
static void sim_dream_post(int32_t y0, uint32_t now)   /* tint the band just drawn and shear each row sideways by a travelling sine wave */
{
    static uint16_t row[240];
    uint32_t x, y;
    for (y = 0; y < cv_h; y++) {
        uint16_t *p = &cv_px[y * cv_w];
        float ph = 0.09f * (float)((int32_t)y + y0) - (float)now * 0.0035f;
        int32_t off = (int32_t)(5.0f * fm_sinf(ph) + 0.5f) + (cv_w == 240u ? 0 : 0);
        for (x = 0; x < cv_w && x < 240u; x++) {
            uint32_t v = swap16(p[x]), r = (v >> 11) * 2u, g = (v >> 5) & 63u, b = (v & 31u) * 2u, l = (r * 77u + g * 150u + b * 29u) >> 8;
            uint32_t R = l * 5u / 8u, G = l * 6u / 8u, B = l * 7u / 8u + 4u;     /* gray with a blue tint, a little dimmer */
            R = R > 63u ? 63u : R;
            G = G > 63u ? 63u : G;
            B = B > 63u ? 63u : B;
            row[x] = swap16((uint16_t)(((R >> 1) << 11) | (G << 5) | (B >> 1)));
        }
        for (x = 0; x < cv_w && x < 240u; x++) {
            int32_t sx = (int32_t)x - off;
            sx = sx < 0 ? 0 : sx >= (int32_t)cv_w ? (int32_t)cv_w - 1 : sx;
            p[x] = row[sx];
        }
    }
}
static uint32_t sim_bub_live;
static void sim_bub_draw(int32_t y0, uint32_t now)   /* the bubbles, in this band: they rise about 2.4 s, wobbling a little */
{
    uint32_t i;
    for (i = 0; i < SIM_NB; i++) {
        int32_t age, y, x, sz;
        if (!sim_bub[i].t0 || now < sim_bub[i].t0)
            continue;
        age = (int32_t)(now - sim_bub[i].t0);
        if (age > 2400) {
            sim_bub[i].t0 = 0;
            continue;
        }
        sim_bub_live = now;
        sz = sim_bub[i].sz;
        y = 240 - age * (95 + (int32_t)(sim_bub[i].seed % 40u)) / 1000 - y0;
        x = sim_bub[i].x + (int32_t)(4.0f * fm_sinf((float)age * 0.006f + (float)sim_bub[i].seed));
        if (y + sz < 0 || y >= (int32_t)cv_h)
            continue;
        cv_rect(x, y, sz, sz, EO_RGB(150, 200, 255));
        cv_rect(x + 1, y + 1, sz - 2, sz - 2, SIM_COL[sim_bub[i].w]);
        cv_rect(x + 1, y + 1, 2, 2, C_WHITE);
    }
}
static void sim_blit(int32_t x, int32_t y)
{
    if (set.game == GAME_SIMON)
        sim_bub_draw(y, plat_ms());
    if (sim_dream)
        sim_dream_post(y, plat_ms());
    cv_blit(x, y);
}
static void draw(void)
{
    sim_dream = (uint8_t)(set.game == GAME_SIMON && ui.state == ST_LISTEN && seq_on && sim_sleepy);
    char t[48], n[12];
    uint32_t i, now = plat_ms(), pct = plat_master() * 100u / 4096u, bright, mid, dim, tx = eo_text();
    if (ui.state == ST_MOVIE) {
        mv_bands(mv_scene, now);
        return;
    }
    if (ui.state == ST_SIMONE) {
        mv_bands(sim_scene, now);
        return;
    }
    if (ui.state == ST_SELECT) {
        mv_bands(sel_scene, now);
        return;
    }
    if (ui.state == ST_INTRO || ui.state == ST_CONTROLS) {
        draw_narration(now);
        return;
    }
    if (set.game == GAME_DOGTOR) {
        draw_dogtor(now, pct);
        return;
    }
    if (eo_round_screen()) {
        draw_round(now, pct);
        return;
    }
    key_levels(&bright, &mid, &dim);
    if (sim_crystal_on()) {                  /* the crystal ball: all three bands blue, nothing but bubbles and the word */
        int32_t cw = sim_crystal_word();
        cv_begin(240, 36, SIM_BLUE);
        sim_crystal_layer(now, 0, -1);
        sim_blit(0, 0);
        cv_begin(240, 144, C_BLACK);         /* the word is drawn once on black, to be painted again bigger */
        if (cw >= 0)
            sim_crystal_mask((uint32_t)cw);
        cv_begin(240, 144, SIM_BLUE);
        sim_crystal_layer(now, 36, cw);
        sim_blit(0, 36);
        cv_begin(240, 60, SIM_BLUE);
        sim_crystal_layer(now, 180, -1);
        vol_overlay(36, now, pct);
        sim_blit(0, 180);
        return;
    }
    /* the header: the name, or a message for a moment */
    cv_begin(240, 36, C_BLACK);
    if (now < ui.msg_until) {
        int32_t x = cv_text(10, 6, &FONT_B, ui.msg[0], C_WHITE);
        cv_text(x + 8, 6, &FONT_B, ui.msg[1], C_HI);
    } else if (set.game == GAME_COPYCAT) {   /* Copycat: where you are, not the app's name */
        char h[24], hn[12];
        h[0] = 0;
        text_cat(h, "COPYCAT - LEVEL ");
        int_text((int32_t)eo_level(), hn);
        text_cat(h, hn);
        cv_text(10, 6, &FONT_B, h, C_WHITE);
        {   /* the root and the voice together: C4 - MEOW (the root moves from level 29) */
            char r[32], rn[12];
            r[0] = 0;
            if (eo_moves()) {
                text_cat(r, "MOVING");
            } else {
                note_name(KEY_NOTE0 + (int32_t)set.root, rn);
                text_cat(r, rn);
            }
            text_cat(r, " - ");
            text_cat(r, eo_voice_name[set.voice < VOICES ? set.voice : 0u]);
            cv_text(230 - text_w(&FONT_XS, r), 10, &FONT_XS, r, C_GRAY);
        }
    } else {
        int32_t x = set.game == GAME_SIMON ? 0 : cv_text(10, 6, &FONT_B, APP_NAME, C_WHITE);
        if (set.game == GAME_SIMON) {            /* she has the strip to herself (her crystal ball sits up here), and the numbers */
            sim_sprite(now, -36, 36);            /* the numbers live up here: LEVEL 1, then LEVEL 1  3/5 in a round */
            char h[32], hn[12];
            h[0] = 0;
            if (ui.state == ST_READY || ui.state == ST_LEVELUP) {
                text_cat(h, "LEVEL ");
                int_text((int32_t)eo_level(), hn);
                text_cat(h, hn);
            } else {
                text_cat(h, "LEVEL ");
                int_text((int32_t)eo_level(), hn);
                text_cat(h, hn);
                text_cat(h, "  ");
                int_text((int32_t)ans_len, hn);
                text_cat(h, hn);
                text_cat(h, "/");
                int_text((int32_t)simon_goal(eo_level()), hn);
                text_cat(h, hn);
            }
            cv_text(232 - text_w(&FONT_S, h), 3, &FONT_S, h, C_AMB);
            if (!set.mode && ui.state != ST_READY && ui.state != ST_LEVELUP) {    /* the wishes left */
                uint32_t w;
                for (w = 0; w < 3u; w++)
                    sim_star(205 + (int32_t)w * 10 - 2, 25, w < sim_wishes ? C_HI : C_DIM);
            }
        } else {
            const char *vn = eo_voice_name[eo_voice < VOICES ? eo_voice : 0u];
            cv_text(x + 8, 10, &FONT_XS, EO_VERSION, C_DIM);
            cv_text(230 - text_w(&FONT_XS, vn), 10, &FONT_XS, vn, C_GRAY);
        }
    }
    sim_blit(0, 0);
    /* the middle: the game and its level, the big word, the progress, the numbers, a hint */
    cv_begin(240, 144, C_BLACK);
    ui.redraw_cat = 0;
    if (set.game == GAME_COPYCAT) {
        cc_band(now);
        sim_blit(0, 36);
    } else if (ui.state == ST_LEVELUP && set.game == GAME_SIMON) {
        sim_up_band(now);
        sim_blit(0, 36);
    } else if (ui.state == ST_LEVELUP) {     /* GOOD JOB: the new level, and what is different about it */
        const char *const *ln = eo_up_text[set.game][eo_level()];
        ctext(4, &FONT_L, TX[TX_GOOD], C_HI);
        t[0] = 0;
        text_cat(t, "LEVEL ");
        int_text((int32_t)eo_level(), n);
        text_cat(t, n);
        ctext(44, &FONT_M, t, C_WHITE);
        for (i = 0; i < 3u && ln[i]; i++)
            ctext(80 + (int32_t)i * 15, &FONT_XS, ln[i], i ? C_GRAY : C_WHITE);
        ctext(128, &FONT_XS, "PLAY CONTINUE      HOME STOP", C_DIM);
        sim_blit(0, 36);
    } else if (set.game == GAME_SIMON) {
        int32_t ew = sim_eyes_word();
        if (ew >= 0) {                       /* use your eyes: the silent note's word, big and rippling, over her bowl */
            cv_begin(240, 144, C_BLACK);
            sim_crystal_mask((uint32_t)ew);
            cv_begin(240, 144, C_BLACK);
        }
        sim_band(now);
        if (ew >= 0)
            sim_crystal_paint(now, (uint32_t)ew);
        sim_blit(0, 36);
    } else {
    t[0] = 0;
    text_cat(t, eo_game_name[set.game]);
    text_cat(t, "   LEVEL ");
    int_text((int32_t)eo_level(), n);
    text_cat(t, n);
    if (set.game == GAME_COPYCAT)
        text_cat(t, copycat_level(eo_level())->lit == LIT_ALL ? "   LIGHTS ON" :
                    copycat_level(eo_level())->lit == LIT_CHROMATIC ? "   CHROMATIC LIT" : "   NO LIGHTS");
    else {
        text_cat(t, "   ");
        int_text(simon_level(eo_level())->n, n);
        text_cat(t, n);
        text_cat(t, " KEYS");
    }
    ctext(2, &FONT_XS, t, C_GRAY);
    ctext(18, &FONT_L, TX[tx], (tx == TX_ROOT || tx == TX_LISTEN || tx == TX_WATCH || tx == TX_NICE) ? C_HI : C_WHITE);
    if (ans_len && ui.state != ST_READY) {   /* one box per note: filled as you play them right */
        int32_t pitch = 220 / (int32_t)ans_len > 18 ? 18 : 220 / (int32_t)ans_len, w = (int32_t)ans_len * pitch - 4;
        int32_t x = (240 - w) / 2;
        for (i = 0; i < ans_len; i++)
            cv_rect(x + (int32_t)i * pitch, 56, pitch - 4, 10,
                    (ui.state == ST_TURN || ui.state == ST_WIN) && i < ans_pos ? C_HI : C_LINE);
    }
    t[0] = 0;
    if (set.game == GAME_COPYCAT) {
        text_cat(t, "ROOT ");
        if (!eo_moves()) {                   /* a moving root is found by ear and by its lit key, not by its name */
            note_name(KEY_NOTE0 + (int32_t)set.root, n);
            text_cat(t, n);
        }
        text_cat(t, "     STREAK ");
        int_text((int32_t)streak, n);
        text_cat(t, n);
    } else if (ui.text == TX_OVER) {
        text_cat(t, "SCORE ");
        int_text((int32_t)eo_score, n);
        text_cat(t, n);
    } else if (ans_len) {
        text_cat(t, "LENGTH ");
        int_text((int32_t)ans_len, n);
        text_cat(t, n);
        if (eo_level() < SIMON_LEVELS) {
            text_cat(t, "/");
            int_text((int32_t)simon_goal(eo_level()), n);
            text_cat(t, n);
        }
    }
    if (t[0])
        text_cat(t, "     ");
    text_cat(t, "BEST ");
    int_text(set.best[set.game], n);
    text_cat(t, n);
    ctext(72, &FONT_S, t, C_AMB);
    t[0] = 0;                                /* the way to the next level */
    if (eo_level() >= eo_levels[set.game]) {
        text_cat(t, "TOP LEVEL");
    } else if (set.game == GAME_COPYCAT) {
        text_cat(t, "LEVEL UP: ");
        int_text(set.prog[GAME_COPYCAT], n);
        text_cat(t, n);
        text_cat(t, "/");
        int_text((int32_t)eo_rounds(GAME_COPYCAT), n);
        text_cat(t, n);
        text_cat(t, " RIGHT");
    } else {
        text_cat(t, "LEVEL UP: A SEQUENCE OF ");
        int_text((int32_t)simon_goal(eo_level()), n);
        text_cat(t, n);
    }
    ctext(91, &FONT_XS, t, C_DIM);
    if (ui.state == ST_READY) {
        ctext(110, &FONT_XS, "PLAY START      PRESETS GAME", C_DIM);
        ctext(125, &FONT_XS, set.game == GAME_COPYCAT ? "SELECT ROOT      K1 LEVEL" : "K1 LEVEL", C_DIM);
    } else if (ui.state == ST_TURN && set.game == GAME_COPYCAT)
        ctext(118, &FONT_XS, "PLAY AGAIN  SELECT STEP  HOME STOP", C_DIM);
    else
        ctext(118, &FONT_XS, "HOME STOP", C_DIM);
    sim_blit(0, 36);
    }
    /* the foot: Simon's keys (Copycat has none on the screen), and the volume for a moment after it changes */
    cv_begin(240, 60, C_BLACK);
    if (set.game == GAME_SIMON) {
        sim_keys(-30);
        sim_sprite(now, 144, 60);        /* her bowl comes down over the keys: the foot's part of her */
    } else if (ui.state == ST_READY)
        cc_facts();
    vol_overlay(36, now, pct);
    sim_blit(0, 180);
}

void app_splash(void)                    /* the very first frame: black, not fm1-base's white title card */
{
    lcd_fill(0, 0, 240, 240, 0);
}

void app_ui_init(void)
{
    lcd_fill(0, 0, 240, 240, C_BLACK);
    ui.redraw = 1;
}

void app_frame(void)
{
    static uint32_t last_pct, last_bright, last_tx;
    uint32_t now = plat_ms(), pct = plat_master() * 100u / 4096u, bright, mid, dim, tx;
    input();
    if (ui.state == ST_SIMONE)
        sim_tick(now);
    if (ui.state == ST_MOVIE || ui.state == ST_SELECT || ui.state == ST_SIMONE) {   /* the movie, the select and Simone's show: a frame about every 33 ms */
        static uint32_t last_mv;
        if (ui.state == ST_MOVIE) {
            movie_tick(now);
        } else if (ui.state == ST_SELECT) {              /* the select's pulse: a plucked G2 bass on each eighth note at 94 bpm */
            static uint32_t last_bt = 0xFFFFFFFFu;
            uint32_t bt = now * SEL_BPM8 / 60000u;
            if (bt != last_bt) {
                last_bt = bt;
                eo_bass_note = 43;
                eo_bass_vel = (bt & 1u) ? 0.75f : 1.0f;      /* the downbeat a little harder */
                eo_bass_trig++;
            }
        }
        if (now / 33u != last_mv) {
            last_mv = now / 33u;
            ui.redraw = 1;
            if (ui.state == ST_MOVIE)
                lights();                    /* the wave moves with no input */
        }
    }
    key_levels(&bright, &mid, &dim);
    if (bright != last_bright) {             /* a sequence's notes change the lit keys with no input */
        last_bright = bright;
        ui.redraw = 1;
    }
    tx = eo_text();
    if (tx != last_tx) {                     /* ROOT gives way to LISTEN with no input either */
        if (last_tx == TX_ROOT && tx == TX_LISTEN)
            app_say("LISTEN", "");
        last_tx = tx;
        ui.redraw = 1;
    }
    if (eo_win_pending) {                    /* a round won: its sound comes when you have let go of the last key */
        if (ui.keys)
            eo_win_due = now + 250u;
        if (now >= eo_win_due || now >= eo_win_at + 1500u)
            eo_win_sound();
    }
    if (seq_done) {                          /* a sequence has ended: what comes next depends on what it was */
        seq_done = 0;
        if (ui.state == ST_LISTEN) {
            ui.state = ST_TURN;
            ui.text = TX_TURN;
            ans_pos = 0;
            sim_turn_t0 = now;
            if (set.game == GAME_COPYCAT && !copycat_level(eo_level())->drone)
                eo_drone_set(-1, 0.0f);      /* the top level: the root is not kept for you */
            app_say("YOUR TURN", "");
        } else if (ui.state == ST_WIN) {
            if (eo_levelup) {
                eo_enter_levelup();
            } else if (set.game == GAME_DOGTOR) {
                ui.state = ST_GAP;               /* the title card, and the ostinato under it */
                gap_until = now + 2500u;
                dt_card_until = gap_until;
            } else {
                ui.state = ST_GAP;
                gap_until = now + 700u;
                if (set.game == GAME_SIMON) {    /* the pause between rounds: she says something, typed out with bubbles; sometimes Meow drops in */
                    sim_cameo = (uint8_t)(SIM_FORCE == 7u || (!set.mode && eo_level() >= 2u && !SIM_FORCE && !sim_cameo_last && sim_fx_rand() % 6u == 0u));
                    sim_cameo_last = sim_cameo;
                    sim_gap_t0 = now;
                    sim_typed_n = 0;
                    gap_until = now + (sim_cameo ? 6600u : 3800u);
                }
            }
        } else if (ui.state == ST_MISS && set.mode && set.game == GAME_COPYCAT) {
            ui.state = ST_TURN;              /* practice: the same melody, played from the start again */
            ui.text = TX_TURN;
            ans_pos = 0;
            ui.result = 0;
            if (!copycat_level(eo_level())->drone)
                eo_drone_set(-1, 0.0f);
            ui.kb_show = 0;
            if (eo_assist()) {               /* it has been a while: the cat plays it again, lit, and the keyboard stays */
                eo_say("Sssss... let me hit the lights for you.", 6000u);
                ui.kb_show = 1;
                ui.kb_at = now;
                copycat_play(1u);
            } else {
                eo_say("Again. Focus.", 2500u);
            }
            app_say("TRY AGAIN", "");
        } else if (ui.state == ST_MISS) {
            eo_drone_set(-1, 0.0f);
            if (set.game == GAME_SIMON) {
                if (!set.mode && sim_wishes) {   /* you have angered the spirits: a wish, or give up */
                    ui.state = ST_WISH;
                    ui.text = TX_MISS;
                    sim_wish_t0 = now;
                } else {
                    simon_over();
                }
            } else {
                if (save_pending) {              /* the streak has ended: a new best is kept now, in the quiet */
                    save_pending = 0;
                    store_now();
                }
                ui.state = ST_GAP;
                gap_until = now + (set.game == GAME_DOGTOR ? 2500u : 900u);
                if (set.game == GAME_DOGTOR)
                    dt_card_until = gap_until;
            }
        }
        ui.redraw = 1;
    }
    if (ui.state == ST_GAP && now >= gap_until) {   /* the pause is over: the next round */
        if (set.game == GAME_SIMON)
            simon_grow();
        else if (set.game == GAME_DOGTOR)
            dogtor_round(0u);
        else
            copycat_round();
    }
    if (set.game == GAME_SIMON && (ui.state <= ST_LEVELUP || ui.state == ST_WISH)) {
        sim_game_tick(now);
        eo_sleep = (uint8_t)(ui.state == ST_LISTEN && seq_on && sim_sleepy);
        eo_water = (uint8_t)(ui.state == ST_LISTEN && seq_on && (sim_crystal || sim_sleepy));
        eo_rev = (uint8_t)(set.game == GAME_SIMON && (ui.state == ST_TURN || ui.state == ST_WIN || ui.state == ST_MISS) && sim_back && !seq_on);
    } else {
        eo_water = 0, eo_rev = 0, eo_sleep = 0;
    }
    if (set.game == GAME_DOGTOR && ui.state != ST_INTRO && ui.state != ST_CONTROLS && ui.state != ST_MOVIE && ui.state != ST_SELECT) {
        static uint32_t last_breath;
        uint32_t br = now / 600u;
        static uint32_t last_pose = 99u;
        uint32_t pz = dt_pose(now);
        dogtor_tick(now);
        if (pz != last_pose) {               /* a new face: at once, not on the next breath */
            last_pose = pz;
            ui.redraw = 1;
        }
        if (br != last_breath) {             /* the dog breathes: a pixel up, a pixel down */
            last_breath = br;
            ui.redraw = 1;
        }
    }
    speak();
    if (pct != last_pct || (ui.msg_until && now >= ui.msg_until)) {
        static uint8_t vol_seen;
        if (pct != last_pct && vol_seen)
            ui.vol_until = now + 1200u;      /* the volume shows for a moment, then goes */
        vol_seen = 1;
        last_pct = pct;
        if (now >= ui.msg_until)
            ui.msg_until = 0;
        ui.redraw = 1;
    }
    if (ui.vol_until && now >= ui.vol_until) {   /* the moment is over: draw without it */
        ui.vol_until = 0;
        ui.redraw = 1;
    }
    if (ui.say[0] && ui.say_until && now >= ui.say_until) {   /* the cat has finished speaking */
        ui.say_until = 0;
        ui.redraw = 1;
    }
    if (ui.kb_show && now - ui.kb_at < 330u)
        ui.redraw = 1;                       /* the keyboard is sliding in */
    if (ui.state == ST_INTRO || ui.state == ST_CONTROLS) {   /* the cat talks: its mouth, and PRESS ANY KEY, blink */
        static uint32_t last_talk;
        uint32_t tk = now / 170u + now / 500u * 1000u;
        if (tk != last_talk) {
            last_talk = tk;
            ui.redraw = 1;
        }
    }
    if (set.game == GAME_COPYCAT && ui.state != ST_INTRO && ui.state != ST_CONTROLS && ui.state != ST_MOVIE && ui.state != ST_SELECT) {   /* the cat's animation */
        static uint32_t last_pose = 99u, last_frame;
        uint32_t pose = eo_pose(now), fr = eo_frame(now, pose);
        if (copycat_level(eo_level())->contour) {    /* the contour: a note comes out, or the sounding one changes */
            static int32_t last_cur = -2;
            static uint32_t last_heard = 99u;
            int32_t cur;
            uint32_t hd = eo_heard(&cur);
            if (cur != last_cur || hd != last_heard || ui.state != ST_LISTEN) {
                if (cur != last_cur || hd != last_heard)
                    ui.redraw = 1;
                last_cur = cur;
                last_heard = hd;
            }
        }
        if (pose != last_pose || fr != last_frame) {
            last_pose = pose;
            last_frame = fr;
            ui.redraw_cat = 1;
        }
        if (ui.state == ST_LEVELUP && now >= ui.up_until && ui.up_until) {   /* the celebration is over: the card */
            ui.up_until = 0;
            ui.redraw = 1;
        }
    }
    if (ui.redraw) {
        ui.redraw = 0;
        draw();
    } else if (ui.redraw_cat) {
        ui.redraw_cat = 0;
        draw_cat_only(now);
    }
}

#ifdef PLAT_HOST
int app_expect(const char *what, int *got)
{
    uint32_t bright, mid, dim;
    key_levels(&bright, &mid, &dim);
    if (!strcmp(what, "game"))
        *got = set.game;
    else if (!strcmp(what, "root"))
        *got = set.root;
    else if (!strcmp(what, "voice"))
        *got = set.voice;
    else if (!strcmp(what, "speed"))
        *got = set.speed * 5;                /* as a percent */
    else if (!strcmp(what, "mode"))
        *got = set.mode;
    else if (!strcmp(what, "kb"))
        *got = ui.kb_show;
    else if (!strcmp(what, "tries"))
        *got = (int)eo_tries;
    else if (!strcmp(what, "top"))
        *got = set.top[set.game];
    else if (!strcmp(what, "open"))
        *got = (int)eo_open(set.game);
    else if (!strcmp(what, "unlock")) {  /* a test hook: every level completed, so every level is open */
        set.top[GAME_SIMON] = SIMON_LEVELS;
        set.top[GAME_COPYCAT] = COPYCAT_LEVELS;
        set.top[GAME_DOGTOR] = DOGTOR_LEVELS;
        *got = COPYCAT_LEVELS;
    } else if (!strcmp(what, "round_root"))
        *got = eo_croot();
    else if (!strcmp(what, "level"))
        *got = (int)eo_level();
    else if (!strcmp(what, "best"))
        *got = set.best[set.game];
    else if (!strcmp(what, "streak"))
        *got = (int)streak;
    else if (!strcmp(what, "score"))
        *got = (int)eo_score;
    else if (!strcmp(what, "prog"))
        *got = set.prog[set.game];
    else if (!strcmp(what, "pending"))
        *got = eo_win_pending;
    else if (!strcmp(what, "dirty"))
        *got = set_dirty;
    else if (!strcmp(what, "note"))
        *got = voice_note;
    else if (!strcmp(what, "drone"))
        *got = eo_drone_note;
    else if (!strcmp(what, "state"))
        *got = ui.state;
    else if (!strcmp(what, "smp"))
        *got = eo_smp_run;
    else if (!strcmp(what, "bass"))
        *got = eo_bass_note;
    else if (!strcmp(what, "simcard"))
        *got = sim_card;
    else if (!strcmp(what, "simtyped"))
        *got = (int)sim_typed(&sim_set[sim_card], plat_ms() - sim_t0);
    else if (!strcmp(what, "simmusic"))
        *got = sim_music;
    else if (!strcmp(what, "bend"))
        *got = (int)(eo_bend_v * 100.0f);
    else if (!strcmp(what, "sel"))
        *got = sel_cur;
    else if (!strcmp(what, "mvstage"))
        *got = mv_stage;
    else if (!strcmp(what, "text"))
        *got = (int)eo_text();
    else if (!strcmp(what, "pos"))
        *got = (int)ans_pos;
    else if (!strcmp(what, "len"))
        *got = (int)ans_len;
    else if (!strncmp(what, "ans", 3))      /* ansN: the round's Nth note, as its key */
        *got = ans[atoi(what + 3) % SIMON_MAX_LEN];
    else if (!strcmp(what, "dtwide")) {      /* the widest story line, in pixels (title cards are 214 wide inside the frame) */
        uint32_t a, b, c;
        int w = 0;
        for (a = 0; a < DOGTOR_LEVELS; a++)
            for (b = 0; b < 8u; b++)
                for (c = 0; c < 2u; c++)
                    if (text_w(&FONT_B, dt_story[a][b][c]) > w)
                        w = text_w(&FONT_B, dt_story[a][b][c]);
        for (a = 0; a < DOGTOR_LEVELS - 1u; a++)
            for (c = 0; c < 2u; c++)
                if (text_w(&FONT_B, dt_cliff[a][c]) > w)
                    w = text_w(&FONT_B, dt_cliff[a][c]);
        *got = w;
    } else if (!strcmp(what, "cups")) {
        *got = dt_cups;
    } else if (!strcmp(what, "dtkey"))
        *got = dt_key_now;
    else if (!strcmp(what, "dtq"))
        *got = dt_q_now;
    else if (!strcmp(what, "dtroot"))
        *got = dt_root_now;
    else if (!strcmp(what, "chord0"))
        *got = eo_chord_note[0];
    else if (!strcmp(what, "chord2"))
        *got = eo_chord_note[2];
    else if (!strcmp(what, "bright"))
        *got = (int)bright;
    else if (!strcmp(what, "mid"))
        *got = (int)mid;
    else if (!strcmp(what, "gap"))           /* the lapse's length (0: none this round), and which kind */
        *got = (int)sim_gap_len;
    else if (!strcmp(what, "gaprestart"))
        *got = (int)sim_gap_restart;
    else if (!strcmp(what, "gapat"))
        *got = (int)sim_gap_at;
    else if (!strcmp(what, "seqn"))
        *got = (int)seq_n;
    else if (!strncmp(what, "fx", 2)) {      /* a test hook: fxN forces her effect N from now on (0: leave it to chance) */
        sim_fx_force = (uint8_t)atoi(what + 2);
        *got = sim_fx_force;
    }
    else if (!strcmp(what, "seqpos"))        /* ms into the sequence now */
        *got = (int)seq_pos;
    else if (!strcmp(what, "back"))          /* this round is played backwards */
        *got = (int)sim_back;
    else if (!strcmp(what, "wishes"))        /* the wishes left */
        *got = (int)sim_wishes;
    else if (!strcmp(what, "seen"))          /* the tricks shown so far this game, a bit each */
        *got = (int)sim_seen;
    else if (!strcmp(what, "crystal"))       /* this round is in the crystal ball */
        *got = (int)sim_crystal;
    else if (!strcmp(what, "stall"))         /* this round's lapse is a held note */
        *got = (int)sim_gap_stall;
    else if (!strcmp(what, "pre"))           /* Simone's pre-roll is on */
        *got = (int)sim_in_preroll();
    else if (!strcmp(what, "wave")) {        /* the movie's wave: the crest's key mask now */
        uint32_t wb = 0, wm = 0, wd = 0;
        movie_wave(&wb, &wm, &wd);
        *got = (int)wb;
    }
    else if (!strcmp(what, "dim"))
        *got = (int)dim;
    else
        return 0;
    return 1;
}
#endif
