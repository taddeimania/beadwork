The first release of Beadwork, a Euclidean drum sequencer that plays a drum rack in Ableton Live (or any VST3 host that routes plugin MIDI).

### What's in it
- **Twelve lanes, one per drum pad.** Each lane spreads its beads as evenly as possible round a necklace of up to 32 steps (Bjorklund's algorithm). Turn STEPS, FILLS and ROTATE and every setting is a rhythm: tresillo, cinquillo, the bossa nova clave and the rest are all in there.
- **Hand edits on top of the figure.** Add or remove beads, accent them, ratchet them (1 to 8 hits in a step), give them a chance of playing, nudge them early or late, set their own velocity. Keep turning FILLS and ROTATE and your edits stay put; FREEZE bakes them in.
- **Polymeter and polyrhythm.** Lanes can run different step lengths and drift against each other, or squeeze their steps into a set number of bars so a 5 against a 4 comes round together.
- **Sixteen patterns per lane, with chains.** Each lane plays its own chain (pattern 1 three times, then a fill, say), and pattern changes land on the beat or the bar. Copy and paste a pattern on one lane or across all twelve at once.
- **Sixteen scenes and a scene chain.** A scene snapshots which pattern every lane is on and what's muted; the scene chain plays scenes for a set number of bars, once or on a loop, following Live's song position.
- **Scatter.** A performance effect after the one on Roland's TR-8 and TR-8S: ten types and ten depths that rearrange what you hear (stutter, reverse, half and double time, rolls, tape stop, gate and more) without touching the pattern. Tap to latch, hold for a throw.
- **Feel.** Swing with an accent on the first or second note of each pair, humanise, a global chance knob, and a clock divider. The "random" parts repeat exactly until you REROLL.
- **Kits.** Seven factory starting points, plus your own saved as files holding every lane, pattern, chain and scene. Lanes default to the General MIDI drum notes, which is where Live's drum racks put their pads.
- **Two PDF guides** in the download: getting a drum kit wired up in Live, and how patterns, chains and scenes fit together.

### Install
Download the file for your system below.

- **macOS** (universal, Apple silicon + Intel): unzip, move `Beadwork.vst3` into `~/Library/Audio/Plug-Ins/VST3`, then run:
  ```sh
  xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/Beadwork.vst3
  ```
  The build isn't signed with an Apple developer certificate, so without this step your DAW won't load it.
- **Windows**: unzip and copy the `Beadwork.vst3` folder into `C:\Program Files\Common Files\VST3`.
- **Linux**: extract, then copy `Beadwork.vst3` into `~/.vst3`.

Then rescan plug-ins in your DAW. Beadwork makes MIDI, not sound: put it on one MIDI track, put a drum rack on another, and set the drum track's **MIDI From** to Beadwork with **Monitor** on **In**. `Beadwork-Getting-Started.pdf` walks through it.
