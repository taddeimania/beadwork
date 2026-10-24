// DrumMaps.h — which pad each lane plays, kept apart from the kit.
//
// A drum map is, for each of the sixteen lanes, the MIDI note it sends, the
// label on the panel and its MIDI channel. Kits carry patterns, chains and
// scenes; maps carry the wiring to one drum rack. Loading a kit never touches
// the map, and loading a map never touches the patterns, so one map per rack
// works with every kit.
//
// User maps live in   <user app data>/Beadwork/Kit Maps/*.beadmap
//   macOS:   ~/Library/Application Support/Beadwork/Kit Maps
//   Windows: %APPDATA%\Beadwork\Kit Maps
//   Linux:   ~/.config/Beadwork/Kit Maps
//
// Message thread only.
#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include <array>
#include <optional>
#include <vector>

#include "Engine/Engine.h"
#include "State.h"

namespace beadwork
{
struct LaneMapping
{
    juce::String name;      // empty: the panel shows "Lane N"
    int note = 36;
    int channel = 0;        // 0: follow the global channel
    bool operator== (const LaneMapping&) const = default;
};

using DrumMap = std::array<LaneMapping, kNumTracks>;

DrumMap mapFromProject (const Project&);
void    applyMap (Project&, const DrumMap&);

juce::ValueTree mapToTree (const DrumMap&, const juce::String& name);
bool            mapFromTree (const juce::ValueTree&, DrumMap&);

struct FactoryMap
{
    const char* name;
    DrumMap map;
};
const std::vector<FactoryMap>& factoryMaps();
inline constexpr const char* defaultMapName = "General MIDI";

class MapManager
{
public:
    struct Entry
    {
        juce::String name;
        bool factory = true;
        int factoryIndex = -1;
        juce::File file;
    };

    explicit MapManager (ProjectStore& s) : store (s), loaded (mapFromProject (s.view())) {}

    static juce::File mapFolder();
    static constexpr const char* extension = ".beadmap";

    juce::Array<Entry> list() const;          // factory first, then yours by name

    bool load (const Entry&);
    bool loadIndex (int index);
    bool saveAs (const juce::String& name);   // overwrites one of yours with the same name
    bool remove (const Entry&);

    juce::String currentName() const { return current; }
    bool currentIsUser() const;
    int  currentIndex() const;

    // True once a lane's note, name or channel differs from the map as loaded.
    bool edited() const { return mapFromProject (store.view()) != loaded; }

    // After a session restores the lanes: remember the map's name and what it
    // held, so "edited" means edited since it was loaded or saved.
    void restore (const juce::String& name);

private:
    std::optional<DrumMap> read (const Entry&) const;

    ProjectStore& store;
    juce::String current { defaultMapName };
    bool currentUser = false;
    DrumMap loaded;
};
} // namespace beadwork
