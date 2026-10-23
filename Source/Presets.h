// Presets.h — factory starting points. Each one is a recipe applied on top of
// the starter kit, so lane names and notes stay where a drum rack expects them.
#pragma once

#include <functional>
#include <vector>
#include "Engine/Engine.h"

namespace beadwork
{
struct FactoryPreset
{
    const char* name;
    int swing = 50;                       // global swing to go with it
    int swingGrid = 1;                    // index into ids::swingGridNames()
    std::function<void (Project&)> build; // runs after initKit()
};

namespace detail
{
// Lanes, by their place in the starter kit.
enum Lane { kick, rim, snare, clap, clHat, opHat, loTom, midTom, hiTom, pedal, crash, ride };

inline Pattern& pat (Project& p, int lane, int slot = 0) { return p.tracks[(size_t) lane].patterns[(size_t) slot]; }

inline void figure (Project& p, int lane, int steps, int fills, int rotate = 0, int division = kSixteenth)
{
    for (auto& x : p.tracks[(size_t) lane].patterns)
    {
        x.steps = (uint8_t) steps; x.fills = (uint8_t) fills;
        x.rotate = (int8_t) rotate; x.division = (uint8_t) division;
    }
}

inline void silence (Project& p)
{
    for (auto& t : p.tracks)
        for (auto& x : t.patterns)
            x.fills = 0;
}
} // namespace detail

inline const std::vector<FactoryPreset>& factoryPresets()
{
    using namespace detail;
    static const std::vector<FactoryPreset> presets {
        { "Init Kit", 50, 1, [] (Project&) {} },

        { "Four To The Floor", 54, 1, [] (Project& p)
        {
            silence (p);
            figure (p, kick, 16, 4);
            figure (p, clap, 16, 2, 4);
            figure (p, clHat, 16, 4, 2);                 // offbeat hats
            figure (p, pedal, 16, 16);                   // sixteenth shaker bed
            for (auto& x : p.tracks[pedal].patterns) { x.velocity = 50; x.accents = 0x4444; x.accent = 30; }
            figure (p, opHat, 16, 1, 14);
            figure (p, rim, 16, 3, 3);
            for (auto& x : p.tracks[rim].patterns) x.chance = 70;
        } },

        { "Tresillo", 50, 1, [] (Project& p)
        {
            silence (p);
            figure (p, kick, 8, 3);                      // x..x..x.
            figure (p, snare, 16, 2, 4);
            figure (p, rim, 8, 5, 0);                    // cinquillo
            figure (p, clHat, 16, 12);
            for (auto& x : p.tracks[clHat].patterns) { x.velocity = 70; x.accents = 0x1111; }
            figure (p, loTom, 16, 3, 2);
        } },

        { "Bossa Clave", 56, 1, [] (Project& p)
        {
            silence (p);
            figure (p, rim, 16, 5, 10);                  // E(5,16) turned: the bossa nova clave
            figure (p, kick, 16, 4);
            for (auto& x : p.tracks[kick].patterns) x.forceOn = (1u << 3) | (1u << 11);
            figure (p, clHat, 16, 8);
            for (auto& x : p.tracks[clHat].patterns) x.velocity = 64;
            figure (p, ride, 12, 7);                     // the west african bell against it
        } },

        { "Polymeter Study", 50, 1, [] (Project& p)
        {
            // Every lane loops at a different length; they line up rarely.
            silence (p);
            figure (p, kick, 16, 4);
            figure (p, rim, 7, 3);
            figure (p, clHat, 5, 2);
            figure (p, opHat, 12, 1, 6);
            figure (p, snare, 16, 2, 4);
            figure (p, hiTom, 9, 4, 1);
            for (auto& x : p.tracks[hiTom].patterns) x.chance = 80;
        } },

        { "Quintuplets", 50, 1, [] (Project& p)
        {
            // Polyrhythm: 4 against 5 against 3, all coming round every bar.
            silence (p);
            figure (p, kick, 4, 4, 0, kQuarter);
            auto squeeze = [&] (int lane, int steps, int fills)
            {
                figure (p, lane, steps, fills);
                for (auto& x : p.tracks[(size_t) lane].patterns) { x.fit = kPolyrhythm; x.fitBars = 1; }
            };
            squeeze (clHat, 5, 5);
            squeeze (rim, 3, 3);
            squeeze (pedal, 10, 7);
            for (auto& x : p.tracks[pedal].patterns) x.velocity = 55;
        } },

        { "Broken Beat", 58, 1, [] (Project& p)
        {
            silence (p);
            figure (p, kick, 16, 5, 1);
            figure (p, snare, 16, 2, 4);
            for (auto& x : p.tracks[snare].patterns) { x.nudge[12] = 8; x.forceOn = 1u << 15; x.stepVel[15] = 45; }
            figure (p, clHat, 16, 11);
            for (auto& x : p.tracks[clHat].patterns) { x.chance = 85; x.ratchet[14] = 3; x.velocity = 72; }
            figure (p, rim, 16, 3, 7);
            for (auto& x : p.tracks[rim].patterns) x.stepChance[10] = 50;
        } },

        { "Chain Demo", 52, 1, [] (Project& p)
        {
            // Lanes 1-3 chain a groove into a fill. Open the bank to see how.
            silence (p);
            figure (p, kick, 16, 4);
            figure (p, snare, 16, 2, 4);
            figure (p, clHat, 16, 8);

            // Pattern 2 on each lane is the fill.
            pat (p, kick, 1).fills = 5;
            pat (p, snare, 1).fills = 7; pat (p, snare, 1).rotate = 1;
            pat (p, snare, 1).ratchet[14] = 2; pat (p, snare, 1).ratchet[15] = 4;
            pat (p, clHat, 1).fills = 16; pat (p, clHat, 1).velocity = 60;

            for (int lane : { (int) kick, (int) snare, (int) clHat })
            {
                Track& t = p.tracks[(size_t) lane];
                t.chainOn = 1;
                t.chainLen = 2;
                t.chainPattern[0] = 0; t.chainRepeats[0] = 3;
                t.chainPattern[1] = 1; t.chainRepeats[1] = 1;
            }
        } },
    };
    return presets;
}
} // namespace beadwork
