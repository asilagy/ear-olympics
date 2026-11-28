/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 asilagy */
/* Name That Interval (Van): the intervals, the levels, and what Van says. The game's code is eo_van_game.h.
 * Every line here is also in the Van dialogue sheet (kept in sync): when a line changes, the sheet changes with it.
 * A card that starts with '*' is an action line (something the art does not show), and one that starts with '#' is Van's
 * symbol-font gibberish. '~' is e-acute, as elsewhere. */
#pragma once
#include <stdint.h>

#define VAN_LEVELS 8u
#define VAN_ROUNDS 6u                    /* intervals named right to graduate a level */
#define VAN_MAX_KEYS 4u
#define VAN_BEATS 3u                     /* chat beats between questions (a first guess: it sets how much of a visit is hangout) */
static const uint8_t van_answer_key[VAN_MAX_KEYS] = {12, 14, 16, 18};   /* A F4, B G4, C A4, D B4: the same keys as Chord Dogtor */

typedef struct { uint8_t semi; const char *name; const char *joke; } van_iv_t;   /* index = semitones - 1 */
static const van_iv_t van_iv[12] = {
    {1, "MINOR 2ND", 0}, {2, "MAJOR 2ND", 0}, {3, "MINOR 3RD", 0}, {4, "MAJOR 3RD", 0}, {5, "FOURTH", 0},
    {6, "TRITONE", "Tritone? That's the one giving sus."}, {7, "FIFTH", "A fifth's the chill one."}, {8, "MINOR 6TH", 0},
    {9, "MAJOR 6TH", 0}, {10, "MINOR 7TH", 0}, {11, "MAJOR 7TH", 0}, {12, "OCTAVE", "Octave. Same note, bigger vibes."}};

typedef struct {
    uint8_t keys;                        /* how many answer keys are lit (never more than four) */
    uint8_t pn;                          /* how many intervals are in the pool */
    uint8_t close;                       /* 1: the wrong answers are the nearest neighbours of the right one */
    uint8_t cmp;                         /* 1: no names yet: two gaps, which is bigger (or smaller)? mind: the least difference between them */
    uint8_t mind;
    uint8_t pool[12];                    /* semitones, the new ones last */
} van_level_t;
static const van_level_t van_level_tab[VAN_LEVELS] = {
    {2, 4, 0, 1, 5, {12, 7, 4, 2}},                            /* 1: which gap is bigger or smaller (no names), far apart */
    {2, 6, 0, 1, 2, {12, 7, 5, 4, 3, 2}},                      /* 2: the same, closer together */
    {2, 2, 0, 0, 0, {12, 7}},                                  /* 3: now they have names: octave, fifth */
    {3, 4, 0, 0, 0, {12, 7, 4, 3}},
    {3, 6, 0, 0, 0, {12, 7, 4, 3, 5, 2}},
    {4, 8, 0, 0, 0, {12, 7, 4, 3, 5, 2, 9, 10}},
    {4, 10, 1, 0, 0, {12, 7, 4, 3, 5, 2, 9, 10, 1, 6}},
    {4, 12, 1, 0, 0, {12, 7, 4, 3, 5, 2, 9, 10, 1, 6, 8, 11}},
};
static const van_level_t *van_level(uint32_t l) { return &van_level_tab[(l >= 1u && l <= VAN_LEVELS ? l : 1u) - 1u]; }

/* ---- the lines */
enum { P_IDLE, P_LOUNGE, P_TALK, P_LAUGH, P_COOKED, P_TAPE, P_SNACK, P_PHONE, P_ASLEEP, P_FISH };   /* the poses: the cast's two, then Van's seven */
enum { K_SAY, K_ASK, K_MEOW, K_HUM, K_FISH };
typedef struct {
    const char *c[4];                    /* the cards, in order (the last is the question for an ask) */
    uint8_t kind, pose;
    const char *ch[3];                   /* an ask: the choices (short) ... */
    const char *re[3];                   /* ... and Van's reply to each */
} van_beat_t;

#define VAN_SAY(p, ...) {{__VA_ARGS__}, K_SAY, p, {0, 0, 0}, {0, 0, 0}}
static const van_beat_t van_beat[] = {
    /* normal things, odd */
    VAN_SAY(P_TALK, "Your pants have a little room in them. And you keep stuff in it. Wild."),
    VAN_SAY(P_TALK, "You cut the day into twenty-four pieces and then just follow them."),
    VAN_SAY(P_LOUNGE, "A beanbag has no beans. I checked. ...Still gonna sit on it."),
    VAN_SAY(P_TALK, "Door. A wall that opens. Sometimes you push, sometimes you pull. Nobody knows ahead of time."),
    VAN_SAY(P_TALK, "These are really good. Five. Great number."),
    VAN_SAY(P_TALK, "It rained on the window. By itself. Nobody asked."),
    VAN_SAY(P_TALK, "The moon's out again. Same one. Like it's nothing."),
    VAN_SAY(P_TALK, "Humans ask how you are and then just keep walking. Wild."),
    VAN_SAY(P_TALK, "Humans have a day where everyone eats the same cake and sings at it. And it's not even about the cake."),
    VAN_SAY(P_TALK, "You guys make water colder on purpose? ...Huh. Cool. Literally. Anyway, I'm good."),
    VAN_SAY(P_TALK, "How do you know when a sandwich is done? ...Mine just ends."),
    /* odd things, normal */
    VAN_SAY(P_TALK, "Mom runs the department that decides what counts as a distance. She's good at it."),
    VAN_SAY(P_FISH, "*The goldfish is at the glass.", "She's been like that since lunch."),
    VAN_SAY(P_COOKED, "Found a sock. ...Don't know if it's left or right foot. Good sock, though."),
    VAN_SAY(P_IDLE, "*The fridge hum changes pitch.", "Oh. It's in a new key. Good for it."),
    VAN_SAY(P_TALK, "I measured the couch once. Four tape-lengths. Then I forgot what a tape-length was. Anyway, it's still a couch."),
    {{"*Van and you watch the fish.", 0, 0, 0}, K_FISH, P_FISH, {0, 0, 0}, {0, 0, 0}},
    VAN_SAY(P_TALK, "Home's sun comes with a thirty-year warranty. Mom keeps the receipt."),
    VAN_SAY(P_TALK, "I requested a form. They sent back a form saying my request is a form. So technically I got what I asked for."),
    VAN_SAY(P_TALK, "Space is mostly nothing. Honestly? Less to carry."),
    VAN_SAY(P_TALK, "Sometimes I say a word until it stops being a word. Spoon. Spoon. Spoon. ...I think it's a different word now. I'm keeping it."),
    VAN_SAY(P_TAPE, "I had a dream I was a tape measure. I was really good at it."),
    VAN_SAY(P_TALK, "The couch is thinking about something. I can tell. I'm not gonna ask."),
    VAN_SAY(P_TALK, "Meow likes the warm spot on the couch. When I get up, he takes it. When I sit back down, he moves one inch. That's the deal. We both know the deal."),
    VAN_SAY(P_SNACK, "We have a word for when the snack's gone but your hand keeps going. I won't say it. Eleven syllables and one of them is sad."),
    VAN_SAY(P_SNACK, "I was gonna do my assignment first. Then I had a snack. Now it's a snack day."),
    VAN_SAY(P_TALK, "On my planet we sort food by how loud it is to eat. Quiet food, medium food, Tuesday food.",
            "Tuesday food is leftovers. My fifth birthday was all Tuesday food. They measured me against a wall.",
            "I came out, like, a fifth. Mom cried. Not a bad cry. ...Anyway. Hot dogs."),
    /* measuring */
    VAN_SAY(P_TAPE, "The kilogram used to be a metal cylinder. Now it's a number. The cylinder's still around. I felt that."),
    VAN_SAY(P_SNACK, "An inch is three barley grains. I tried it with chips. Different inch. Tastier."),
    VAN_SAY(P_TALK, "Atoms wiggle. I know an Adam. He wiggles when there's chips. Same thing, probably."),
    VAN_SAY(P_TALK, "A foot is a foot. Supposedly a king's. That's the whole system."),
    VAN_SAY(P_TAPE, "A meter is how far light goes in a really tiny moment. I asked how tiny. They had a number. I trust it. I'm the surveyor, not the number guy."),
    VAN_SAY(P_TALK, "Approximately is the best unit. It works everywhere."),
    VAN_SAY(P_TAPE, "My tape measure doesn't have numbers. I just kind of know. It's all feel, bub."),
    /* asks, and the ones with sound */
    {{"Real question. What's a hot dog?", 0, 0, 0}, K_ASK, P_TALK, {"A sandwich.", "Not a dog.", "Don't ask."},
     {"Okay, bold. I've been calling it a snack bun with a guy in it.",
      "Right? Back home we had a food called a glorp. Also not a dog. Nobody checked.", "Valid. Moving on."}},
    {{"Huh.", 0, 0, 0}, K_ASK, P_TALK, {"Huh.", "Yeah.", "..."}, {"Right?", "Right?", "Right?"}},
    {{"*Something meows, offstage.", "That's Meow. He doesn't like the fridge. I don't either, but I respect it.", 0, 0}, K_MEOW, P_IDLE, {0, 0, 0}, {0, 0, 0}},
    {{"*Van looks at the TV.", "Hold on. Come look at this.", "Don't say anything. Just watch.", 0}, K_HUM, P_IDLE, {0, 0, 0}, {0, 0, 0}},
    /* the fish: what Van says afterwards */
    VAN_SAY(P_FISH, "She does that. Just looks. Like she knows something.", "She doesn't. But she looks."),
    VAN_SAY(P_FISH, "I asked her what's coming. She said snacks.", "...She's never been wrong."),
    VAN_SAY(P_FISH, "They say a goldfish remembers three seconds. Is that true?", "...Or is that just something people tell fish?"),
};
#define VAN_NBEATS ((uint32_t)(sizeof van_beat / sizeof van_beat[0]))

static const char *const van_intro[4] = {"#", "...Hold on. Decoder was off.", "Sup duude, the name's Ivan Terval, but my bro-bros call me Van.",
                                         "Yeah, I'm not, like, the evolved kind. Sorry."};
static const char *const van_again = "Oh. Hey. You're back. Cool.";
static const char *const van_hum_after[2] = {"...", "...Yeah."};
static const char *const van_snack = "Wait, was that your snack break? ...Oh. Cool. Cool cool cool.";
static const char *const van_lead[4] = {"*Van pulls out the tape measure.", "Hold on. Gotta measure something.", "Okay, tape measure time.", "Two notes. How far apart?"};
static const char *const van_right[5] = {"Sick.", "Nice ears, dude.", "That's the one.", "No cap, that was bussin.", "Third? Fifth? Bro. You ate."};
static const char *const van_wrong[5] = {"Eh, close-ish.", "Not that one. No worries.", "Huh. Okay.", "Ooh. Cooked. Lowkey cooked.", "Sus. But I respect the confidence."};
static const char *const van_up[2] = {"New level, bro. Bigger gaps.", "You got better at distances. I'm not sure how I feel. Proud? Proud, probably."};

/* ---- conversation: beats come in threads (a few that follow from one another), with a link between threads, so it talks
 * like a person who wanders but does not just list facts. Each number is an index into van_beat[]; 255 ends a thread. */
#define VAN_THREADS 12u
static const uint8_t van_thread[VAN_THREADS][6] = {
    {0, 7, 1, 8, 3, 9},                  /* humans, and their habits */
    {5, 6, 19, 255, 0, 0},               /* the window: rain, moon, space */
    {11, 17, 18, 255, 0, 0},             /* home, and Mom's department */
    {12, 16, 38, 39, 40, 35},            /* the goldfish: at the glass, a long look, then what Van thinks of her */
    {2, 22, 23, 36, 255, 0},             /* the couch, and Meow */
    {25, 4, 24, 10, 255, 0},            /* snacks */
    {26, 34, 255, 0, 0, 0},              /* food on his planet, then hot dogs */
    {15, 33, 21, 255, 0, 0},             /* the tape measure */
    {27, 28, 29, 255, 0, 0},             /* what things are made of, and weighed by */
    {30, 31, 32, 255, 0, 0},             /* feet, meters, approximately */
    {13, 14, 20, 255, 0, 0},             /* small odd things in the room */
    {37, 255, 0, 0, 0, 0},               /* the TV (once, ever) */
};
static const char *const van_link_new[8] = {"Anyway.", "Okay, different thing.", "Oh. Unrelated.", "Hm. Random.", "So. Update.",
                                            "Wait. Thought of something.", "Where was I? ...Doesn't matter.", "Okay. New topic. Same couch."};
static const char *const van_link_in[6] = {"Also.", "Speaking of.", "Related.", "Oh, and.", "Same energy.", "One more."};
static const char *const van_link_back[4] = {"Anyway. Where was I?", "Right. So.", "Okay. Back to it.", "Sorry. You were saying nothing. Where was I?"};
static const char *const van_fish_ack[4] = {"...right.", "...yeah.", "...cool.", "...that's Simon~."};
