/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 asilagy */
/* Chord Dogtor, pose 1 (IDLE / READY): one char per pixel, from chord_dogtor/pose1_idle_ready.txt.
 * Preview only: shown on Copycat's ready screen when EO_DOG_PREVIEW is 1. */
#pragma once
#define EO_DOG_W 48
#define EO_DOG_H 60
static const char EO_DOG[EO_DOG_H][EO_DOG_W + 1] = {
    "................................................",
    "..................................BBBB..........",
    ".................................BBBBBK.........",
    ".........KKK....................BBBBBBK.........",
    ".........KBBBB..................BBBBBBB.........",
    ".........KBBBBBB...............AABBIIJAK........",
    ".........KBBAABBA..............ABBBIIJAK........",
    "..........BBBBBBAA............BBBBIIIJAK........",
    "..........BBIIBBBAA..........BBBBIIIIJAK........",
    "..........BBIIIBBAAB.........BBBIIIIIJAK........",
    "..........BBJIIIBBAB........ABBBIIIIIAK.........",
    "..........KAJIIIBABAA......AABAIIIIIBAK.........",
    "..........KABJIIIABAA.....KABBAIIIIIBKK.........",
    "..........KABJIIIAAABBKKKKKABAAIIIIBBAK.........",
    "..........KABBIAAABBBBBBBBBAAKKAJIIBIAK.........",
    "...........KBBBABBBBBBBBBBBBBBKAJIBBIAK.........",
    "...........KAABCCBBBBBCCBBBBBBBKJIBJJAK.........",
    "............AACCCBBBBBDDDBBBBBBKKABJAAK.........",
    "............KABBCBBBBBBBBBBBBBBBKAAAAA..........",
    "...........KKKKBBBBBBBBAAAABBBBBBAAAAA..........",
    "...........KKKKKBBBBBBKKKKKKBBBBBAKKA...........",
    "...........KKEJKBBBBBBKJEKJKBBBBBAAK............",
    "............KJJKBBBBBBKJJKJKBBBBBBAA............",
    "...........CCKKBBBBBBBBKKKKBBBBBAAKK............",
    "........CAACBBBBBBBBBBCCCBBBBBBKKKK.............",
    "......AACCCAABBBBBBBBBBCBBBBBBBKKK..............",
    "......ABBAAABBCCBBBBBBBBBBBCBBBKKK..............",
    "......KKBAKABDDDDBBBBBBBBBCCBBAKKK..............",
    "......BBAAKADDDDDCBBBBBBBCCCBBAKAK..............",
    "......BBAADDDDDDDCBBBBBBBBCBBAAKAAK.............",
    "......BDBBDDDDDDDCBBBBBBBBBBBAKKAAK.............",
    "......KDBBBDDDDDDDBCCCBBBBBAAKKABAK.............",
    "......KBBBBCCDDDDDCCCDBBBAAKKKAABAK.............",
    "........BDDCCCCCCBBCCDBAAKKKKBBBBAK.............",
    ".........KKDDDCCCCCCCKKKKKKKBBBBBAK.............",
    "...........KKKCKKKKBBKKKKKAABBBBBBK.............",
    "..............KKKKKDBBKKKBBBBBBBKK..............",
    ".............CCAAKKDEEHAABBBBBBAKKK.............",
    "............KCBABBDEEEHHBBBBBBBAKBFA............",
    "............KFBABBDEEEEHBBBBBBKKKBFAB...........",
    "...........BFFAABBCHEEEEBBBBBKKCCBFCBB..........",
    ".........CBBFCCAABCHEEECCBBBKKKCEBFCEBC.........",
    "........DCBFFDDAABBEEEECBBAAKKEEEBFCEBCKK.......",
    "......CHDDBFCEEHAABEEEECBBKKKEEEEFBDECEEEC......",
    ".....CCHEDBFCEEHHABEEEEEBKKCCEEEEFBDECEEECC.....",
    "....CCEEEDFBDDDDHCBEEEEEHKCCEEEEBFCDDCEEEECC....",
    "...BCEEEEDFBCCCDECCEEEEEHKCEDDDABBCDDEEEEEHCB...",
    "..KBEEEEEDFBCCCEEECEEEEEBBEEEDDAFBEEEEEEEEHEBB..",
    "..KEHEEEEFFFBBBEEECBEEEBBEEEEDDFFDEEEEEEEHHEDB..",
    ".KKEHEEEFFBBBBCCEEEBBEEBEEEEEHBFFDEEEEEEEHEEDB..",
    ".KDEEHHFFCCCBBCCCCEEBBDDEEEDDHBFCEEEEEEEHHEEDBK.",
    ".KDEEHDFFCEEDBBDCCEEBBDEEEEDCCBFCEEEEEDEHEEEHDK.",
    ".KDEEHDFFDEEDBBDEECEBEEECCEDCCCCEEEEBBDEHEEHHDK.",
    "DCEEEHDFFDEEDFFDEECBBEEECEECDDDCCEEEBFBEEEHDHDK.",
    "DCEEEHCFFEEEDCCDEEDBBCCEEEECDDDDCCCCHFBCCHHDHHKK",
    "DCEEEHCFCEEEDCCDEEDDCCEEEEECCDDBCCDHHHHHCDHDHECA",
    "DDEEHHCFCEEEDFCDEECDCHEEEEEEHDDBECDHHHHHCDHDEECA",
    "CEEEDHCCBBCBFFCEEECDHHHEEEEEHDDEECHEEEEECDHDEEEB",
    "CEEEDHEEBBCBBHHEEECDEHHEEEEEEEEEECHEEEEECDHDEEEB",
    "CEEEDHEEDDHHHHEEEECDEEEEEEEEEEEEECHEEEEECDHDEEEB",
};
static inline uint16_t eo_dog_col(char c)
{
    switch (c) {
    case 'A': return EO_RGB(11, 11, 11);
    case 'B': return EO_RGB(46, 46, 46);
    case 'C': return EO_RGB(90, 90, 90);
    case 'D': return EO_RGB(184, 184, 184);
    case 'E': return EO_RGB(255, 255, 255);
    case 'F': return EO_RGB(44, 62, 153);
    case 'G': return EO_RGB(143, 180, 255);
    case 'H': return EO_RGB(214, 214, 214);
    case 'I': return EO_RGB(244, 162, 89);
    case 'J': return EO_RGB(139, 76, 50);
    case 'K': return EO_RGB(0, 0, 0);
    default: return EO_RGB(0, 0, 0);
    }
}
