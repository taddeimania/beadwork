Beadwork for Logic Pro: the same Euclidean drum sequencer as the VST3, built as an Audio Unit for Logic's **MIDI FX** slot.

### What it is
- **Beadwork MIDI FX** sits in the MIDI FX slot of a Software Instrument track, in front of Drum Machine Designer, Drum Kit Designer or any drum instrument. It needs no second track and no MIDI routing.
- **The same Beadwork:** twelve Euclidean lanes, hand edits, polymeter, sixteen patterns per lane with chains, scenes and a scene chain, Scatter, and the feel controls. It follows Logic's transport.
- **Shared saves:** grooves and kit maps live in the same folder as the VST3's, so a groove saved in Live loads in Logic. Pick **General MIDI** for Drum Kit Designer, or **Chromatic C1-B1** for Drum Machine Designer and save a map for your kit.

### Install
macOS 11 or later, Apple silicon or Intel.

1. Unzip and move `Beadwork MIDI FX.component` into `~/Library/Audio/Plug-Ins/Components`.
2. The build isn't signed with an Apple developer certificate, so clear the download quarantine or Logic won't load it:
   ```sh
   xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/Components/"Beadwork MIDI FX.component"
   ```
3. Open Logic. If Beadwork isn't listed, open **Logic Pro > Settings > Plug-in Manager**, select it and click **Reset & Rescan Selection**.
4. On a drum track, click the **MIDI FX** slot and choose **Audio Units > Taddei > Beadwork MIDI FX**, then press play.

`Beadwork-in-Logic.txt` in the download covers matching lanes to pads and recording what it plays. The VST3 for Ableton Live is a separate release.
