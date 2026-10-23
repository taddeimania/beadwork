Beadwork 1.1.0: a blank slate to start from, a CLEAR that really clears, and a switch for the tooltips.

### New
- **Empty kit.** The first kit in the list has no beads and no lane names. Its lanes sit on C1 to B1, the first twelve pads of any drum rack, so you can build from nothing instead of chiselling a preset down.
- **TIPS button** in the header turns the hover tooltips on and off. Beadwork remembers the choice across sessions and Live sets, and every open Beadwork follows it.
- **Scene menu.** "Save all lanes to scene" replaces "Overwrite with the current lanes". The new "Update current lane in scene" changes one lane in a stored scene and leaves the other eleven alone.
- **Copy all lanes / Paste all lanes** in the pattern pad's right-click menu copies a slot from all twelve lanes in one go.
- **Scene pads light up** the way pattern pads do: amber for the one you picked, green while it's playing.

### Changed
- **CLEAR empties the pattern.** It sets the fills to zero, the rotate to zero and wipes your hand edits. **Shift-click** CLEAR to wipe only the edits and keep the Euclidean figure, which is what CLEAR used to do.
- **Chain "+"** adds the pattern you've highlighted, not pattern 1.
- A lane with no name shows "Lane N".

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
