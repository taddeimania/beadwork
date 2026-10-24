// EngineTests.cpp — engine checks that need no JUCE and no audio device.
// Build: part of the BeadworkEngineTests target, run with ctest.

#include "Engine/Engine.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

using namespace beadwork;

// A Sequencer is about 170 KB, so every test keeps its sequencers on the heap:
// several on the stack at once overflow Windows' 1 MB default stack.
namespace
{
int failures = 0;
int checks   = 0;

void check (bool ok, const std::string& what)
{
    ++checks;
    if (! ok)
    {
        ++failures;
        std::printf ("  FAIL  %s\n", what.c_str());
    }
}

void checkNear (double a, double b, double tol, const std::string& what)
{
    ++checks;
    if (! (std::abs (a - b) <= tol))
    {
        ++failures;
        std::printf ("  FAIL  %s  (got %.6g, expected %.6g +/- %.3g)\n", what.c_str(), a, b, tol);
    }
}

void section (const char* name) { std::printf ("%s\n", name); }

std::string bits (uint32_t m, int n)
{
    std::string s;
    for (int i = 0; i < n; ++i)
        s += ((m >> i) & 1u) ? 'x' : '.';
    return s;
}

struct Msg
{
    long long sample;
    double    beat;
    int       track;
    int       note;
    int       velocity;
    bool      on;
};

constexpr double kSr = 48000.0;

// Run the sequencer over `beats` in blocks and collect every event.
std::vector<Msg> run (Sequencer& seq, const Project& p, const Globals& g, double beats,
                      double bpm = 120.0, int block = 512, double startPpq = 0.0)
{
    std::vector<Msg> out;
    const double bps = bpm / (60.0 * kSr);
    const long long total = (long long) std::llround (beats / bps);
    for (long long s = 0; s < total; s += block)
    {
        BlockInfo b;
        b.playing    = true;
        b.ppq        = startPpq + s * bps;
        b.bpm        = bpm;
        b.sampleRate = kSr;
        b.numSamples = (int) std::min<long long> (block, total - s);
        seq.process (b, g, p, [&] (int off, const NoteEvent& e)
        {
            out.push_back ({ s + off, e.beat, e.track, e.note, e.velocity, e.on });
        });
    }
    return out;
}

std::vector<Msg> onsFor (const std::vector<Msg>& m, int track)
{
    std::vector<Msg> r;
    for (auto& x : m)
        if (x.on && x.track == track)
            r.push_back (x);
    return r;
}

// A project with every lane silent, ready for one lane to be set up.
Project emptyProject()
{
    Project p;
    initKit (p);
    for (auto& t : p.tracks)
        for (auto& pat : t.patterns)
            pat.fills = 0;
    return p;
}

// Every note-on is matched by a note-off on the same lane before the lane's
// next note-on, and time never runs backwards within a lane. Lanes are emitted
// one after another inside a block (the plugin's MidiBuffer sorts them), so the
// check runs on a stable sort by sample, which keeps each lane's own order.
bool wellFormed (std::vector<Msg> m)
{
    std::stable_sort (m.begin(), m.end(), [] (const Msg& a, const Msg& b) { return a.sample < b.sample; });

    std::array<int, kNumTracks> active {};
    std::array<long long, kNumTracks> last {};
    active.fill (-1);
    last.fill (-1);
    for (auto& x : m)
    {
        if (x.sample < last[(size_t) x.track]) return false;
        last[(size_t) x.track] = x.sample;
        int& a = active[(size_t) x.track];
        if (x.on)
        {
            if (a >= 0) return false;
            a = x.note;
        }
        else
        {
            if (a != x.note) return false;
            a = -1;
        }
    }
    return true;
}
} // namespace

// ---------------------------------------------------------------------------
static void testEuclid()
{
    section ("Euclid");

    // Canonical figures from Toussaint, "The Euclidean Algorithm Generates
    // Traditional Musical Rhythms".
    check (bits (euclid (8, 3, 0), 8)   == "x..x..x.",         "E(3,8) tresillo");
    check (bits (euclid (8, 5, 0), 8)   == "x.xx.xx.",         "E(5,8) cinquillo");
    check (bits (euclid (4, 1, 0), 4)   == "x...",             "E(1,4)");
    check (bits (euclid (12, 7, 0), 12) == "x.xx.x.xx.x.",     "E(7,12) west african bell");
    check (bits (euclid (16, 4, 0), 16) == "x...x...x...x...", "E(4,16) four on the floor");
    check (bits (euclid (13, 5, 0), 13) == "x..x.x..x.x..",    "E(5,13)");

    check (euclid (16, 0, 0) == 0u,          "no fills is silent");
    check (euclid (16, 16, 0) == 0xFFFFu,    "all fills is every step");
    check (euclid (32, 32, 0) == 0xFFFFFFFFu, "32 of 32");
    check (euclid (16, 99, 0) == 0xFFFFu,    "fills clamp to steps");

    // Popcount always equals fills.
    bool counts = true;
    for (int n = 1; n <= kMaxSteps; ++n)
        for (int k = 0; k <= n; ++k)
        {
            uint32_t m = euclid (n, k, 0);
            int c = 0;
            for (int i = 0; i < 32; ++i) c += (m >> i) & 1u;
            counts = counts && c == k;
            if (k > 0) counts = counts && (m & 1u);
            if (n < 32) counts = counts && (m >> n) == 0u;
        }
    check (counts, "every (steps, fills) has the right number of beads, starting on step 1");

    // Rotation.
    check (bits (euclid (8, 3, 1), 8)  == ".x..x..x", "rotate +1 moves beads later");
    check (bits (euclid (8, 3, -1), 8) == "..x..x.x", "rotate -1 moves beads earlier");
    check (euclid (8, 3, 8) == euclid (8, 3, 0),      "rotate by the length is a no-op");
    check (euclid (16, 2, 4) == ((1u << 4) | (1u << 12)), "backbeat: E(2,16) rotated 4");
    check (bits (euclid (16, 5, 10), 16) == "x..x..x...x..x..", "bossa nova clave: E(5,16) rotated 10");
}

// ---------------------------------------------------------------------------
static void testPatternMask()
{
    section ("Pattern layers");

    Pattern p;
    p.steps = 8; p.fills = 3; p.rotate = 0;
    check (p.mask() == euclid (8, 3, 0), "plain figure");

    p.forceOn = 1u << 1;
    check (((p.mask() >> 1) & 1u) == 1u, "force-on adds a bead");

    p.forceOff = 1u << 0;
    check ((p.mask() & 1u) == 0u, "force-off removes a bead");

    p.forceOn |= 1u << 20;
    check ((p.mask() >> 8) == 0u, "layers beyond the step count are ignored");

    checkNear (p.loopBeats(), 2.0, 1e-12, "8 x 1/16 = 2 beats");
    p.fit = kPolyrhythm; p.fitBars = 1;
    checkNear (p.loopBeats(), 4.0, 1e-12, "polyrhythm squeezes 8 steps into a bar");
    p.steps = 5;
    checkNear (p.stepBeats(), 0.8, 1e-12, "5 steps in a bar = 0.8 beat each");
}

// ---------------------------------------------------------------------------
static void testTiming()
{
    section ("Timing");

    Project p = emptyProject();
    Globals g;
    Pattern& k = p.tracks[0].patterns[0];
    k.steps = 16; k.fills = 4;

    auto seqOwned = std::make_unique<Sequencer>(); auto& seq = *seqOwned;
    auto m = run (seq, p, g, 16.0);
    auto ons = onsFor (m, 0);

    check (ons.size() == 16, "four on the floor over four bars: 16 kicks (got " + std::to_string (ons.size()) + ")");
    bool onBeat = true;
    for (size_t i = 0; i < ons.size(); ++i)
    {
        const double expected = i * 1.0 * (60.0 / 120.0) * kSr;
        onBeat = onBeat && std::abs ((double) ons[i].sample - expected) <= 1.0;
    }
    check (onBeat, "every kick lands on its beat, sample-accurately");
    check (ons.empty() || ons[0].note == 36, "kick lane plays note 36");
    check (wellFormed (m), "note-ons and offs are balanced");

    // Block size must not change anything.
    auto aOwned = std::make_unique<Sequencer>(); auto& a = *aOwned;
    auto bOwned = std::make_unique<Sequencer>(); auto& b = *bOwned;
    auto ma = run (a, p, g, 8.0, 120.0, 64);
    auto mb = run (b, p, g, 8.0, 120.0, 1999);
    bool same = ma.size() == mb.size();
    for (size_t i = 0; same && i < ma.size(); ++i)
        same = ma[i].sample == mb[i].sample && ma[i].on == mb[i].on;
    check (same, "identical output at block sizes 64 and 1999");

    // Clock rate halves everything.
    auto hOwned = std::make_unique<Sequencer>(); auto& h = *hOwned;
    Globals g2 = g; g2.rate = 0.5;
    auto mh = onsFor (run (h, p, g2, 8.0), 0);
    check (mh.size() == 4, "clock /2: half the kicks (got " + std::to_string (mh.size()) + ")");
}

// ---------------------------------------------------------------------------
static void testPolymeter()
{
    section ("Polymeter and polyrhythm");

    Project p = emptyProject();
    Globals g;

    // Lane 0: 16 x 1/16 with one bead (every bar). Lane 1: 12 x 1/16 (dotted
    // half), one bead. They line up again after 3 bars.
    p.tracks[0].patterns[0].steps = 16; p.tracks[0].patterns[0].fills = 1;
    p.tracks[1].patterns[0].steps = 12; p.tracks[1].patterns[0].fills = 1;

    auto seqOwned = std::make_unique<Sequencer>(); auto& seq = *seqOwned;
    auto m = run (seq, p, g, 12.0);
    check (onsFor (m, 0).size() == 3, "16-step lane: 3 hits in 3 bars");
    check (onsFor (m, 1).size() == 4, "12-step lane: 4 hits in 3 bars");

    // Polyrhythm: 5 steps squeezed into a bar, every step on.
    Project q = emptyProject();
    Pattern& r = q.tracks[2].patterns[0];
    r.steps = 5; r.fills = 5; r.fit = kPolyrhythm; r.fitBars = 1;
    auto s2Owned = std::make_unique<Sequencer>(); auto& s2 = *s2Owned;
    auto ons = onsFor (run (s2, q, g, 4.0), 2);
    check (ons.size() == 5, "quintuplet bar: 5 hits");
    if (ons.size() == 5)
        checkNear (ons[1].beat - ons[0].beat, 0.8, 1e-9, "quintuplets 0.8 beat apart");
}

// ---------------------------------------------------------------------------
static void testLayers()
{
    section ("Accent, ratchet, chance, nudge");

    Project p = emptyProject();
    Globals g;
    Pattern& pat = p.tracks[0].patterns[0];
    pat.steps = 4; pat.fills = 4; pat.division = kQuarter;
    pat.velocity = 80; pat.accent = 30;
    pat.accents = 1u << 2;
    pat.ratchet[3] = 3;

    auto seqOwned = std::make_unique<Sequencer>(); auto& seq = *seqOwned;
    auto m   = run (seq, p, g, 4.0);
    auto ons = onsFor (m, 0);
    check (ons.size() == 6, "4 steps with a 3x ratchet on the last: 6 hits (got " + std::to_string (ons.size()) + ")");
    if (ons.size() == 6)
    {
        check (ons[0].velocity == 80,  "plain step at base velocity");
        check (ons[2].velocity == 110, "accented step adds the accent");
        checkNear (ons[4].beat - ons[3].beat, 1.0 / 3.0, 1e-9, "ratchet hits a third of a step apart");
    }
    check (wellFormed (m), "ratchets retrigger cleanly");

    // Chance 0 on a step silences it; pattern chance 0 silences the lane.
    Project c = emptyProject();
    Pattern& cp = c.tracks[0].patterns[0];
    cp.steps = 4; cp.fills = 4; cp.division = kQuarter;
    cp.stepChance[1] = 1; // 1 %
    auto csOwned = std::make_unique<Sequencer>(); auto& cs = *csOwned;
    auto cons = onsFor (run (cs, c, g, 400.0), 0);
    // 100 passes: steps 0,2,3 always fire (300), step 1 about once.
    check (cons.size() >= 300 && cons.size() <= 306, "1 % step fires rarely (got " + std::to_string (cons.size()) + ")");

    cp.stepChance[1] = 0;
    cp.chance = 50;
    auto cs2Owned = std::make_unique<Sequencer>(); auto& cs2 = *cs2Owned;
    auto half = onsFor (run (cs2, c, g, 400.0), 0);
    check (half.size() > 160 && half.size() < 240, "50 % pattern chance is about half (got " + std::to_string (half.size()) + ")");

    // Same seed, same dice.
    auto cs3Owned = std::make_unique<Sequencer>(); auto& cs3 = *cs3Owned;
    auto again = onsFor (run (cs3, c, g, 400.0), 0);
    bool same = again.size() == half.size();
    for (size_t i = 0; same && i < half.size(); ++i) same = again[i].sample == half[i].sample;
    check (same, "chance is deterministic for a given seed");

    // Nudge.
    Project n = emptyProject();
    Pattern& np = n.tracks[0].patterns[0];
    np.steps = 4; np.fills = 4; np.division = kQuarter;
    np.nudge[1] = 25;
    auto nsOwned = std::make_unique<Sequencer>(); auto& ns = *nsOwned;
    auto nons = onsFor (run (ns, n, g, 4.0), 0);
    if (nons.size() == 4)
        checkNear (nons[1].beat, 1.25, 1e-9, "nudge +25 % pushes a quarter of a step late");
    else
        check (false, "nudge: expected 4 hits");
}

// ---------------------------------------------------------------------------
static void testSwing()
{
    section ("Swing");

    Project p = emptyProject();
    Globals g;
    g.swing = 75; g.swingGrid = kSixteenth;
    Pattern& h = p.tracks[4].patterns[0];
    h.steps = 16; h.fills = 16;

    auto seqOwned = std::make_unique<Sequencer>(); auto& seq = *seqOwned;
    auto ons = onsFor (run (seq, p, g, 4.0), 4);
    check (ons.size() == 16, "16 hats");
    if (ons.size() == 16)
    {
        checkNear (ons[0].beat, 0.0,   1e-9, "first of the pair stays");
        checkNear (ons[1].beat, 0.375, 1e-9, "75 % swing: second 1/16 lands at 3/4 of the pair");
        checkNear (ons[2].beat, 0.5,   1e-9, "next pair starts on the grid");
    }

    g.swing = 50; g.swingVel = 0;
    auto s2Owned = std::make_unique<Sequencer>(); auto& s2 = *s2Owned;
    auto v = onsFor (run (s2, p, g, 1.0), 4);
    if (v.size() >= 2)
        check (v[1].velocity < v[0].velocity, "swing velocity < 50 accents the first of each pair");
}

// ---------------------------------------------------------------------------
static void testMuteSolo()
{
    section ("Mute and solo");

    Project p = emptyProject();
    p.tracks[0].patterns[0].fills = 4;
    p.tracks[2].patterns[0].fills = 2;

    Globals g;
    g.muted[0] = true;
    auto aOwned = std::make_unique<Sequencer>(); auto& a = *aOwned;
    auto m = run (a, p, g, 4.0);
    check (onsFor (m, 0).empty(),  "muted lane is silent");
    check (onsFor (m, 2).size() == 2, "other lanes play on");

    Globals s;
    s.soloed[2] = true;
    auto bOwned = std::make_unique<Sequencer>(); auto& b = *bOwned;
    auto ms = run (b, p, s, 4.0);
    check (onsFor (ms, 0).empty() && onsFor (ms, 2).size() == 2, "solo silences everything else");

    // Mute is a gate: unmuting mid-bar picks up in place.
    Globals g3;
    auto cOwned = std::make_unique<Sequencer>(); auto& c = *cOwned;
    run (c, p, g3, 2.0);
    check (c.playingStep (0) >= 0, "a lane reports its step while playing");
}

// ---------------------------------------------------------------------------
static void testPatternsAndChains()
{
    section ("Patterns and chains");

    Project p = emptyProject();
    Track& t = p.tracks[0];
    t.patterns[0].fills = 1;               // one hit per bar
    t.patterns[1].fills = 4;               // four per bar
    t.patterns[2].fills = 2;

    // Chain: P1 x2, P2 x1, P3 x1 -> 1 + 1 + 4 + 2 = 8 hits in 4 bars.
    t.chainOn = 1;
    t.chainLen = 3;
    t.chainPattern[0] = 0; t.chainRepeats[0] = 2;
    t.chainPattern[1] = 1; t.chainRepeats[1] = 1;
    t.chainPattern[2] = 2; t.chainRepeats[2] = 1;

    Globals g;
    auto seqOwned = std::make_unique<Sequencer>(); auto& seq = *seqOwned;
    auto ons = onsFor (run (seq, p, g, 16.0), 0);
    check (ons.size() == 8, "chain P1x2 P2 P3 over 4 bars: 8 hits (got " + std::to_string (ons.size()) + ")");

    // And it loops: 8 more in the next 4 bars.
    auto seq2Owned = std::make_unique<Sequencer>(); auto& seq2 = *seq2Owned;
    auto ons2 = onsFor (run (seq2, p, g, 32.0), 0);
    check (ons2.size() == 16, "chain loops (got " + std::to_string (ons2.size()) + ")");

    // Selecting a pattern by parameter waits for the quantise boundary.
    Project q = emptyProject();
    q.tracks[0].patterns[0].fills = 1;
    q.tracks[0].patterns[1].fills = 4;
    Globals sel;
    sel.quantise = kNextBar;
    auto s3Owned = std::make_unique<Sequencer>(); auto& s3 = *s3Owned;
    std::vector<Msg> out;
    const double bps = 120.0 / (60.0 * kSr);
    const int block = 480;
    for (long long s = 0; s < (long long) (8.0 / bps); s += block)
    {
        BlockInfo b;
        b.playing = true; b.ppq = s * bps; b.bpm = 120.0; b.sampleRate = kSr; b.numSamples = block;
        if (b.ppq >= 1.5) sel.selected[0] = 1; // change half way through bar 1
        s3.process (b, sel, q, [&] (int off, const NoteEvent& e)
        {
            out.push_back ({ s + off, e.beat, e.track, e.note, e.velocity, e.on });
        });
    }
    auto qo = onsFor (out, 0);
    // Bar 1: P1 (1 hit). Bar 2: P2 (4 hits).
    check (qo.size() == 5, "pattern change lands on the next bar (got " + std::to_string (qo.size()) + ")");
    check (s3.playingPattern (0) == 1, "lane reports the new pattern");
}

// ---------------------------------------------------------------------------
static void testScenes()
{
    section ("Scenes");

    Project p = emptyProject();
    p.tracks[0].patterns[0].fills = 1;
    p.tracks[0].patterns[1].fills = 4;

    p.scenes[0].used = 1; p.scenes[0].pattern[0] = 0;
    p.scenes[1].used = 1; p.scenes[1].pattern[0] = 1;
    p.scenes[2].used = 1; p.scenes[2].muted = 1u; // lane 0 muted

    p.sceneMode = kPlayLoop;
    p.sceneChainLen = 3;
    p.sceneChainScene[0] = 0; p.sceneChainBars[0] = 1;
    p.sceneChainScene[1] = 1; p.sceneChainBars[1] = 1;
    p.sceneChainScene[2] = 2; p.sceneChainBars[2] = 1;

    Globals g;
    g.quantise = kNextBar;
    auto seqOwned = std::make_unique<Sequencer>(); auto& seq = *seqOwned;
    auto ons = onsFor (run (seq, p, g, 12.0, 120.0, 480), 0);
    // 1 + 4 + 0 = 5 hits in 3 bars.
    check (ons.size() == 5, "scene chain drives patterns and mutes (got " + std::to_string (ons.size()) + ")");

    // Same again with blocks that don't divide the bar: scene changes must
    // land on the bar line, not a bar late.
    for (int block : { 512, 333, 4096 })
    {
        auto oddOwned = std::make_unique<Sequencer>(); auto& odd = *oddOwned;
        auto oo = onsFor (run (odd, p, g, 12.0, 120.0, block), 0);
        check (oo.size() == 5, "scene chain at block size " + std::to_string (block) + " (got " + std::to_string (oo.size()) + ")");
    }

    p.sceneMode = kPlayOnce;
    auto onceOwned = std::make_unique<Sequencer>(); auto& once = *onceOwned;
    auto o = onsFor (run (once, p, g, 24.0, 120.0, 480), 0);
    check (o.size() == 5, "PLAY runs the chain once then holds the last scene (got " + std::to_string (o.size()) + ")");
}

// ---------------------------------------------------------------------------
static void testTransport()
{
    section ("Transport");

    Project p = emptyProject();
    p.tracks[0].patterns[0].fills = 4;
    p.tracks[0].patterns[0].gate = 200; // long notes so one is held at stop

    Globals g;
    auto seqOwned = std::make_unique<Sequencer>(); auto& seq = *seqOwned;
    std::vector<Msg> out;
    auto block = [&] (bool playing, double ppq)
    {
        BlockInfo b;
        b.playing = playing; b.ppq = ppq; b.bpm = 120.0; b.sampleRate = kSr; b.numSamples = 512;
        seq.process (b, g, p, [&] (int off, const NoteEvent& e)
        {
            out.push_back ({ off, e.beat, e.track, e.note, e.velocity, e.on });
        });
    };

    block (true, 0.0);
    check (! out.empty() && out[0].on, "a note at the downbeat");
    block (false, 0.05);
    check (! out.empty() && ! out.back().on, "stopping closes the held note");
    check (seq.playingStep (0) == -1, "no step shown when stopped");

    // Starting mid-bar lands on the right step.
    out.clear();
    auto midOwned = std::make_unique<Sequencer>(); auto& mid = *midOwned;
    BlockInfo b;
    b.playing = true; b.ppq = 2.0; b.bpm = 120.0; b.sampleRate = kSr; b.numSamples = 64;
    mid.process (b, g, p, [&] (int off, const NoteEvent& e)
    {
        out.push_back ({ off, e.beat, e.track, e.note, e.velocity, e.on });
    });
    check (out.size() == 1 && out[0].on && out[0].sample == 0, "starting on beat 3 plays beat 3's kick");

    // A jump re-seeks.
    out.clear();
    b.ppq = 9.0;
    mid.process (b, g, p, [&] (int off, const NoteEvent& e)
    {
        out.push_back ({ off, e.beat, e.track, e.note, e.velocity, e.on });
    });
    bool hasOn = false;
    for (auto& x : out) hasOn = hasOn || x.on;
    check (hasOn, "jumping to a beat plays that beat");
}

// ---------------------------------------------------------------------------
static void testStress()
{
    section ("Stress");

    Project p;
    initKit (p);
    for (auto& t : p.tracks)
        for (auto& pat : t.patterns)
        {
            pat.steps = 32; pat.fills = 32;
            pat.ratchet.fill (8);
            pat.nudge.fill (-50);
            pat.division = kThirtySecondTriplet;
        }
    Globals g;
    g.swing = 99; g.humanise = 100;
    auto seqOwned = std::make_unique<Sequencer>(); auto& seq = *seqOwned;
    auto m = run (seq, p, g, 8.0, 300.0, 37);
    check (wellFormed (m), "worst case: every lane, every step, 8x ratchets, max swing and humanise");
    check (! m.empty(), "and it still plays");
}


// ---------------------------------------------------------------------------
// Run with a hook that can change the globals before each block.
template <typename Hook>
std::vector<Msg> runWith (Sequencer& seq, const Project& p, Globals g, double beats, int block, Hook&& hook)
{
    std::vector<Msg> out;
    const double bps = 120.0 / (60.0 * kSr);
    const long long total = (long long) std::llround (beats / bps);
    for (long long s = 0; s < total; s += block)
    {
        BlockInfo b;
        b.playing = true; b.ppq = s * bps; b.bpm = 120.0; b.sampleRate = kSr;
        b.numSamples = (int) std::min<long long> (block, total - s);
        hook (g, b.ppq);
        seq.process (b, g, p, [&] (int off, const NoteEvent& e)
        {
            out.push_back ({ s + off, e.beat, e.track, e.note, e.velocity, e.on });
        });
    }
    return out;
}

static void testScatter()
{
    section ("Scatter");

    // Lane 0: steps 0 and 1 lit with different velocities, nothing else.
    Project p = emptyProject();
    Pattern& k = p.tracks[0].patterns[0];
    k.steps = 16; k.fills = 0;
    k.forceOn = 0b11u;
    k.stepVel[0] = 100; k.stepVel[1] = 50;

    // REVERSE, depth 1 (a one-beat window): the beat plays back to front.
    {
        Globals g; g.scatter = true; g.scatterType = 1; g.scatterDepth = 1;
        auto seqOwned = std::make_unique<Sequencer>(); auto& seq = *seqOwned;
        auto ons = onsFor (run (seq, p, g, 1.0), 0);
        check (ons.size() == 2, "reverse: two hits in the beat (got " + std::to_string (ons.size()) + ")");
        if (ons.size() == 2)
        {
            checkNear (ons[0].beat, 0.50, 1e-9, "reverse: step 2 now plays third");
            check (ons[0].velocity == 50, "reverse: ... with step 2's velocity");
            checkNear (ons[1].beat, 0.75, 1e-9, "reverse: step 1 now plays last");
            check (ons[1].velocity == 100, "reverse: ... with step 1's velocity");
        }
    }

    // STUTTER, depth 1: every pair repeats its first step.
    {
        Globals g; g.scatter = true; g.scatterType = 0; g.scatterDepth = 1;
        auto seqOwned = std::make_unique<Sequencer>(); auto& seq = *seqOwned;
        auto ons = onsFor (run (seq, p, g, 1.0), 0);
        check (ons.size() == 2, "stutter: two hits (got " + std::to_string (ons.size()) + ")");
        if (ons.size() == 2)
            check (ons[1].velocity == 90, "stutter: the repeat is step 1 again (x0.9)");
    }

    // HALF on four on the floor: half as many kicks.
    {
        Project q = emptyProject();
        q.tracks[0].patterns[0].fills = 4;
        Globals g; g.scatter = true; g.scatterType = 2; g.scatterDepth = 7;
        auto seqOwned = std::make_unique<Sequencer>(); auto& seq = *seqOwned;
        auto ons = onsFor (run (seq, q, g, 4.0), 0);
        check (ons.size() == 2, "half time: 2 kicks in the bar (got " + std::to_string (ons.size()) + ")");
    }

    // Engaging is quantised to the next 1/16, and letting go returns the
    // pattern exactly where it would have been.
    {
        Project q = emptyProject();
        Pattern& h = q.tracks[4].patterns[0];
        h.steps = 16; h.fills = 16;
        for (int i = 0; i < 16; ++i) h.stepVel[(size_t) i] = (uint8_t) (40 + i * 5);

        auto plainOwned = std::make_unique<Sequencer>(); auto& plain = *plainOwned;

        auto thrownOwned = std::make_unique<Sequencer>(); auto& thrown = *thrownOwned;
        auto base = run (plain, q, Globals {}, 8.0, 120.0, 256);
        auto withScatter = runWith (thrown, q, Globals {}, 8.0, 256, [] (Globals& g, double ppq)
        {
            g.scatterType = 1; g.scatterDepth = 9;
            g.scatter = ppq >= 0.1 && ppq < 4.0;
        });

        auto onsBase = onsFor (base, 4), onsThrown = onsFor (withScatter, 4);
        bool firstSame = ! onsThrown.empty() && onsThrown[0].velocity == onsBase[0].velocity;
        check (firstSame, "the step before the next 1/16 plays as written");

        // After the throw (and the cell it finishes) everything matches.
        std::vector<Msg> a, b;
        for (auto& m : onsBase)   if (m.beat >= 4.5) a.push_back (m);
        for (auto& m : onsThrown) if (m.beat >= 4.5) b.push_back (m);
        bool same = a.size() == b.size() && ! a.empty();
        for (size_t i = 0; same && i < a.size(); ++i)
            same = a[i].sample == b[i].sample && a[i].velocity == b[i].velocity;
        check (same, "releasing scatter drops straight back into the pattern");

        // And during the throw it really is different.
        bool differs = false;
        for (size_t i = 0; i < onsBase.size() && i < onsThrown.size(); ++i)
            if (onsBase[i].beat < 4.0 && onsBase[i].velocity != onsThrown[i].velocity) differs = true;
        check (differs, "while engaged, the steps are rearranged");
    }

    // Every type at every depth: well formed and block-size independent.
    {
        Project busy;
        initKit (busy);
        busy.tracks[4].patterns[0].ratchet[3] = 3;
        busy.tracks[0].patterns[0].nudge[2] = -20;
        busy.tracks[2].patterns[0].steps = 7;

        bool allWell = true, allSame = true, anySound = true;
        std::string firstBad;
        for (int type = 0; type < kNumScatterTypes; ++type)
            for (int depth = 1; depth <= kMaxScatterDepth; ++depth)
            {
                Globals g; g.scatter = true; g.scatterType = type; g.scatterDepth = depth; g.swing = 60;
                auto aOwned = std::make_unique<Sequencer>(); auto& a = *aOwned;
                auto bOwned = std::make_unique<Sequencer>(); auto& b = *bOwned;
                auto ma = run (a, busy, g, 8.0, 120.0, 64);
                auto mb = run (b, busy, g, 8.0, 120.0, 1999);
                const bool well = wellFormed (ma) && wellFormed (mb);
                // Block order differs across lanes, so compare per lane.
                bool same = true;
                for (int t = 0; t < kNumTracks && same; ++t)
                {
                    std::vector<Msg> x, y;
                    for (auto& m : ma) if (m.track == t) x.push_back (m);
                    for (auto& m : mb) if (m.track == t) y.push_back (m);
                    same = x.size() == y.size();
                    for (size_t i = 0; same && i < x.size(); ++i)
                        // +/- 1 sample: a hit exactly half way between two samples may
                        // round either way depending on where the block starts.
                        same = std::llabs (x[i].sample - y[i].sample) <= 1 && x[i].on == y[i].on
                            && x[i].velocity == y[i].velocity;
                }
                if ((! well || ! same) && firstBad.empty())
                    firstBad = std::string (kScatterTypeNames[(size_t) type]) + " depth " + std::to_string (depth)
                             + (well ? "" : " (malformed)") + (same ? "" : " (block-size dependent)");
                allWell = allWell && well;
                allSame = allSame && same;
                anySound = anySound && ! ma.empty();
            }
        check (allWell, "every type and depth: notes balanced, time never runs backwards" + (firstBad.empty() ? "" : "  first: " + firstBad));
        check (allSame, "every type and depth: identical at block sizes 64 and 1999" + (firstBad.empty() ? "" : "  first: " + firstBad));
        check (anySound, "every type and depth makes sound");
    }

    // Names line up with the types.
    check (std::string (kScatterTypeNames[1]) == "REVERSE" && std::string (kScatterTypeNames[9]) == "SCRAMBLE",
           "type names");
}

// ---------------------------------------------------------------------------
int main()
{
    testEuclid();
    testPatternMask();
    testTiming();
    testPolymeter();
    testLayers();
    testSwing();
    testMuteSolo();
    testPatternsAndChains();
    testScenes();
    testTransport();
    testStress();
    testScatter();

    std::printf ("\n%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
