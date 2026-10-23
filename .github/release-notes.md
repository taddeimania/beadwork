Beadwork 1.2.0: your drum rack's wiring and your grooves are now saved separately.

### New
- **Kit maps.** A new **KIT MAP** selector in the header holds each lane's label, MIDI note and channel. Save one per drum rack and pick it whenever you use that rack. The factory maps are General MIDI (the lane names and notes Beadwork has always opened with), Chromatic C1-B1 and Chromatic C2-B2. A `*` after the map's name means you've changed a note, label or channel since loading it, and **Save** in the map's menu updates one of your maps in place.
- **Grooves.** What used to be called a kit is now a **GROOVE**: the beads and hand edits, all sixteen patterns per lane, the chains, the scenes and the feel knobs. Loading a groove never touches the kit map, and loading a kit map never touches the groove, so any groove plays on any rack.

### Changed
- **Init Kit** is now **Init Groove**, and **Empty** only clears the beads. It leaves your lane notes and labels alone.
- Your saves live in `~/Library/Application Support/Beadwork/` (`%APPDATA%\Beadwork` on Windows, `~/.config/Beadwork` on Linux), with kit maps in `Kit Maps/` and grooves in `Grooves/`.
- **Upgrading:** kits saved by 1.1.0 and earlier are copied from `Presets/` into `Grooves/` the first time you run 1.2.0. They load as grooves, so their old notes and labels are left out; pick or save a kit map for them.
- The two PDF guides and the README cover kit maps and grooves.

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
