Beadwork 1.3.0: sixteen lanes, one for every pad of a standard drum rack.

### Changed
- **Sixteen lanes** instead of twelve, so all sixteen pads of Live's default Drum Rack (C1 to D#2) get a lane. The panel stays the same size; the rows are a little shorter.
- **General MIDI kit map:** lanes 1 to 12 are unchanged. Lanes 13 to 16 are Snare 2 (E1), Floor Tom (G1), LoMid Tom (B1) and High Tom (D2), the four rack pads the old map left out.
- **Chromatic kit maps** now span sixteen pads and are called Chromatic C1-D#2 and Chromatic C2-D#3.
- Lanes 13 to 16 have their own Pattern, Mute, Solo and Level parameters for automation. Automation on lanes 1 to 12 keeps working.
- The README and both PDF guides cover sixteen lanes.

### Upgrading
Sets, grooves and kit maps saved by 1.2.0 and earlier load into lanes 1 to 12. Lanes 13 to 16 start empty on their default notes. If you saved a kit map for a rack, add the last four pads and save it again.

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
