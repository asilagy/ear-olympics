# Ear Olympics

> "It was THESE. Obviously."
> — Meow, a cat, after you got it wrong

**A cozy ear-training arcade for the M-VAVE FM-1, hosted by a cast of animals and objects who are brilliant at music
and hopeless at everything else.** A cat with perfect pitch who is not shy about it. A dog in a white coat who feels
every wrong chord in her bones. An alien who came for the snacks. A goldfish who sees the future and forgot what it was.

**[The colourful tour, with screenshots and the browser version](https://asilagy.github.io/ear-olympics/)**

You come to learn to hear. You stay because the alien is about to show you something on the TV and he says,
"Don't say anything. Just watch."

## How it teaches

Every question is **relative to a root note** that sounds first. You learn how notes sit against each other, not what
they are called, which is the useful kind of hearing and the kind that can be trained. Each game climbs in small
steps, **one new trick per level**, and the levels rise on their own as you get things right. A wrong answer never
stings: you hear the right one, the key lights, and someone makes a face. Nobody shouts at you. The cat comes close.

The world is bright, a little weird and very sure of itself. Underneath, the root is the one thing you can trust.
Mostly. (Play long enough and you may hear something drift.)

## Meet the cast

| Game | You | Host | In their own words |
|---|---|---|---|
| **Copycat** | Hear a melody over a root, play it back. 34 levels | **Meow**, an orange tabby in headphones | "Purrfect." / "...really?" Withering, a secret perfectionist, rooting for you very quietly. |
| **Chord Dogtor** | Name the chord, from major and minor up to suspended | **Dr. Diggie**, a dog in a white coat | "Hello, I'm Dr. Diggie, your Chord Dogtor." The calm professional who feels every sad chord and runs on coffee. Speaks in silent-film title cards. |
| **Name That Interval** | Which note is higher, then which interval | **Van** (Ivan Terval), an alien surveyor | "Sup duude, the name's Ivan Terval, but my bro-bros call me Van." About 90% couch, 10% training. |
| **Simon** | Remember a sequence | **Simoné**, a goldfish clairvoyant | A placeholder game so far. She rhymes, calls you darling, and speaks of herself in the third person. Simoné forgot the rest. |

Eight more are on the way: a raccoon in a trench coat who questions everything, a hedgehog inspector, a crab with a
metal detector, a bear who apologizes to the tuna, a yodeling goat, a snail in a racing helmet, birds on a wire and a
drive-through speaker who just wants you to stay. A few of them already suspect each other.

## The games

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


