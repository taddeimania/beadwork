# Beadwork

A Euclidean drum sequencer, built as a VST3 MIDI generator for Ableton Live.
Twelve lanes, one per drum-rack pad. Each lane threads a number of beads as
evenly as possible round a necklace of steps (Bjorklund's algorithm), and you
take it from there: turn the necklace, add and remove beads by hand, accent
them, ratchet them, give them a chance of playing, nudge them early or late.
Every lane has sixteen patterns and its own chain, and sixteen scenes snapshot
the whole kit and chain together into a song.

The Euclidean engine and the pattern / sequence workflow are inspired by the
Euclid step mode of a well-known hardware sequencer, trimmed to what a drum
rack in Live actually needs.

Sibling to [Dicey](https://github.com/taddeimania/dicer): same panel language,
same "follows Live's transport" approach to the clock.

![Beadwork panel](docs/beadwork_panel.png)

---

## Building

Needs CMake 3.22+ and a C++20 compiler. JUCE 8.0.9 is fetched on first
configure.

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

The VST3 is copied into `~/Library/Audio/Plug-Ins/VST3` after each build. Pass
`-DBEADWORK_COPY_PLUGIN=OFF` to skip that.

| Option | Default | What it does |
| --- | --- | --- |
| `BEADWORK_JUCE_DIR` | *(empty)* | Use a local JUCE checkout instead of downloading one |
| `BEADWORK_COPY_PLUGIN` | `ON` (not Windows) | Install into the system VST3 folder after building |
| `BEADWORK_BUILD_TOOLS` | `OFF` | Also build the headless snapshot tool |

For a universal macOS binary:

```sh
cmake -B build -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" -DCMAKE_BUILD_TYPE=Release
```

### Tests

```sh
cmake --build build --target BeadworkEngineTests && ./build/BeadworkEngineTests
```

The engine has no JUCE dependency, so the tests build and run anywhere. They
cover the Euclidean figures against Toussaint's published rhythms (tresillo,
cinquillo, bossa nova clave ...), the per-step layers, sample-accurate timing
at any block size, polymeter and polyrhythm, accents, ratchets, chance, nudge,
swing, mute and solo, pattern chains, quantised pattern changes, scene chains,
transport start / stop / jumps, and a worst-case stress run.

With `-DBEADWORK_BUILD_TOOLS=ON`:

```sh
./build/BeadworkSnapshot_artefacts/Release/BeadworkSnapshot panel.png 6   # render the panel playing kit 6
./build/BeadworkSnapshot_artefacts/Release/BeadworkSnapshot --midi 2 2    # print kit 2's output as a grid
./build/BeadworkSnapshot_artefacts/Release/BeadworkSnapshot --state-test  # session round trip
```

---

## Using it in Live

Beadwork makes MIDI, not sound.

1. Put Beadwork on a MIDI track.
2. Put your drum rack (a 909 kit, say) on a second MIDI track.
3. On the drum rack's track, set **MIDI From** to the Beadwork track, choose
   **Beadwork** in the second dropdown, and set **Monitor** to **In**.
4. Press play in Live.

Lanes default to the General MIDI drum notes, which is where Live's drum racks
put their pads: kick C1, rim C#1, snare D1, clap D#1, closed hat F#1, open hat
A#1, and so on. Click a lane's note to change it. Beadwork sends on channel 10
by default; each lane can have its own channel.

Incoming MIDI passes straight through, so you can still play the rack from a
keyboard on Beadwork's track.

---

## The panel

### Lanes

Twelve rows, one per lane: a gate LED that blinks on every hit, the name
(double-click to rename), the MIDI note, **M**ute, **S**olo, the pattern on
screen, and the beads. Click a row to focus it in the necklace.

Mute is a gate, not a stop: a muted lane keeps its place and comes back in
time.

The **EDIT** buttons choose what a click or drag on a bead does:

| Layer | Click | Drag up / down |
| --- | --- | --- |
| BEADS | add or remove a bead (drag across to paint) | |
| ACCENT | accent a bead: velocity + ACCENT (drag to paint) | |
| RATCHET | cycle 1, 2, 3, 4, 6, 8 hits in the step | 1 to 8 hits |
| CHANCE | toggle 50 % / 100 % | 1 to 100 % |
| NUDGE | centre it | up to half a step early or late |
| VELO | follow the pattern's velocity | the bead's own velocity |

Edits sit on top of the Euclidean figure, so you can keep turning FILLS and
ROTATE and your edits stay put. **FREEZE** bakes the current figure into
hand-placed beads. **CLEAR** wipes every edit and keeps the figure. **DICE**
rolls a new figure. **COPY** / **PASTE** move a whole pattern.

### Necklace

The focused lane's pattern as a ring of LEDs, step 1 at the top. Drag the
centre round to **rotate** the necklace, scroll the centre for **fills**,
shift-scroll for **steps**. Step numbers turn amber where you've edited the
figure.

| Control | Range | What it does |
| --- | --- | --- |
| STEPS | 1 - 32 | slots on the necklace |
| FILLS | 0 - steps | beads, spread as evenly as possible |
| ROTATE | -31 - 31 | turn the necklace; positive moves beads later |
| LEVEL | 0 - 200 % | the lane's level (automatable) |
| VELOCITY | 1 - 127 | an ordinary bead |
| ACCENT | 0 - 127 | added on accented beads |
| GATE | 1 - 200 % | note length, as a share of the step |
| CHANCE | 0 - 100 % | the chance each bead plays; per-step chance multiplies with it |
| STEP | 1/4 - 1/32T | step length |
| FIT | METER / RHYTHM | see below |
| BARS | 1 - 16 | in RHYTHM fit, the bars the steps are squeezed into |
| DIRECTION | FWD, REV, PEND, RAND | which way the playhead walks |
| ON CHANGE | FREE, RELATCH, ONCE | what the lane does when its pattern changes |
| LANE CH | GLOBAL, 1 - 16 | MIDI channel |

**FIT** decides what happens when lanes have different numbers of steps:

- **METER** (polymeter): a step is a fixed note value. A 7-step lane and a
  16-step lane drift against each other and line up again every 112 steps.
- **RHYTHM** (polyrhythm): the steps are squeezed into BARS bars, so every lane
  comes round together. Five steps in a bar against a four-on-the-floor kick is
  a 5:4 polyrhythm.

**ON CHANGE**: FREE keeps the playhead's place in the loop, RELATCH restarts
the loop from step 1, ONCE plays the new pattern once and then waits silently
for the next change.

### Feel

Global, automatable.

| Control | What it does |
| --- | --- |
| SWING | 50 - 99 %: delays every second slot of the swing grid. 50 % is straight |
| SWING ACC | accent the first (left of centre) or second (right) of each swung pair |
| HUMANISE | random timing spread |
| CHANCE | scales the chance of every bead on every lane; ride it for breakdowns |
| VELOCITY | scales every velocity |
| SWING GRID | 1/8, 1/16 or 1/32 |
| PATTERN CHANGE | when a newly picked pattern takes over: at once, next step, beat, 1 / 2 / 4 / 8 bars, or end of the loop |
| CLOCK | x2, x1, /2, /4, /8 against Live |
| MIDI CH | the default channel |

The random parts (chance, humanise, RAND direction) are deterministic: the same
dice every time round, so a groove you like stays the groove you like.
**REROLL** throws new dice.

### Patterns

The focused lane's sixteen patterns. Click a pattern to play and edit it; the
switch waits for PATTERN CHANGE. Shift-click adds a pattern to the lane's chain.
Right-click to copy, paste, clear, copy to every slot, or add to the chain.

**CHAIN** turns the lane's chain on. Each slot plays its pattern N times, then
moves on, and the chain loops. Drag a slot's top half to change its pattern,
its bottom half to change the repeats; click a slot for insert / delete; **+**
and **-** lengthen and shorten the chain. While a chain plays, the lane shows
the pattern that's playing.

Each lane's pattern is also a host parameter (**Lane N Pattern**), so you can
automate pattern changes in Live's arrangement.

### Scenes

A scene stores every lane's pattern and mute. Click an empty pad to store the
current kit into it, click a stored one to recall it, shift-click to
overwrite. Right-click to rename, clear, or add to the scene chain.

| Mode | What it does |
| --- | --- |
| PERFORM | scenes change only when you click them |
| PLAY | play the scene chain once from the song's start, then hold the last scene |
| LOOP | loop the scene chain for as long as Live plays |

Each scene-chain slot has a scene and a length in bars. While the scene chain
runs it chooses each lane's pattern and mute; a lane's own chain still wins
over it.

### Transport

The LCD shows the focused lane, its playing pattern and step, and the scene
chain's position. **RESTART** brings every lane and chain back to the top on
the next beat. **REROLL** throws new dice. **MUTE ALL** silences everything
while it keeps running underneath.

### Kits

The KIT display holds factory starting points (Four To The Floor, Tresillo,
Bossa Clave, Polymeter Study, Quintuplets, Broken Beat, Chain Demo) and your
own saves. A kit holds all twelve lanes, every pattern, the chains, the scenes
and the global knobs. Your kits live in
`~/Library/Application Support/Beadwork/Presets` as `.beadwork` files.

---

## License

AGPLv3. See [LICENSE](LICENSE).
