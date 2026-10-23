// PresetManager.h — factory kits plus user presets saved as files.
//
// User presets live in   <user app data>/Beadwork/Presets/*.beadwork
//   macOS:   ~/Library/Application Support/Beadwork/Presets
//   Windows: %APPDATA%\Beadwork\Presets
//   Linux:   ~/.config/Beadwork/Presets
// Each file is the same XML the plugin stores in a session, so a preset carries
// every lane, all sixteen patterns per lane, the chains, the scenes and the
// global knobs.
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
    juce::String current { "Init Kit" };
    bool currentUser = false;
};
} // namespace beadwork
