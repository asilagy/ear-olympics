# Ear Olympics

A WarioWare-style ear-training arcade game for the M-VAVE FM-1.

The aim: short musical challenges come one after another, each a different skill (repeat a melody by ear, name the
interval, identify the chord, the scale, the progression). Every skill has its own difficulty level that rises as you
get things right, and a wrong answer plays the right one so every mistake teaches. Everything is relative to a root
note that sounds first, so it trains relative pitch, never perfect pitch.

**Status: two games on one shell, with automatic levels (0.7.2).** No arcade mode yet.

## The two games

They are kept apart on purpose.

**Simon** a place-holder for now, building more games later

**Copycat** is ear training. You hear a root note alone (the screen says ROOT and its key lights), then a melody
over it, and you play the melody back. The root is a foundation, not a note of the melody: it is a soft, round drone
that keeps sounding under the melody (and under your turn, at the lower levels). The levels:

| Level | New idea | Notes | Rounds |
|---|---|---|---|
| 1 | steps, do re mi; the keys light as they sound | 3 | 8 |
| 2 | by ear: no lights | 3 | 8 |
| 3 | **fa and sol arrive: the lights are back, and one of them is in every melody** | 3 | 8 |
| 4 | the same five notes, lights off | 3 | 8 |
| 5 | longer | 4 | 8 |
| 6 | thirds join the steps | 4 | 8 |
| 7 | **la arrives: lit, and in every melody** | 3 | 8 |
| 8 | la by ear | 3 | 8 |
| 9 | longer | 4 | 8 |
| 10 | **ti and the octave arrive: lit, and in every melody** | 3 | 8 |
| 11 | the whole major scale by ear | 3 | 8 |
| 12 | longer | 4 | 8 |
| 13 | semitones, C to G (lit) | 3 | 10 |
| 14 | semitones by ear | 3 | 10 |
| 15 | leaps: every melody has one, to a chord tone | 3 | 10 |
| 16 | longer | 4 | 10 |
| 17 | a leap to any note, no contour on screen | 4 | 10 |
| 18 | up to two leaps | 5 | 10 |
| 19 | semitones and a leap | 4 | 10 |
| 20 | chromatic passing notes across the octave (lit) | 4 | 10 |
| 21 | the same by ear, longer | 5 | 10 |
| 22 | no drone, the melody starts on the root | 4 | 10 |
| 23 | longer | 5 | 10 |
| 24 | it starts anywhere | 5 | 12 |
| 25 | **below the root: lit, and a low note in every melody** | 4 | 12 |
| 26 | below the root, by ear | 5 | 12 |
| 27 | longer | 6 | 12 |
| 28 | chromatic leaps (lit) | 5 | 12 |
| 29 | the root moves: C G D | 4 | 12 |
| 30 | longer | 5 | 12 |
| 31 | C G D A E | 5 | 12 |
| 32 | all seven roots, up to F# and C#, start anywhere | 5 | 12 |
| 33 | chromatic notes in every key (lit) | 5 | 12 |
| 34 | boss: any note, any root, up to two leaps | 6 | 12 |

The ladder takes one new thing per level and braids several tracks: when new pitches arrive (the whole scale, the
semitones, a new key) the melody is short again and grows on the next level, and a short chromatic run takes its turn
between longer diatonic melodies with leaps. **When new pitches arrive, the lights come back** for one level and the melody always uses at least one of the new
pitches; the next level has the same notes with the lights off. Every new level has a card, with a cat pun, saying
what is new.

**Crutches that fade out:** the melody plays once; PLAY replays it whenever you want (and the SELECT knob steps through
it a note at a time). At the first levels its **contour** (the shape: each note a dash, high or low) appears on
screen as you hear it and stays for your turn; it is gone from level 17. The keys' lights go at level 2.
**A note can repeat** (it is played again, never held), at any level; a leap is still always answered by a step.

**Rounds to graduate** are 8 at levels 1-12, 10 at 13-23 and 12 at 24-34, so the foundation is solid. **A miss takes a
quarter of the rounds off the count** (2 at 8 rounds, 3 at 10 or 12).

The order follows how aural-skills courses teach melodic dictation (for example Baylor's open Ear Training
Companion): a few scale degrees at a time, steps then thirds then leaps (leaps to chord tones first, as they are easier
to hear), the drone and then the start-on-the-root anchor removed, the key moved, and chromatic notes last.

**Difficulty follows the melody's motion, not just its range.** Steps and thirds are easy to hold; leaps (a fourth or
more) are hard, and more of them is harder. So the generator prefers steps over thirds over leaps, caps the leaps per
melody and the melody's total span, and where leaps are allowed, **a leap is always followed by a step back toward
where it came from**, filling the gap, as in real melodies.

From level 29 the root changes every round along the cycle of fifths (never the same twice running). Only its key is
lit; the screen does not name it, since this is relative pitch.

A wrong note plays the right melody, lit, over its root, so you hear what it was.

## The cat, the voices, the lock

- **The cat** (Copycat only) is an original orange tabby in headphones. It dances while the melody plays, sings along
  while you play, cheers when you are right, droops when you miss, and celebrates for three seconds when you level up, in sunglasses, with confetti and a song (happy meows climbing, a long
  one, then a purr), before the GOOD JOB card tells you what is new.
- **Sounds:** the success sound is always the cat (two happy meows, above the keyboard). A miss is the cat's sad yowl,
  then the right melody so you hear what it was. The MEOW voice for the melody's own notes hardly bends the
  pitch (you have to copy it); only the cat's success meow swoops.
- **Voices:** KNOB 2 picks the sound of the notes, with a short note to hear it: MEOW (default), WARM, BELL, E-PIANO,
  TRIANGLE, SQUARE. The voice is kept with your settings and shown at the top right of the screen.
- **KNOB 3, speed:** how fast the melody plays back, 25% to 200% in steps of 5% (100% is normal). Applies to both games.
- **KNOB 4, mode:** CHALLENGE (the default) is the game as it is: a miss ends the round. PRACTICE lets you keep trying
  the same melody: a wrong note plays the cat's yowl and the right melody, then you start again from the first note
  (Simon: the sequence plays again). Practice counts for nothing: no streak, no progress toward a level, no best,
  nothing saved but the settings.
- **Mid-round:** KNOB 2 (voice), KNOB 3 (speed) and KNOB 4 (mode) can be turned at any time, in the middle of a round.
  During YOUR TURN, **SELECT steps through the melody one note per notch** (each note sounds for about a second; a note still sounding is cut off first, so they never overlap) (clockwise next, counter-clockwise
  previous, wrapping round); the screen says which note ("2 OF 5"). PLAY still replays the whole melody.
- **Levels are locked:** you can play up to three levels above the highest you have completed (enough rounds right).
  KNOB 1 past that says what to finish first.

## Levels graduate by themselves

Play enough rounds right at a level and you move up: in Copycat, 8, 10 or 12 melodies played right (they don't have to
be in a row, but **a miss takes a quarter of the rounds off the count**, down to 0, so guessing cannot get you through); in Simon, a sequence of ten. A **GOOD JOB!** card says the new level and what is different about it, and
waits for PLAY. The count is shown on screen ("LEVEL UP: 3/8 RIGHT"), and kept with your settings. KNOB 1 still
sets the level by hand. The top level has nothing to graduate to.

## The success sound

When you play the last note right, the screen says NICE! at once, but the sound waits for you to let go of the key, so
your last note is not cut off. It is a quick high ding, above the top key of the FM-1, so it can never be mistaken
for a note of the melody.

## Every start: the cat explains

On every start the cat narrates: an **intro** (it is rude about your pitch), then three **controls** screens that say what each
control does, in its voice (PLAY, SELECT, HOME, PRESETS, KNOBS 1 to 4, SAVE, the volume), then the game. **Any key**
goes to the next screen; **HOME** skips straight to the game. The knobs do nothing on those two screens.

## The round screen (Copycat)

During a round the screen keeps only: the mode and level line, the cat (big), what it says, YOUR TURN with the note
dashes (each dash a note; on the early levels the dashes sit high or low to show the melody's shape), and STREAK and
BEST. The root and LEVEL UP progress appear only in the pause between rounds; the volume shows for about a second after
you change it. There is no on-screen keyboard: with the lights off, **after a miss it slides in with the right notes
lit**, and the cat says so ("It was THESE. Obviously."); it hides again when the next round starts. The cat also comments
now and then ("Purrfect." on a run, "...really?" on a miss). Simon's screen is unchanged.

## The ready screen, and the cat's temper

Before a level starts the screen says **PRESS PLAY TO START. Meow.** The top line says `COPYCAT - LEVEL n`, and at the top
right the root and the voice together (`C4 - MEOW`; `MOVING - MEOW` from level 29, when the root moves). **SEL** shows the controls again.
Underneath is the level: its name (for example "Lights off"), the notes you choose from, how long the tunes are, what is
new in them (a new note, a leap, chromatic notes), and what is switched on (lights, drone, shape, start on the root). The cat's
words about what is new are on the GOOD JOB card, which appears when you reach the level: **PRESS PLAY TO CONTINUE**.
After a miss the big word changes each time (MISS, UMM..., WHOOPS., HISSS!, WRONG!, NYAAAHH!, INCORRECT) and so does the
cat's line. After **3 misses in a row** the cat gives advice instead (PLAY replays, the SELECT knob, slow it down, Practice
mode, think first, sing the notes). A right melody gets NICE!, WOW!, SWEET!, AMAZING! or YUP. In Practice mode, after
**3 wrong tries** at a no-lights melody, the cat says "Sssss... let me hit the lights for you." and replays it lit, with
the keyboard showing.

## Controls

- **PRESETS:** the game. **KNOB 1:** its level. **SELECT:** Copycat's root, C4 to G4. All only while ready.
- **PLAY:** start. In Copycat, PLAY during your turn plays it all again, root first. **HOME** stops.
- **Keys:** play the notes back. They stay at C4 and above, because the FM-1's speaker is small and a low note is weak.
- **SAVE:** keep the game, root, levels and best scores. HOME keeps them too, and so does a streak ending, a game
  ending, or moving up a level. Nothing is written to the FM-1's flash between rounds.
- **MIDI keyboard:** MIDI notes 53 to 79 (F3 to G5) play the 27 keys, over USB on the FM-1 and through "MIDI ports" in the browser emulator.
- **LEDs:** in Copycat the root is mid-bright (bright while it is introduced), the level's other notes glow dimly;
  in Simon the keys in play glow dimly. The sounding or held key is bright.

## Roadmap

| Step | Goal |
|---|---|
| Skeleton | Builds, installs, runs in the simulator and on the FM-1 (done) |
| Two games (this) | Simon and Copycat, each with levels and feedback |
| Shared engine | A common microgame interface, then Name That Interval and Pitch Please |
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

MPL-2.0, like fm1-base and Boardwalk. Built on [fm1-base](https://github.com/charlesvestal/fm1-base) and
felucca-bsp, from Felucca by Leo Kuroshita (Hügelton Instruments). Its structure follows Charles Vestal's
[Boardwalk](https://charlesvestal.github.io/fm1-boardwalk/).

## Chord Dogtor (0.8.0, new)

A third game: name the chord. Dr. Diggie, a very serious dog in a white coat, plays a chord over a slow pedal bass (the
chord's root, an octave down) with a heart-monitor beep ticking. You press the lit key for its quality: A (F4), B (G4),
C (A4), D (B4). Six levels: major or minor; plus diminished; plus augmented; the same with a new root every round; sevenths;
suspended chords. Eight right graduates a level; a miss costs a quarter of the way. The beep is atmosphere, not a timer. The
dog has six poses (idle, thinking, correct, worried, shocked, yawn). Settings from earlier versions are reset by this
update (the saved layout grew a game).
