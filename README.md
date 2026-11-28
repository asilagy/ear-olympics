# Ear Olympics

**An ear-training arcade for the M-VAVE FM-1, run by a cast of strange animals and objects who are very good at music
and only okay at people.**

**[The colourful tour, with screenshots and the browser version](https://asilagy.github.io/ear-olympics/)**

Short musical challenges, one skill each: repeat a melody, name a chord, measure an interval. Everything is
**relative to a root note** that sounds first, so you are training relative pitch, never perfect pitch. Each game has
levels that rise by themselves as you get things right, one new trick at a time, and a wrong answer always plays the
right one, so every mistake teaches. Nobody ever shouts at you. (The cat comes close.)

**Status: three games on one shell, no arcade mode yet.** Everything so far has been checked in the simulator and in
screenshots only; the FM-1 itself is the next test. Installing on a real device is not open yet, but you can play it
in the browser tour above.

## The games

| Game | You | Host | Their deal |
|---|---|---|---|
| **Copycat** | Hear a melody over a root and play it back, 34 levels | **Meow**, an orange tabby in headphones | Has perfect pitch and mentions it. Snarky, withering, secretly rooting for you. |
| **Chord Dogtor** | Name the chord: major, minor, diminished and on up to suspended | **Dr. Diggie**, a dog in a white coat | An anxious empath who physically feels every wrong chord. Speaks in silent-film title cards. |
| **Name That Interval** | Which note is higher, then which interval | **Van** (Ivan Terval), an alien surveyor | Here for the snacks. About 90% hanging out on his couch, 10% training. |
| **Simon** | Remember a sequence | **Simoné**, a goldfish clairvoyant | A placeholder game for now. She rhymes, calls you darling and forgets what she was saying. |

Eight more games and characters are planned (a raccoon detective, a hedgehog inspector, a crab with a metal detector,
a yodeling goat and others). The tiles for them say COMING SOON, and a few of them are already suspicious of each other.

### Copycat: the cat judges your pitch

You hear a root note alone, then a melody over it, then you play the melody back. It climbs 34 levels, one new idea at
a time: steps, thirds, leaps, the whole scale, semitones, a root that moves. When a new pitch arrives the lights come
back for one level to introduce it, then go out again. The cat dances while the melody plays, sings along on your turn,
cheers when you are right, droops when you miss, and celebrates a level up in sunglasses. The full level
table is in [the details](docs/COPYCAT.md).

### Chord Dogtor: a very serious dog, a very quiet machine

Dr. Diggie plays a chord over a faint machine hum while a heart monitor beeps along. You press the lit key that names
its quality. Six levels: major or minor; then diminished, augmented, a new root every round, sevenths and suspended
chords. A right answer gets a soft bell and a happy howl; a wrong one, a whimper and the right key lit. Eight right
answers graduate a level, and a miss costs a quarter of the way. The beep is atmosphere, not a timer. Settings from
earlier versions are reset by this update.

### Name That Interval: hanging out with Van

Van asks what you do all day, then asks which of two notes is higher, then (later) which interval it was. In between
he shows you things: a long look at Simoné the goldfish in her bowl with his reflection on the glass, the fridge hum, a
cricket, the TV static (he gets very close to the glass), and **10-ISH**, a game of Pong on his TV that you play with
knob 4 while the conversation carries on over it. Under it all, a radio plays a bossa nova that wanders through keys.
You pick a level on a title screen and after that it rises by itself. He talks in plain mixed-case text, five lines at a
time, and takes the occasional drawl very slowly.

### Simon

A placeholder for now: a sequence game that exists so the select screen has something in the corner.

## Playing it

- **PRESETS** pick the game, **PLAY** starts it, **HOME** goes back. **KNOB 1** sets the level, **KNOB 2** the sound
  of the notes (MEOW, WARM, BELL, E-PIANO, TRIANGLE, SQUARE), **KNOB 3** the speed, **KNOB 4** the mode:
  CHALLENGE, or PRACTICE, where nothing counts and nothing is lost.
- Every new level has a card saying what is new, and the levels are locked: you can play up to three above the
  highest you have finished.
- Keys, lights and the screen are the whole interface. A MIDI keyboard works too.

## More

- [Copycat, the controls and the power-on movie: every level and rule](docs/COPYCAT.md)
- [BUILDING.md](BUILDING.md) to build it yourself

## Roadmap

| Step | Goal |
|---|---|
| Skeleton | Builds, installs, runs in the simulator and on the FM-1 (done) |
| Three games (this) | Copycat, Chord Dogtor and Name That Interval, each with levels and a cast member who has opinions |
| Shared engine | A common microgame interface, then the other nine characters (Pitch Please next) |
| Arcade mode | Shuffled microgames, per-skill levels, lives, scoring, stages, boss rounds |
| Polish | Themes, sound effects, high scores, Daily Challenge codes, accessibility, release |

## Tests

`tests/run_tests.sh` runs the music unit tests (`tests/host/music_test.c`) and every script in `app/tests/` in
fm1-base's simulator. The `levelup_*.txt` scripts play whole levels, so they are **recorded** by
`python3 tests/gen_levelup.py` (after `./build.sh --host`); record them again whenever a game's note drawing or timing
changes.

## Build

See [BUILDING.md](BUILDING.md). In short: `./build.sh --host` builds a simulator that runs on your computer,
`./build.sh` builds the firmware, and `tests/run_tests.sh` runs the simulator tests.

## Licence and credits

MPL-2.0, like fm1-base. Built on [fm1-base](https://github.com/charlesvestal/fm1-base) and
felucca-bsp, from Felucca by Leo Kuroshita (Hügelton Instruments).


