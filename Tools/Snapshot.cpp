// Snapshot.cpp — dev tool, built with -DBEADWORK_BUILD_TOOLS=ON.
//   BeadworkSnapshot panel.png [kit] [scale] [beats] [lane] [layer] [demo|scatter|scenes]
//                                                       render the panel mid-playback
//   BeadworkSnapshot --midi [kit] [bars] [scatterType depth] print the MIDI a kit sends
//   BeadworkSnapshot --state-test                           session / kit round trip
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
struct FakeHost final : juce::AudioPlayHead
{
    double ppq = 0.0, bpm = 120.0;
    bool playing = true;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo info;
        info.setIsPlaying (playing);
        info.setBpm (bpm);
        info.setPpqPosition (ppq);
        return info;
    }
};

// Run the processor for `beats` and hand every block's MIDI to `onMidi`.
template <typename F>
void play (BeadworkProcessor& p, FakeHost& host, double beats, F&& onMidi)
{
    constexpr double sr = 48000.0;
    constexpr int block = 512;
    p.setPlayHead (&host);
    p.setPlayConfigDetails (0, 2, sr, block);
    p.prepareToPlay (sr, block);
    juce::AudioBuffer<float> audio (2, block);
    juce::MidiBuffer midi;
    const double bps = host.bpm / (60.0 * sr);
    const long total = (long) (beats / bps);
    for (long s = 0; s < total; s += block)
    {
        host.ppq = s * bps;
        midi.clear();
        p.processBlock (audio, midi);
        onMidi (midi, host.ppq);
    }
}
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::String first = argc > 1 ? juce::String (argv[1]) : juce::String ("panel.png");

    if (first == "--state-test")
    {
        auto a = std::make_unique<BeadworkProcessor>();
        a->presets.loadIndex (8); // Chain Demo
        auto& pr = a->store.edit();
        pr.tracks[3].patterns[5].nudge[7] = -17;
        pr.tracks[3].patterns[5].forceOn = 0x80000001u;
        pr.tracks[3].name.fill (0);
        juce::String ("Clap & Co").copyToUTF8 (pr.tracks[3].name.data(), 15);
        a->storeScene (2);
        a->store.markDirty();

        juce::MemoryBlock mb;
        a->getStateInformation (mb);

        auto b = std::make_unique<BeadworkProcessor>();
        b->setStateInformation (mb.getData(), (int) mb.getSize());

        const bool same = std::memcmp (&a->store.view(), &b->store.view(), sizeof (beadwork::Project)) == 0;
        if (! same)
        {
            const auto* x = reinterpret_cast<const unsigned char*> (&a->store.view());
            const auto* y = reinterpret_cast<const unsigned char*> (&b->store.view());
            int shown = 0;
            for (size_t i = 0; i < sizeof (beadwork::Project) && shown < 12; ++i)
                if (x[i] != y[i])
                {
                    const size_t track = i / sizeof (beadwork::Track);
                    const size_t inTrack = i % sizeof (beadwork::Track);
                    std::printf ("  byte %zu (track %zu, +%zu; pattern %zu +%zu): %d vs %d\n", i, track, inTrack,
                                 inTrack >= offsetof (beadwork::Track, patterns) ? (inTrack - offsetof (beadwork::Track, patterns)) / sizeof (beadwork::Pattern) : 999,
                                 inTrack >= offsetof (beadwork::Track, patterns) ? (inTrack - offsetof (beadwork::Track, patterns)) % sizeof (beadwork::Pattern) : 0,
                                 x[i], y[i]);
                    ++shown;
                }
        }
        std::printf ("session round trip: %s (%d bytes of XML)\n", same ? "identical" : "DIFFERENT", (int) mb.getSize());
        std::printf ("groove name restored: %s\n", b->presets.currentName().toRawUTF8());
        return same ? 0 : 1;
    }

    if (first == "--maps-test")
    {
        // Kit maps and grooves stay out of each other's way.
        int fails = 0;
        auto check = [&] (bool ok, const char* what) { std::printf ("  %s  %s\n", ok ? "ok  " : "FAIL", what); fails += ok ? 0 : 1; };
        auto name = [] (const beadwork::Track& t) { return juce::String::fromUTF8 (t.name.data()); };

        auto a = std::make_unique<BeadworkProcessor>();
        const auto& tr = a->store.view().tracks;
        check (a->maps.currentName() == "General MIDI" && ! a->maps.edited(), "opens on General MIDI, unedited");

        check (a->maps.loadIndex (1), "load Chromatic C1-B1");
        check (tr[0].note == 36 && tr[3].note == 39 && name (tr[0]) == "C1" && name (tr[1]) == "C#1", "chromatic notes and labels");
        check (tr[0].patterns[0].fills == 4, "patterns untouched by the map");

        auto& e = a->store.edit();
        e.tracks[0].note = 35;
        e.tracks[0].name.fill (0);
        juce::String ("BD").copyToUTF8 (e.tracks[0].name.data(), 15);
        e.tracks[2].channel = 10;
        check (a->maps.edited(), "a lane edit marks the map edited");
        check (a->maps.saveAs ("Test Rack") && a->maps.currentIsUser() && ! a->maps.edited(), "save as Test Rack");

        check (a->presets.loadIndex (3), "load groove Tresillo");
        check (tr[0].patterns[0].steps == 8 && tr[0].patterns[0].fills == 3, "groove's beads arrive");
        check (tr[0].note == 35 && name (tr[0]) == "BD" && tr[2].channel == 10 && tr[5].note == 41, "map survives the groove");
        check (a->maps.currentName() == "Test Rack" && ! a->maps.edited(), "map name and state unchanged");

        check (a->presets.loadIndex (0), "load groove Empty");
        check (tr[4].patterns[0].fills == 0 && name (tr[4]) == "E1" && tr[4].note == 40, "Empty keeps the map");

        a->presets.loadIndex (5);
        check (a->presets.saveAs ("Maps Test Groove"), "save a groove");
        check (a->maps.loadIndex (0) && tr[0].note == 36 && name (tr[0]) == "Kick", "back to General MIDI");
        int user = -1;
        auto all = a->presets.list();
        for (int i = 0; i < all.size(); ++i) if (all[i].name == "Maps Test Groove") user = i;
        check (user >= 0 && a->presets.loadIndex (user), "load the saved groove");
        check (tr[0].note == 36 && name (tr[0]) == "Kick" && tr[2].channel == 0 && a->maps.currentName() == "General MIDI",
               "a saved groove doesn't bring its old map");
        check (tr[1].patterns[0].steps == 7, "...but does bring its beads");

        // Session: both come back, and the map isn't flagged edited.
        a->maps.loadIndex (a->maps.list().size() - 1);
        juce::MemoryBlock mb;
        a->getStateInformation (mb);
        auto b = std::make_unique<BeadworkProcessor>();
        b->setStateInformation (mb.getData(), (int) mb.getSize());
        check (b->maps.currentName() == a->maps.currentName() && ! b->maps.edited(), "session restores the map name");
        check (b->presets.currentName() == "Maps Test Groove", "session restores the groove name");
        check (std::memcmp (&a->store.view(), &b->store.view(), sizeof (beadwork::Project)) == 0, "session round trip identical");
        std::printf ("map folder: %s\ngroove folder: %s\n", beadwork::MapManager::mapFolder().getFullPathName().toRawUTF8(),
                     beadwork::PresetManager::presetFolder().getFullPathName().toRawUTF8());
        std::printf ("%d failures\n", fails);
        return fails == 0 ? 0 : 1;
    }

    if (first == "--midi")
    {
        const int kit  = argc > 2 ? juce::String (argv[2]).getIntValue() : 0;
        const int bars = argc > 3 ? juce::String (argv[3]).getIntValue() : 2;
        auto p = std::make_unique<BeadworkProcessor>();
        p->presets.loadIndex (kit);
        p->store.publishIfDirty();
        std::printf ("kit: %s", p->presets.currentName().toRawUTF8());
        if (argc > 5)
        {
            const int type = juce::String (argv[4]).getIntValue();     // 1..10, as on the panel
            const int depth = juce::String (argv[5]).getIntValue();
            p->setParam (beadwork::ids::scatter, 1.0f);
            p->setParam (beadwork::ids::scatterType, (float) (type - 1));
            p->setParam (beadwork::ids::scatterDepth, (float) depth);
            std::printf ("   scatter %s depth %d", beadwork::ids::scatterTypeNames()[type - 1].toRawUTF8(), depth);
        }
        std::printf ("\n");

        FakeHost host;
        std::array<juce::String, beadwork::kNumTracks> grid;
        const int stepsTotal = bars * 16;
        for (auto& g : grid) g = juce::String::repeatedString (".", stepsTotal);
        play (*p, host, bars * 4.0, [&] (const juce::MidiBuffer& m, double ppq)
        {
            for (const auto meta : m)
            {
                const auto msg = meta.getMessage();
                if (! msg.isNoteOn()) continue;
                const double beat = ppq + meta.samplePosition * (120.0 / 60.0 / 48000.0);
                if (beat >= bars * 4.0 - 1e-9)
                    continue; // the next bar's downbeat, from the last block's overhang
                const int step = juce::jlimit (0, stepsTotal - 1, (int) std::floor (beat * 4.0 + 1e-6));
                for (int t = 0; t < beadwork::kNumTracks; ++t)
                    if (p->store.view().tracks[(size_t) t].note == msg.getNoteNumber())
                        grid[(size_t) t] = grid[(size_t) t].replaceSection (step, 1, msg.getVelocity() > 105 ? "X" : "x");
            }
        });
        for (int t = 0; t < beadwork::kNumTracks; ++t)
        {
            const auto& n = p->store.view().tracks[(size_t) t].name;
            std::printf ("%-8s %s\n", n.data(), grid[(size_t) t].toRawUTF8());
        }
        return 0;
    }

    // Panel snapshot.
    const int    kit   = argc > 2 ? juce::String (argv[2]).getIntValue() : 0;
    const float  scale = argc > 3 ? (float) juce::String (argv[3]).getDoubleValue() : 1.0f;
    const double beats = argc > 4 ? juce::String (argv[4]).getDoubleValue() : 5.3;

    auto p = std::make_unique<BeadworkProcessor>();
    p->presets.loadIndex (kit);
    p->store.publishIfDirty();

    // Optional: three stored scenes and a looping scene chain, for the docs.
    if (argc > 7 && juce::String (argv[7]) == "scenes")
    {
        auto& pr = p->store.edit();
        auto name = [] (beadwork::Scene& sc, const char* n) { sc.name.fill (0); std::strncpy (sc.name.data(), n, beadwork::kNameChars - 1); };
        for (int t = 0; t < beadwork::kNumTracks; ++t)
        {
            pr.scenes[0].pattern[(size_t) t] = 0;
            pr.scenes[1].pattern[(size_t) t] = 1;
            pr.scenes[2].pattern[(size_t) t] = 0;
        }
        pr.scenes[0].used = pr.scenes[1].used = pr.scenes[2].used = 1;
        pr.scenes[2].muted = 1u;                       // the breakdown drops the kick
        name (pr.scenes[0], "VERSE"); name (pr.scenes[1], "CHRUS"); name (pr.scenes[2], "BREAK");
        pr.sceneMode = beadwork::kPlayLoop;
        pr.sceneChainLen = 4;
        const int sc[] { 0, 1, 2, 1 }, bars[] { 8, 8, 4, 8 };
        for (int i = 0; i < 4; ++i)
        {
            pr.sceneChainScene[(size_t) i] = (uint8_t) sc[i];
            pr.sceneChainBars[(size_t) i]  = (uint8_t) bars[i];
        }
        p->store.markDirty();
        p->store.publishNow();
    }
    FakeHost host;
    play (*p, host, beats, [] (const juce::MidiBuffer&, double) {});

    if (argc > 7 && juce::String (argv[7]) == "scatter")
    {
        p->setParam (beadwork::ids::scatter, 1.0f);
        p->setParam (beadwork::ids::scatterType, 6.0f);   // 7 ROLL
        p->setParam (beadwork::ids::scatterDepth, 7.0f);
        play (*p, host, 0.25, [] (const juce::MidiBuffer&, double) {});
    }
    BeadworkPanel panel (*p);
    // Optional: focus a lane and pick an edit layer, for checking the overlays.
    if (argc > 5) panel.focusLane (juce::String (argv[5]).getIntValue());
    if (argc > 6) panel.setLayer ((beadwork::ui::Layer) juce::String (argv[6]).getIntValue());
    if (argc > 7 && juce::String (argv[7]) == "demo")
    {
        // A busy 32-step pattern on the focused lane, to eyeball every overlay.
        const int lane = panel.focusedLane();
        auto& pt = p->store.edit().tracks[(size_t) lane].patterns[0];
        pt.steps = 32; pt.fills = 13; pt.rotate = 0;
        pt.accents = 0x01010101u;
        for (int s = 0; s < 32; s += 3) { pt.ratchet[(size_t) s] = (uint8_t) (1 + s % 4); pt.stepChance[(size_t) s] = (uint8_t) (20 + s * 2); pt.nudge[(size_t) s] = (int8_t) (s % 2 ? 30 : -30); pt.stepVel[(size_t) s] = (uint8_t) (40 + s * 2); }
        p->store.markDirty();
    }

    auto img = panel.createComponentSnapshot (panel.getLocalBounds(), true, scale);
    juce::File out = juce::File::getCurrentWorkingDirectory().getChildFile (first);
    out.deleteFile();
    juce::FileOutputStream os (out);
    juce::PNGImageFormat png;
    const bool ok = os.openedOk() && png.writeImageToStream (img, os);
    std::printf ("%s %s (%dx%d)\n", ok ? "wrote" : "FAILED", out.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
    return ok ? 0 : 1;
}
