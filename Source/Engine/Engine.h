// Engine.h — the Euclidean drum sequencer behind Beadwork.
//
// Pure C++20, no JUCE dependency, so the whole thing can be unit-tested on its
// own (see Tests/EngineTests.cpp).
//
// Model
// -----
//   * Twelve independent TRACKS. Each track fires one MIDI note, so a track is
//     one lane of a drum rack (kick, snare, closed hat, ...).
//   * Each track owns sixteen PATTERNS. A pattern is a Euclidean recipe --
//     STEPS slots, FILLS beads distributed evenly among them, ROTATEd -- plus
//     per-step layers that are laid over the generated result: force-on,
//     force-off, accent, ratchet, chance, nudge and velocity.
//     The Euclidean figure is the starting point, never a cage.
//   * A pattern also carries its own step length (DIVISION) and step count, so
//     tracks of different lengths run against each other. Two ways to read
//     that, picked per pattern by FIT:
//       - polymeter  : a step is an absolute note value. Loops of different
//                      lengths drift against each other.
//       - polyrhythm : the steps are squeezed to fill FITBARS bars, so every
//                      track comes back round together but the beat differs.
//   * Patterns can be CHAINED per track: a list of (pattern, repeats) that
//     advances one slot per pass of the loop.
//   * Sixteen SCENES snapshot the selected pattern and mute of every track, and
//     scenes themselves can be chained with a bar count each. Scene chaining is
//     the only thing that overrides the per-track selection while it runs;
//     recalling a scene by hand is done by the editor, which writes the host
//     parameters so the host sees and can automate it.
//   * Output is MIDI only. Mute is a MIDI gate: a muted track keeps running and
//     keeps its place, it just stops speaking.
//
// Timing
// ------
//   The sequencer walks forward through host beats (ppq), scaled by a global
//   clock rate. It is sample-accurate inside a block and survives tempo
//   changes, loops and transport jumps. Because a chain's loop length depends
//   on which pattern is playing, the playhead cannot be derived in closed form
//   from the song position: each track keeps a small amount of runtime state
//   and is walked forward. A transport jump restarts the chains at the new
//   position rather than replaying the song silently to catch up.

#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <type_traits>

namespace beadwork
{

// ---------------------------------------------------------------------------
// Sizes
// ---------------------------------------------------------------------------
constexpr int kNumTracks       = 12;
constexpr int kMaxSteps        = 32;
constexpr int kPatternsPerTrack = 16;
constexpr int kNumScenes       = 16;
constexpr int kMaxChain        = 16; // slots in a track's pattern chain
constexpr int kMaxSceneChain   = 32; // slots in the scene chain
constexpr int kNameChars       = 16; // including the terminator

// ---------------------------------------------------------------------------
// Step length
// ---------------------------------------------------------------------------
constexpr int kNumDivisions = 8;

enum Division
{
    kQuarter = 0,
    kQuarterTriplet,
    kEighth,
    kEighthTriplet,
    kSixteenth,
    kSixteenthTriplet,
    kThirtySecond,
    kThirtySecondTriplet
};

// One step in quarter-note beats.
inline constexpr std::array<double, kNumDivisions> kDivisionBeats {
    1.0, 2.0 / 3.0, 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125, 1.0 / 12.0
};

inline constexpr std::array<const char*, kNumDivisions> kDivisionNames {
    "1/4", "1/4T", "1/8", "1/8T", "1/16", "1/16T", "1/32", "1/32T"
};

// ---------------------------------------------------------------------------
// Enumerations
// ---------------------------------------------------------------------------
enum Fit
{
    kPolymeter = 0, // a step is an absolute note value; loops drift
    kPolyrhythm     // the steps are squeezed into fitBars bars
};

enum Direction
{
    kForward = 0,
    kReverse,
    kPendulum,
    kRandom
};

inline constexpr std::array<const char*, 4> kDirectionNames { "FWD", "REV", "PEND", "RAND" };

// What a track does when the pattern under it changes.
enum RunMode
{
    kFree = 0,  // keep the playhead where it is
    kRelatch,   // restart the loop from step one
    kOneShot    // play the new pattern once, then hold silent until it changes
};

inline constexpr std::array<const char*, 3> kRunModeNames { "FREE", "RELATCH", "ONCE" };

// When a pending pattern change is allowed to take effect.
enum Quantise
{
    kInstant = 0,
    kNextStep,
    kNextBeat,
    kNextBar,
    kTwoBars,
    kFourBars,
    kEightBars,
    kEndOfLoop
};

inline constexpr std::array<const char*, 8> kQuantiseNames {
    "INST", "STEP", "BEAT", "1 BAR", "2 BAR", "4 BAR", "8 BAR", "LOOP"
};

enum SceneMode
{
    kPerform = 0, // scenes are recalled by hand only
    kPlayOnce,    // run the scene chain once, then stop advancing
    kPlayLoop     // run the scene chain round and round
};

inline constexpr std::array<const char*, 3> kSceneModeNames { "PERFORM", "PLAY", "LOOP" };

// ---------------------------------------------------------------------------
// Deterministic randomness
//
// Every random decision is a hash of (seed, track, pass, step, field), so a
// seed fully determines the dice: turning a knob re-reads the same dice rather
// than rolling new ones, and the result is identical on every machine and at
// every buffer size. Tests depend on that.
// ---------------------------------------------------------------------------
inline uint64_t mix64 (uint64_t x) noexcept
{
    // splitmix64 finaliser
    x += 0x9E3779B97F4A7C15ull;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
    return x ^ (x >> 31);
}

enum RandomField : uint32_t
{
    kFieldChance = 0,
    kFieldHumanise,
    kFieldVelocity,
    kFieldRandomStep,
    kFieldRatchet
};

// Uniform value in [0, 1).
inline double rand01 (uint64_t seed, uint32_t a, uint32_t b, uint32_t field) noexcept
{
    const uint64_t h = mix64 (seed ^ mix64 (((uint64_t) a << 32) ^ ((uint64_t) b << 8) ^ (uint64_t) field));
    return double (h >> 11) * (1.0 / 9007199254740992.0);
}

// ---------------------------------------------------------------------------
// Euclidean rhythm
// ---------------------------------------------------------------------------
// Bjorklund's algorithm: distribute `fills` beads as evenly as possible among
// `steps` slots. Returns a bitmask, bit 0 being the first step. Step 0 always
// carries a bead when fills > 0, which is what makes the figures feel like
// rhythms rather than arbitrary sets.
//
// `rotate` then turns the necklace: a positive value moves the beads later.
uint32_t euclid (int steps, int fills, int rotate) noexcept;

// ---------------------------------------------------------------------------
// Pattern
// ---------------------------------------------------------------------------
// Trivially copyable on purpose: the whole Project is snapshotted and handed to
// the audio thread as a block of bytes, and compared with memcmp to decide
// whether a cached figure is stale.
struct Pattern
{
    // --- the Euclidean recipe ---
    uint8_t steps    = 16;          // 1..kMaxSteps
    uint8_t fills    = 4;           // 0..steps
    int8_t  rotate   = 0;           // -(kMaxSteps-1)..(kMaxSteps-1)
    uint8_t division = kSixteenth;   // Division
    uint8_t fit      = kPolymeter;   // Fit
    uint8_t fitBars  = 1;           // 1..16, used when fit == kPolyrhythm

    // --- how the beads are played ---
    uint8_t velocity  = 100;        // 1..127, the un-accented level
    uint8_t accent    = 26;         // 0..127 added on accented steps
    uint8_t chance    = 100;        // 0..100 %, scales every step's chance
    uint8_t gate      = 50;         // 1..200 % of a step
    uint8_t direction = kForward;    // Direction

    uint8_t pad_ = 0;               // keep the per-step arrays aligned

    // --- per-step layers laid over the figure ---
    uint32_t forceOn  = 0;          // bead here whatever the figure says
    uint32_t forceOff = 0;          // silence here whatever the figure says
    uint32_t accents  = 0;          // play this bead at velocity + accent

    std::array<uint8_t, kMaxSteps> ratchet {};    // 0/1 = one hit, 2..8 = n hits
    std::array<uint8_t, kMaxSteps> stepChance {}; // 0 = inherit (100), else 1..100
    std::array<uint8_t, kMaxSteps> stepVel {};    // 0 = inherit, else 1..127
    std::array<int8_t,  kMaxSteps> nudge {};      // -50..+50 % of a step

    bool sameAs (const Pattern& o) const noexcept
    {
        return std::memcmp (this, &o, sizeof (Pattern)) == 0;
    }

    // The figure this pattern describes, with the per-step layers applied.
    uint32_t mask() const noexcept
    {
        const uint32_t all = steps >= 32 ? 0xFFFFFFFFu : ((1u << steps) - 1u);
        return ((euclid (steps, fills, rotate) | forceOn) & ~forceOff) & all;
    }

    double stepBeats() const noexcept
    {
        if (fit == kPolyrhythm && steps > 0)
            return (double) (fitBars < 1 ? 1 : fitBars) * 4.0 / (double) steps;
        return kDivisionBeats[(size_t) (division < kNumDivisions ? (int) division : (int) kSixteenth)];
    }

    double loopBeats() const noexcept { return (steps < 1 ? 1 : steps) * stepBeats(); }
};

static_assert (std::is_trivially_copyable_v<Pattern>);

// ---------------------------------------------------------------------------
// Track
// ---------------------------------------------------------------------------
struct Track
{
    uint8_t note     = 36;  // the MIDI note this lane fires
    uint8_t channel  = 0;   // 0 = follow the global channel, else 1..16
    uint8_t runMode  = kFree;
    uint8_t selected = 0;   // pattern index, used when the chain is off

    uint8_t chainOn  = 0;
    uint8_t chainLen = 1;   // 1..kMaxChain
    std::array<uint8_t, kMaxChain> chainPattern {};
    std::array<uint8_t, kMaxChain> chainRepeats {}; // 1..16 passes per slot
    std::array<uint8_t, 2> pad_ {};                 // explicit, so the struct has no hidden padding

    std::array<Pattern, kPatternsPerTrack> patterns {};
    std::array<char, kNameChars> name {};

    Track()
    {
        chainRepeats.fill (1);
        name.fill (0);
    }
};

static_assert (std::is_trivially_copyable_v<Track>);

// ---------------------------------------------------------------------------
// Scene
// ---------------------------------------------------------------------------
struct Scene
{
    uint8_t used = 0;
    std::array<uint8_t, kNumTracks> pattern {};
    uint8_t pad_ = 0;    // explicit, so the struct has no hidden padding
    uint16_t muted = 0;  // bit per track
    std::array<char, kNameChars> name {};

    Scene() { name.fill (0); }
};

static_assert (std::is_trivially_copyable_v<Scene>);

// ---------------------------------------------------------------------------
// Project: everything the editor owns and the audio thread reads
// ---------------------------------------------------------------------------
struct Project
{
    std::array<Track, kNumTracks> tracks {};
    std::array<Scene, kNumScenes> scenes {};

    uint8_t sceneMode     = kPerform;
    uint8_t sceneChainLen = 1;
    std::array<uint8_t, kMaxSceneChain> sceneChainScene {};
    std::array<uint8_t, kMaxSceneChain> sceneChainBars {};
    std::array<uint8_t, 2> pad_ {};

    Project() { sceneChainBars.fill (1); }
};

static_assert (std::is_trivially_copyable_v<Project>);

// No hidden padding anywhere: two projects with the same contents are the same
// bytes, so memcmp-based change detection (and the session round-trip test)
// can't be fooled by uninitialised padding.
static_assert (std::has_unique_object_representations_v<Pattern>);
static_assert (std::has_unique_object_representations_v<Track>);
static_assert (std::has_unique_object_representations_v<Scene>);
static_assert (std::has_unique_object_representations_v<Project>);

// Fill a project with a sensible twelve-lane drum kit: the General MIDI /
// Ableton drum-rack note layout and a starter figure on each lane.
void initKit (Project& p) noexcept;

// ---------------------------------------------------------------------------
// Globals: the automatable values, read fresh every block
// ---------------------------------------------------------------------------
struct Globals
{
    double rate = 1.0;      // Beadwork beats per host beat (clock divider)
    int    channel = 10;    // 1..16, the default MIDI channel

    int    swing      = 50;  // 50..99 %, 50 = straight
    int    swingGrid  = kEighth;  // Division the swing is measured on
    int    swingVel   = 50;  // 0..100, 50 = no accent; < 50 favours the first
                             // of the pair, > 50 the second
    int    humanise   = 0;   // 0..100 %, symmetric random timing spread
    int    chance     = 100; // 0..100 %, scales every pattern's chance
    int    velTrim    = 100; // 0..200 %, global velocity scale

    uint8_t quantise  = kNextBar; // when a pending pattern change lands
    bool    masterMute = false;

    // Per-track performance state. These mirror host parameters so the host can
    // automate them; the engine never writes them.
    std::array<uint8_t, kNumTracks> selected {};  // pattern index per track
    std::array<bool,    kNumTracks> muted {};
    std::array<bool,    kNumTracks> soloed {};
    std::array<uint8_t, kNumTracks> trackVel {};  // 0..200 % per track

    uint64_t seed = 0x9E3779B9ull;

    Globals()
    {
        selected.fill (0);
        muted.fill (false);
        soloed.fill (false);
        trackVel.fill (100);
    }
};

// ---------------------------------------------------------------------------
// Host block
// ---------------------------------------------------------------------------
struct BlockInfo
{
    bool   playing    = false;
    double ppq        = 0.0;     // host position in quarter notes at block start
    double bpm        = 120.0;
    double sampleRate = 48000.0;
    int    numSamples = 0;
};

// ---------------------------------------------------------------------------
// One scheduled MIDI event
// ---------------------------------------------------------------------------
struct NoteEvent
{
    double beat = 0.0;   // absolute Beadwork beat
    int    track = 0;
    int    channel = 10;
    int    note = 36;
    int    velocity = 0;
    bool   on = true;
};

// ---------------------------------------------------------------------------
// Sequencer
// ---------------------------------------------------------------------------
class Sequencer
{
public:
    Sequencer() { reset(); }

    void reset() noexcept;

    // Restart every track from step one on the next beat.
    void requestRestart() noexcept { restartRequested = true; }

    // Emit signature: void (int sampleOffset, const NoteEvent&)
    template <typename Emit>
    void process (const BlockInfo& b, const Globals& g, const Project& p, Emit&& emit);

    // --- state for the editor (read from the message thread) ----------------
    int  playingPattern (int track) const noexcept
    {
        return track >= 0 && track < kNumTracks ? rt[(size_t) track].pattern : 0;
    }
    int  playingStep (int track) const noexcept
    {
        return track >= 0 && track < kNumTracks ? rt[(size_t) track].uiStep : -1;
    }
    int  chainSlot (int track) const noexcept
    {
        return track >= 0 && track < kNumTracks ? rt[(size_t) track].chainSlot : 0;
    }
    int  currentSceneSlot() const noexcept { return sceneSlot; }
    int  currentScene() const noexcept { return sceneIndex; }
    bool isSceneChainRunning() const noexcept { return sceneActive; }
    bool isRunning() const noexcept { return wasPlaying; }

private:
    // One scheduled hit of a pass, already swung, nudged, humanised and
    // velocity-resolved. Ratchets are expanded into separate hits here, so
    // playback is a simple walk down a sorted list.
    struct Hit
    {
        double  beat     = 0.0; // absolute Beadwork beat of the note-on
        double  offBeat  = 0.0; // absolute beat of the note-off
        uint8_t velocity = 0;   // 0 means the step did not fire
        uint8_t step     = 0;   // which step of the pattern it came from
    };

    static constexpr int kMaxRatchet = 8;
    static constexpr int kMaxHits    = kMaxSteps * kMaxRatchet;

    struct TrackRt
    {
        int    pattern     = 0;
        int    chainSlot   = 0;
        int    repeatsDone = 0;
        double loopStart   = 0.0;
        double loopBeats   = 4.0;
        int    pass        = 0;
        int    dir         = 1;     // pendulum direction
        bool   silent      = false; // a one-shot that has finished its pass
        int    activeNote  = -1;
        int    activeChan  = 10;
        double offAt       = 0.0;
        int    uiStep      = -1;

        std::array<Hit, kMaxHits> hits {};
        int hitCount  = 0;
        int hitCursor = 0;
    };

    static constexpr double kNone = -std::numeric_limits<double>::infinity();

    // Where a pending change is allowed to land, at or after `from`.
    double boundary (double from, uint8_t quantise, const TrackRt& t, double stepBeats) const noexcept;

    // Which pattern a track should be playing for the pass starting now.
    int patternForPass (const Track& tr, const Globals& g, int index, TrackRt& t) const noexcept;

    // Lay out every hit of one pass, starting at beat `at`.
    void beginPass (const Track& tr, const Globals& g, int index, TrackRt& t, double at) noexcept;

    // Restart every track's chain at `at`, aligned to the anchor grid.
    void seekAll (const Globals& g, const Project& p, double at) noexcept;

    template <typename Emit>
    void runTrack (int index, const Track& tr, const Globals& g, const Project& p,
                   double from, double to, double bps, double p0, int numSamples, Emit& emit);

    template <typename Emit>
    void closeNote (TrackRt& t, int index, double at, double bps, double p0, int numSamples, Emit& emit);

    // Scene chain: the scene running at `beat`, or -1 when the chain is off or
    // that slot's scene is empty. `slotOut` receives the chain slot.
    int sceneAt (const Project& p, double beat, int* slotOut) const noexcept;

    // Make the scene running at `beat` the current one (what patternForPass
    // and the editor see).
    void updateScene (const Project& p, double beat) noexcept;

    std::array<TrackRt, kNumTracks> rt {};

    bool   wasPlaying       = false;
    double expectedPpq      = 0.0;
    double anchor           = 0.0;
    bool   restartRequested = false;
    double restartAt        = kNone;

    int   sceneSlot   = 0;
    int   sceneIndex  = 0;
    bool  sceneActive = false;  // the scene chain is running and its scene is in use
    Scene currentSceneData {};
};

// ---------------------------------------------------------------------------
// Swing & humanise
// ---------------------------------------------------------------------------
// Returns the beat a hit lands on once swing and humanise have moved it.
// Swing delays every second slot of the swing grid, exactly as the hardware
// does: 50 % is straight, 99 % pushes the late slot almost onto the next one.
double applySwing (double beat, double anchorBeat, const Globals& g) noexcept;

// How much swing changes a hit's velocity: < 50 accents the first of the pair,
// > 50 the second. Returns a multiplier around 1.0.
double swingVelocityScale (double beat, double anchorBeat, const Globals& g) noexcept;

} // namespace beadwork

#include "Engine.inl"
