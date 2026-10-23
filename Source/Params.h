// Params.h — host-automatable parameters for Beadwork.
//
// Only the performance controls are host parameters: the global feel knobs and,
// per lane, mute / solo / pattern / level. That is what you'd want to automate
// in an arrangement. The pattern banks themselves (twelve lanes x sixteen
// patterns x per-step layers) live in the Project and are saved with the
// session, but are not exposed as thousands of automation lanes.
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Engine/Engine.h"

namespace beadwork::ids
{
inline constexpr const char* clockRate  = "clockRate";
inline constexpr const char* channel    = "channel";
inline constexpr const char* swing      = "swing";
inline constexpr const char* swingGrid  = "swingGrid";
inline constexpr const char* swingVel   = "swingVel";
inline constexpr const char* humanise   = "humanise";
inline constexpr const char* chance     = "chance";
inline constexpr const char* velTrim    = "velTrim";
inline constexpr const char* quantise   = "quantise";
inline constexpr const char* masterMute = "masterMute";

inline juce::String mute (int t)    { return "mute"  + juce::String (t + 1); }
inline juce::String solo (int t)    { return "solo"  + juce::String (t + 1); }
inline juce::String pattern (int t) { return "pat"   + juce::String (t + 1); }
inline juce::String level (int t)   { return "level" + juce::String (t + 1); }

// Clock: how fast Beadwork runs relative to the host. Same set as Dicey.
// "\xc3\x97" is the multiply sign and "\xc3\xb7" the divide sign (UTF-8).
inline const juce::StringArray& clockRateNames()
{
    static const juce::StringArray n {
        juce::String::fromUTF8 ("\xc3\x97" "2"),  juce::String::fromUTF8 ("\xc3\x97" "1"),
        juce::String::fromUTF8 ("\xc3\xb7" "2"),  juce::String::fromUTF8 ("\xc3\xb7" "4"),
        juce::String::fromUTF8 ("\xc3\xb7" "8")
    };
    return n;
}
inline constexpr int clockRateDefaultIndex = 1;
inline double clockRateFromIndex (int i)
{
    static constexpr double r[] { 2.0, 1.0, 0.5, 0.25, 0.125 };
    return r[juce::jlimit (0, 4, i)];
}

// Swing grid choices and the Division each maps to.
inline const juce::StringArray& swingGridNames()
{
    static const juce::StringArray n { "1/8", "1/16", "1/32" };
    return n;
}
inline int swingGridDivision (int i)
{
    static constexpr int d[] { kEighth, kSixteenth, kThirtySecond };
    return d[juce::jlimit (0, 2, i)];
}

inline juce::StringArray quantiseNames()
{
    juce::StringArray n;
    for (auto* s : kQuantiseNames) n.add (s);
    return n;
}
} // namespace beadwork::ids

namespace beadwork
{
inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto pct = AudioParameterIntAttributes().withStringFromValueFunction (
        [] (int v, int) { return String (v) + "%"; });

    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ids::clockRate, 1 }, "Clock",
        ids::clockRateNames(), ids::clockRateDefaultIndex));
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { ids::channel, 1 }, "MIDI Channel", 1, 16, 10));

    layout.add (std::make_unique<AudioParameterInt> (ParameterID { ids::swing, 1 }, "Swing", 50, 99, 50, pct));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ids::swingGrid, 1 }, "Swing Grid",
        ids::swingGridNames(), 1));
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { ids::swingVel, 1 }, "Swing Accent", 0, 100, 50,
        AudioParameterIntAttributes().withStringFromValueFunction ([] (int v, int)
        {
            if (v == 50) return String ("Off");
            return v < 50 ? String ("First ") + String ((50 - v) * 2) + "%"
                          : String ("Second ") + String ((v - 50) * 2) + "%";
        })));
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { ids::humanise, 1 }, "Humanise", 0, 100, 0, pct));
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { ids::chance, 1 }, "Chance", 0, 100, 100, pct));
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { ids::velTrim, 1 }, "Velocity", 0, 200, 100, pct));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ids::quantise, 1 }, "Pattern Quantise",
        ids::quantiseNames(), kNextBar));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ids::masterMute, 1 }, "Mute All", false));

    for (int t = 0; t < kNumTracks; ++t)
    {
        const auto n = String (t + 1);
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { ids::mute (t), 1 }, "Lane " + n + " Mute", false));
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { ids::solo (t), 1 }, "Lane " + n + " Solo", false));
        layout.add (std::make_unique<AudioParameterInt> (ParameterID { ids::pattern (t), 1 }, "Lane " + n + " Pattern",
            1, kPatternsPerTrack, 1));
        layout.add (std::make_unique<AudioParameterInt> (ParameterID { ids::level (t), 1 }, "Lane " + n + " Level",
            0, 200, 100, pct));
    }

    return layout;
}
} // namespace beadwork
