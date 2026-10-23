// PresetManager.h — grooves: factory starting points plus your own saved as files.
//
// Your grooves live in   <user app data>/Beadwork/Grooves/*.beadwork
//   macOS:   ~/Library/Application Support/Beadwork/Grooves
//   Windows: %APPDATA%\Beadwork\Grooves
//   Linux:   ~/.config/Beadwork/Grooves
// Each file is the same XML the plugin stores in a session: every lane's
// sixteen patterns, the chains, the scenes and the global knobs. The lane
// notes, labels and channels are in there too but are ignored on load; those
// belong to the kit map (DrumMaps.h).
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Presets.h"

class BeadworkProcessor;

namespace beadwork
{
class PresetManager
{
public:
    struct Entry
    {
        juce::String name;
        bool factory = true;
        int factoryIndex = -1;
        juce::File file;
    };

    explicit PresetManager (BeadworkProcessor& p) : proc (p) {}

    static juce::File presetFolder();
    static constexpr const char* extension = ".beadwork";

    juce::Array<Entry> list() const;          // factory first, then user presets by name

    bool load (const Entry& e);
    bool loadIndex (int index);
    void step (int delta);

    bool saveAs (const juce::String& name);   // overwrites a user preset of the same name
    bool remove (const Entry& e);

    juce::String currentName() const { return current; }
    void restoreName (const juce::String& name);
    bool currentIsUser() const;
    int  currentIndex() const;

    static juce::String sanitise (const juce::String& name);

private:
    BeadworkProcessor& proc;
    juce::String current { "Init Groove" };
    bool currentUser = false;
};
} // namespace beadwork
