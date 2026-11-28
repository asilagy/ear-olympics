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
    {1, "MINOR 2ND", "A second. Tiny. Goldfish-memory tiny. Three of those and she's forgotten the first."}, {2, "MAJOR 2ND", "A second. Like the time kind. ...Both go by before I'm ready."}, {3, "MINOR 3RD", 0}, {4, "MAJOR 3RD", 0}, {5, "FOURTH", 0},
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
enum { K_SAY, K_ASK, K_MEOW, K_HUM, K_FISH, K_BLINK, K_TEN, K_PHONE };
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
    /* more of him: neighbours, clothes, sleep, music, time, the kitchen, names, a pigeon */
    VAN_SAY(P_TALK, "There's a guy upstairs who walks the same eight steps every night. I think it's a song."),
    VAN_SAY(P_TALK, "I've started counting them. Eight. Then a pause. Then eight."),
    VAN_SAY(P_TALK, "I think he's a drummer. Or lost. Either way, we get along."),
    VAN_SAY(P_SNACK, "I left a snack outside his door once. It was gone by morning. No note. Perfect."),
    VAN_SAY(P_TALK, "Humans put a second skin on their feet. Then another over that. For the floor."),
    VAN_SAY(P_COOKED, "I tried socks. They don't fit my feet. I have, like, extra foot."),
    VAN_SAY(P_TALK, "So now I wear one sock. On the antenna. It's cozy."),
    VAN_SAY(P_TALK, "Don't tell Mom. She'll say that's a hat."),
    VAN_SAY(P_TALK, "You lie down, close your eyes, and just leave for eight hours. Every night. Brave."),
    VAN_SAY(P_TALK, "I sleep standing up. It's less to explain."),
    VAN_SAY(P_ASLEEP, "I had a dream I was awake. Then I woke up and was also awake. Confusing day."),
    VAN_SAY(P_TALK, "Meow sleeps like he's been told to. Respect."),
    VAN_SAY(P_TALK, "Music is just air being shaken in a polite order."),
    VAN_SAY(P_TALK, "Back home we shake the air all the time. We just don't have an order."),
    VAN_SAY(P_TALK, "You guys have an order. And it makes people cry. In a good way. I asked."),
    VAN_SAY(P_TAPE, "The tape measure doesn't hear it. I'm working on that."),
    VAN_SAY(P_TALK, "Humans have a thing called later. You can keep stuff there. For a long time."),
    VAN_SAY(P_SNACK, "I put my assignment in later. It's doing great."),
    VAN_SAY(P_TALK, "Soon is like later, but nervous."),
    VAN_SAY(P_TALK, "I like now. Fewer moving parts."),
    VAN_SAY(P_TALK, "The fridge makes cold. On purpose. For free. Nobody thanks it."),
    VAN_SAY(P_TALK, "I thanked it. It hummed. I'm counting that."),
    VAN_SAY(P_TALK, "Then I opened it to say thanks again and the light came on. We're good."),
    VAN_SAY(P_TALK, "There's a drawer with only spoons. I respect that level of commitment."),
    VAN_SAY(P_TALK, "Everyone here has two names. One for friends, one for forms."),
    VAN_SAY(P_TALK, "I'm Van for friends. Ivan for forms. Terval for the cousins."),
    VAN_SAY(P_TALK, "There's an Ivan down the hall. Different Ivan. We nod. It's weird. It's nice."),
    VAN_SAY(P_TALK, "Mom says a name is just a sound that got lucky. ...She's good at lunches."),
    VAN_SAY(P_TALK, "The pigeon outside has opinions. I've asked it nothing. It tells me anyway."),
    VAN_SAY(P_TALK, "It does the walk-and-nod. That's the whole plan. I admire it."),
    VAN_SAY(P_SNACK, "I offered it a chip. It looked at me like I'd offered it a lease."),
    VAN_SAY(P_TALK, "We're not close. But we're aware of each other."),
    /* Van asks about you: your pick, and he answers (two cards where he has more to say; | splits them) */
    {{"Okay, I have to ask. What do you do all day?", 0, 0, 0}, K_ASK, P_TALK, {"Music.", "Not much.", "This."},
     {"Music. On purpose? ...Respect. That's basically my job, but with feelings.|Mine's measuring. Yours sounds better.", "Not much. That's a real answer. I'm going to think about it for a week.", "This? Hanging out with me? ...Best job I've heard."}},
    {{"What do you do when nobody's around?", 0, 0, 0}, K_ASK, P_TALK, {"Sing.", "Nap.", "Talk to myself."},
     {"Sing. Sure. I'd listen. ...From another room.", "A nap. Good. Great. Honestly the dream.", "Talking to yourself. I do it too. ...I'm in the room when I do it, so it counts."}},
    {{"I like sounds that come before you notice them. Like a fridge.", "Do you have a favorite sound?", 0, 0}, K_ASK, P_TALK, {"Rain.", "A chord.", "Silence."},
     {"Rain. Water falling on purpose. Classic.", "A chord. Of course. That's why you're here.|It's why I'm here too. Kind of. Mostly the snacks.", "Silence. I keep that one in the drawer with the spoons."}},
    {{"Are you more of a morning person or a night person?", 0, 0, 0}, K_ASK, P_ASLEEP, {"Morning.", "Night.", "Neither."},
     {"Morning. Wow. So you're up when it's bright. That sounds loud.", "Night. Same. The dark has better light.", "Neither. A third thing. A secret afternoon person. I see you."}},
    {{"If I learned one human thing, what should it be?", 0, 0, 0}, K_ASK, P_TALK, {"Whistling.", "Small talk.", "Napping."},
     {"Whistling. You make air go in a shape. I've been practicing. It's just air.", "Small talk. Like, 'so, weather.' ...Weather's great. We do it already.", "Napping. I'll be bad at it. I'll do it on purpose."}},
    {{"What's the weirdest thing about being human?", 0, 0, 0}, K_ASK, P_TALK, {"Feet.", "Alarms.", "Lunch."},
     {"Feet. Yeah. Two of them, in the dark, in shoes. Every day.", "Alarms. You choose a time to be woken up and then get mad at it. I love that.", "Needing lunch. Every day. Like, it keeps coming back. Honestly? Relatable."}},
    {{"Be honest. Do I talk too much about the measuring?", 0, 0, 0}, K_ASK, P_TAPE, {"Not at all.", "A little.", "Love it."},
     {"Good. That's my job. I'm here to measure stuff. It's the whole deal.", "Fair. I'll keep it short. ...No promises. Mom says I rehearse.", "Same. It's the best part of my day. Don't tell the couch."}},
    {{"Do you like snacks?", 0, 0, 0}, K_ASK, P_SNACK, {"Yes.", "Always.", "Define snack."},
     {"Yes. We're going to be good friends.", "Always. See? Aligned.", "Define snack. ...Mom does that too. We could be a family."}},
    /* the song: 81 asks, and every answer leads into the song (van_song), then 82 asks what you think */
    {{"*Van sits up. He seems nervous.", "Do you want to hear a song I'm working on?", 0, 0}, K_ASK, P_TALK, {"Sure.", "Please.", "No."},
     {"Sweet. Okay. Okay okay okay.@", "Please. He said please. Okay.@", "Too late. It's already happening.@"}},
    {{"So. What do you think?", 0, 0, 0}, K_ASK, P_TALK, {"Love it.", "It's long.", "Encore."},
     {"Really? ...Okay. I'm putting it in the fridge. For later.", "Yeah. It's a draft. A long draft. I'll cut it. ...Probably not.", "Encore? ...Okay. I only know this one. ...Mashed potatoes."}},
    /* 83: after the cat (thread 2): the petting */
    VAN_SAY(P_TALK, "Meow lets me pet him. That reminds me. I petted a human once.", "I asked first. He said, 'Dude, I'm a person.'", "I felt really bad. Then he gave me a pretzel, so it worked out."),
    /* 84: the staring contest (he loses, on purpose or not) */
    {{"*Van stares at you. He is not blinking.", "Staring contest. Go. Don't blink.", 0, 0}, K_BLINK, P_TALK, {0, 0, 0}, {0, 0, 0}},
    /* 85: the invitation to 10-ISH (inner: Van offers it after two interval questions, it is in no thread) */
    {{"Hey. Wanna see something cool?", "I got this console.", "Wanna play 10-ISH? Two paddles, one ball.", 0}, K_ASK, P_LOUNGE, {"Yes", "Later", 0},
     {"Sweet. You're the right paddle.|Just twist knob four. I'll go easy on you.", "No stress. It's not going anywhere. It's a TV.", 0}},
    /* 86: Mom calls */
    {{"*A tiny phone buzzes. Van picks it up.", "Hey, Mom. ...No, I'm at work. ...Measuring. ...Yes, things.", "...Yes, I ate. A snack is a meal if you're brave. ...Love you too. Bye. Bye. Bye.", "*Van hangs up. ...Mom says hi. And to wear a hat."}, K_PHONE, P_PHONE, {0, 0, 0}, {0, 0, 0}},
};
#define VAN_NBEATS ((uint32_t)(sizeof van_beat / sizeof van_beat[0]))

static const char *const van_intro[4] = {"#", "...Hold on. Decoder was off.", "Sup duude, the name's Ivan Terval, but my bro-bros call me Van.",
                                         "Yeah, I'm not, like, the evolved kind. Sorry."};
static const char *const van_again = "Oh. Hey. You're back. Cool.";
static const char *const van_hum_after[2] = {"...", "...Yeah."};
static const char *const van_snack = "Wait, was that your snack break? ...Oh. Cool. Cool cool cool.";
static const char *const van_song[] = {
    "*Van clears his throat. Twice. A third time, to be safe.",
    "Mashed potatoes. Mashed potatoes. Ma-ashed pot-tay-toes.",
    "Ma-ashed spoo-tay-toes. Doot doo doo.",
    "Mashed potatoes. Mashed potatoes. Ma-ashed pot-tay-toes. Ma-ashed spoo-tay-toes. Doot doo doo.",
    "*Van closes his eyes.",
    "Oh, they're soft. And they're warm. And they've never done me wrong.",
    "Mashed potatoes. Ma-ashed pot-tay-toes. Mashed potatoes, all night long.",
    "Ma-ashed spoo-tay-toes. Ma-ashed spoo-tay-toes. Doot doo doo. Doot doo doo-doo.",
    "This is the bridge. Butter. Butter. Bu-u-utter. Butter.",
    "Mashed potatoes. Ma-ashed pot-tay-toes. Ma-ashed spoo-tay-toes. Doot doo doo. Doot doo doo.",
    "Doot doo doo. Doot doo doo. Doot... doo... doooo.",
    "*A long note. It keeps going. He seems to be enjoying it.",
    "Mashed. Pot-a-toes.",
    "*Van opens his eyes. He seems surprised it's over.",
};
#define VAN_NSONG ((uint32_t)(sizeof van_song / sizeof van_song[0]))
static const char *const van_radio_down = "*Van turns the radio down.";
static const char *const van_blink_after[3] = {"*Van blinked.", "...You win.", "I wasn't even trying. ...Okay, I was a little."};
static const char *const van_pong_hint[3] = {"Knob four. The paddle sits still until you turn it.", "Knob four. It's shy. Give it a nudge.", "No pressure. I'll just play both sides."};
static const char *const van_lead[8] = {"*Van pulls out the tape measure.", "Hold on. Gotta measure something.", "Okay, tape measure time.", "Two notes. How far apart?",
                                         "Yep. Still my job. Measuring stuff.", "Don't mind me. I'm just here to measure stuff.", "That's what I'm here for. Gaps.", "Okay. Back to work. Sort of."};
static const char *const van_right[12] = {"Sick.", "Nice ears, dude.", "That's the one.", "That one landed.", "Okay, you've done this before.", "Yep.", "Ears: confirmed.",
                                           "Mm. That's it.", "You're better at this than my tape measure.", "Cool. Cool cool.", "Boom. That's a note.", "Correct. I think. I checked the tape."};
static const char *const van_wrong[10] = {"Eh, close-ish.", "Not that one. No worries.", "Huh. Okay.", "Ooh. That was a different one.", "Hm. Bold. I respect the confidence.",
                                           "Nope. But a good nope.", "Close. Or far. I wasn't measuring.", "That one was a different one.", "Interesting choice.", "Mm. Not quite."};
static const char *const van_up[2] = {"New level, bro. Bigger gaps.", "You got better at distances. I'm not sure how I feel. Proud? Proud, probably."};

/* ---- conversation: beats come in threads (a few that follow from one another), with a link between threads, so it talks
 * like a person who wanders but does not just list facts. Each number is an index into van_beat[]; 255 ends a thread. */
#define VAN_THREADS 27u
#define VAN_FIRST_INNER 82u                  /* beat 82 is only reached from the song, not from a thread */
static const uint8_t van_thread[VAN_THREADS][6] = {
    {0, 7, 1, 8, 3, 9},                  /* humans, and their habits */
    {5, 6, 19, 255, 0, 0},               /* the window: rain, moon, space */
    {11, 17, 18, 255, 0, 0},             /* home, and Mom's department */
    {12, 16, 38, 39, 40, 35},            /* the goldfish: at the glass, a long look, then what Van thinks of her */
    {2, 22, 23, 36, 83, 255},             /* the couch, and Meow */
    {25, 4, 24, 10, 255, 0},            /* snacks */
    {26, 34, 255, 0, 0, 0},              /* food on his planet, then hot dogs */
    {15, 33, 21, 255, 0, 0},             /* the tape measure */
    {27, 28, 29, 255, 0, 0},             /* what things are made of, and weighed by */
    {30, 31, 32, 255, 0, 0},             /* feet, meters, approximately */
    {13, 14, 20, 255, 0, 0},             /* small odd things in the room */
    {37, 255, 0, 0, 0, 0},               /* the TV (once, ever) */
    {41, 42, 43, 44, 255, 0},            /* the guy upstairs */
    {45, 46, 47, 48, 255, 0},            /* socks */
    {49, 50, 51, 52, 255, 0},            /* sleep */
    {53, 54, 55, 56, 255, 0},            /* music, from outside */
    {57, 58, 59, 60, 255, 0},            /* later, and soon, and now */
    {61, 62, 63, 64, 255, 0},            /* the fridge, the drawer */
    {65, 66, 67, 68, 255, 0},            /* names */
    {73, 74, 255, 0, 0, 0},              /* you: your days */
    {75, 76, 255, 0, 0, 0},              /* you: sounds and sleep */
    {77, 78, 255, 0, 0, 0},              /* you: being human */
    {79, 80, 255, 0, 0, 0},              /* you: the measuring, and snacks */
    {81, 255, 0, 0, 0, 0},               /* the song */
    {84, 255, 0, 0, 0, 0},               /* the staring contest */
    {86, 255, 0, 0, 0, 0},               /* Mom */
    {69, 70, 71, 72, 255, 0},            /* the pigeon */
};
static const char *const van_link_new[8] = {"Anyway.", "Okay, different thing.", "Oh. Unrelated.", "Hm. Random.", "So. Update.",
                                            "Wait. Thought of something.", "Where was I? ...Doesn't matter.", "Okay. New topic. Same couch."};
static const char *const van_link_in[6] = {"Also.", "Speaking of.", "Related.", "Oh, and.", "Same energy.", "One more."};
static const char *const van_link_back[4] = {"Anyway. Where was I?", "Right. So.", "Okay. Back to it.", "Sorry. You were saying nothing. Where was I?"};
static const char *const van_fish_ack[4] = {"...right.", "...yeah.", "...cool.", "...that's Simon~."};
static const char *const van_retry[8] = {"Go again. I'll wait.", "Again. Nobody's timing you.", "Try another. I've got snacks.", "Listen again. No rush.",
                                           "Take your time. I live here.", "Same notes. Fresh ears.", "One more time. It's a good one.", "Shake it off. Go again."};
static const char *const van_retry_ok[4] = {"There it is.", "That one doesn't count toward the level. But it counts to me.", "Got there. Scenic route.", "Doesn't count for the level. Still nice, though."};
