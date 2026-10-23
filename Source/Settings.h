// Settings.h — per-user preferences that outlive a session, shared by every
// Beadwork instance on the machine (as opposed to the kit, which lives in the
// Live set).
//
// Stored next to the user presets:
//   macOS:   ~/Library/Application Support/Beadwork/Beadwork.settings
//   Windows: %APPDATA%\Beadwork\Beadwork.settings
//   Linux:   ~/.config/Beadwork/Beadwork.settings
//
// Message thread only.
#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include <juce_events/juce_events.h>

namespace beadwork
{
// The file itself. One is shared by every open Beadwork in the process (via
// SharedResourcePointer), so flipping a setting in one window reaches the
// others; it's freed when the last one closes. Another process (a second host)
// picks a change up the next time it loads Beadwork.
struct SettingsStore
{
    SettingsStore();
    juce::PropertiesFile props;
};

class Settings
{
public:
    bool tooltipsEnabled() const;           // default: on
    void setTooltipsEnabled (bool);         // saved straight away

    static juce::File file();               // where the settings live

private:
    juce::SharedResourcePointer<SettingsStore> store;
};
} // namespace beadwork
