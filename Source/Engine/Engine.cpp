// Engine.cpp — the non-templated half of the Beadwork engine.

#include "Engine/Engine.h"

#include <algorithm>

namespace beadwork
{

// ---------------------------------------------------------------------------
// Euclid (Bjorklund)
// ---------------------------------------------------------------------------
namespace
{
struct Seq
{
    uint32_t bits = 0; // element j is bit j
    int      len  = 0;
};

inline Seq concat (Seq a, Seq b) noexcept
{
    return { a.bits | (b.bits << a.len), a.len + b.len };
}
} // namespace

uint32_t euclid (int steps, int fills, int rotate) noexcept
{
    steps = std::clamp (steps, 1, kMaxSteps);
    fills = std::clamp (fills, 0, steps);

    const uint32_t all = steps >= 32 ? 0xFFFFFFFFu : ((1u << steps) - 1u);

    uint32_t m = 0;
    if (fills == 0)
        m = 0;
    else if (fills == steps)
        m = all;
    else
    {
        // Bjorklund: start with `fills` groups of [1] and the rest [0], then keep
        // pairing the remainder onto the front groups until one remainder is left.
        std::array<Seq, kMaxSteps> a {}, b {}, na {}, nb {};
        int na_ = fills, nb_ = steps - fills;
        for (int i = 0; i < na_; ++i) a[(size_t) i] = { 1u, 1 };
        for (int i = 0; i < nb_; ++i) b[(size_t) i] = { 0u, 1 };

        while (nb_ > 1)
        {
            const int k = std::min (na_, nb_);
            for (int i = 0; i < k; ++i)
                na[(size_t) i] = concat (a[(size_t) i], b[(size_t) i]);

            int rest = 0;
            if (na_ > k)
                for (int i = k; i < na_; ++i) nb[(size_t) rest++] = a[(size_t) i];
            else
                for (int i = k; i < nb_; ++i) nb[(size_t) rest++] = b[(size_t) i];

            a = na; b = nb;
            na_ = k; nb_ = rest;
        }

        Seq out;
        for (int i = 0; i < na_; ++i) out = concat (out, a[(size_t) i]);
        for (int i = 0; i < nb_; ++i) out = concat (out, b[(size_t) i]);
        m = out.bits & all;
    }

    // Rotate: a positive value moves every bead later.
    int r = rotate % steps;
    if (r < 0)
        r += steps;
    if (r == 0)
        return m;

    uint32_t rotated = 0;
    for (int i = 0; i < steps; ++i)
        if ((m >> i) & 1u)
            rotated |= 1u << ((i + r) % steps);
    return rotated;
}

// ---------------------------------------------------------------------------
// Starter kit
// ---------------------------------------------------------------------------
void initKit (Project& p) noexcept
{
    struct Lane { const char* name; int note; int steps; int fills; int rotate; int velocity; };

    // General MIDI drum notes, which is also where Ableton's drum racks put
    // their pads (C1 = 36 is the bottom-left pad).
    static constexpr Lane lanes[kNumTracks] {
        { "Kick",    36, 16, 4, 0, 110 },
        { "Rim",     37, 16, 3, 3,  80 },
        { "Snare",   38, 16, 2, 4, 105 },
        { "Clap",    39, 16, 0, 0, 100 },
        { "Cl Hat",  42, 16, 8, 0,  80 },
        { "Op Hat",  46, 16, 4, 2,  85 },
        { "Lo Tom",  41, 16, 0, 0,  95 },
        { "Mid Tom", 45, 16, 0, 0,  95 },
        { "Hi Tom",  48, 16, 0, 0,  95 },
        { "Pedal",   44, 16, 0, 0,  80 },
        { "Crash",   49, 16, 0, 0, 100 },
        { "Ride",    51, 16, 0, 0,  85 },
    };

    p = Project {};

    for (int i = 0; i < kNumTracks; ++i)
    {
        Track& t = p.tracks[(size_t) i];
        const Lane& l = lanes[i];
        t.note = (uint8_t) l.note;
        std::strncpy (t.name.data(), l.name, kNameChars - 1);

        Pattern base;
        base.steps    = (uint8_t) l.steps;
        base.fills    = (uint8_t) l.fills;
        base.rotate   = (int8_t) l.rotate;
        base.velocity = (uint8_t) l.velocity;

        // Every slot starts as the same figure so switching to a fresh slot
        // isn't a jolt; it's a copy waiting to be bent into something else.
        t.patterns.fill (base);

        for (int c = 0; c < kMaxChain; ++c)
            t.chainPattern[(size_t) c] = (uint8_t) c;
    }

    for (int s = 0; s < kMaxSceneChain; ++s)
        p.sceneChainScene[(size_t) s] = (uint8_t) (s % kNumScenes);
}

// ---------------------------------------------------------------------------
// Swing
// ---------------------------------------------------------------------------
namespace
{
// Which half of the swing pair a beat sits on: 0 first, 1 second, -1 neither
// (a hit off the swing grid, e.g. a triplet against straight swing).
int swingSlot (double beat, double anchorBeat, const Globals& g) noexcept
{
    const double grid = kDivisionBeats[(size_t) std::clamp (g.swingGrid, 0, kNumDivisions - 1)];
    const double pair = 2.0 * grid;
    double f = std::fmod (beat - anchorBeat, pair);
    if (f < 0.0)
        f += pair;

    constexpr double eps = 1.0e-6;
    if (f < eps || pair - f < eps)
        return 0;
    if (std::abs (f - grid) < eps)
        return 1;
    return -1;
}
} // namespace

double applySwing (double beat, double anchorBeat, const Globals& g) noexcept
{
    if (g.swing <= 50 || swingSlot (beat, anchorBeat, g) != 1)
        return beat;

    const double grid = kDivisionBeats[(size_t) std::clamp (g.swingGrid, 0, kNumDivisions - 1)];
    const double amt  = std::clamp (g.swing, 50, 99) / 100.0;
    // The late slot moves from half way through the pair (50 %) to `amt` of it.
    return beat + (2.0 * amt - 1.0) * grid;
}

double swingVelocityScale (double beat, double anchorBeat, const Globals& g) noexcept
{
    const int sv = std::clamp (g.swingVel, 0, 100);
    if (sv == 50)
        return 1.0;

    const int slot = swingSlot (beat, anchorBeat, g);
    if (slot < 0)
        return 1.0;

    // Below 50 the second of each pair is pulled down (first accented); above
    // 50 the first is pulled down (second accented). Full travel is -60 %.
    const double depth = std::abs (sv - 50) / 50.0 * 0.6;
    if (sv < 50 && slot == 1) return 1.0 - depth;
    if (sv > 50 && slot == 0) return 1.0 - depth;
    return 1.0;
}

// ---------------------------------------------------------------------------
// Scatter
// ---------------------------------------------------------------------------
namespace
{
// A stable value in [0, 1) for a (type, a, b) triple: the dice scatter uses.
// It doesn't depend on the session seed, so a type and depth always sound the
// same, the way a hardware preset does.
double scatterDice (int type, int64_t a, int64_t b) noexcept
{
    return rand01 ((uint64_t) 0x5CA77E5 + (uint64_t) type, (uint32_t) a, (uint32_t) b, kFieldRatchet);
}

// Window lengths that grow with depth: a beat, two beats, a bar.
int windowFor (int depth) noexcept { return depth <= 3 ? 4 : (depth <= 6 ? 8 : 16); }

int64_t floorDiv (int64_t a, int64_t b) noexcept { return a >= 0 ? a / b : -((-a + b - 1) / b); }
int64_t floorMod (int64_t a, int64_t b) noexcept { return a - floorDiv (a, b) * b; }
} // namespace

ScatterStep scatterStep (int type, int depth, int64_t cell) noexcept
{
    type  = std::clamp (type, 0, kNumScatterTypes - 1);
    depth = std::clamp (depth, 1, kMaxScatterDepth);

    ScatterStep s;
    switch (type)
    {
        case 0: // STUTTER: each group repeats its first step. Depth widens the group.
        {
            const int g = depth <= 3 ? 2 : (depth <= 6 ? 4 : (depth <= 9 ? 8 : 16));
            s.src = (float) -floorMod (cell, g);
            s.vel = floorMod (cell, g) == 0 ? 1.0f : 0.9f;
            break;
        }
        case 1: // REVERSE: the window's steps in backwards order.
        {
            const int n = windowFor (depth);
            const int i = (int) floorMod (cell, n);
            s.src = (float) (n - 1 - 2 * i);
            break;
        }
        case 2: // HALF: the window's first half, at half speed.
        {
            const int n = windowFor (depth);
            const int i = (int) floorMod (cell, n);
            s.src   = -0.5f * (float) i;
            s.speed = 0.5f;
            s.gate  = 1.6f;
            break;
        }
        case 3: // DOUBLE: the window twice, at double speed.
        {
            const int n = windowFor (depth);
            const int i = (int) floorMod (cell, n);
            s.src   = (float) ((2 * i) % n - i);
            s.speed = 2.0f;
            s.gate  = 0.6f;
            break;
        }
        case 4: // SKIP: jump ahead, dropping one step in every k.
        {
            const int k = std::max (2, 11 - depth);          // depth 1: drop 1 in 10 ... depth 10: 1 in 2
            const int i = (int) floorMod (cell, 16);
            const int jumped = i + i / (k - 1);
            s.src = (float) (jumped % 16 - i);
            break;
        }
        case 5: // SWAP: exchange neighbouring steps, more pairs with depth.
        {
            const int64_t pair = floorDiv (cell, 2);
            if (scatterDice (type, floorMod (pair, 8), 0) < depth / 10.0)
                s.src = floorMod (cell, 2) == 0 ? 1.0f : -1.0f;
            break;
        }
        case 6: // ROLL: the end of each bar rolls the step before it, building up.
        {
            const int i    = (int) floorMod (cell, 16);
            const int tail = std::max (1, (depth * 8 + 9) / 10);   // 1..8 cells
            const int from = 16 - tail;
            if (i >= from)
            {
                const float pos = tail > 1 ? (float) (i - from) / (float) (tail - 1) : 1.0f;
                s.src  = (float) (from - 1 - i);
                s.reps = pos < 0.5f ? 2 : 4;
                s.vel  = 0.55f + 0.45f * pos;
                s.gate = 0.5f;
            }
            break;
        }
        case 7: // TAPE STOP: the end of each bar slows to a halt.
        {
            const int i    = (int) floorMod (cell, 16);
            const int tail = std::max (2, (depth * 16 + 9) / 10);  // 2..16 cells
            const int from = 16 - tail;
            if (i >= from)
            {
                // Speed falls linearly to near zero; the source position is the
                // running sum of the speeds before this cell.
                double pos = from;
                for (int j = 0; j < i - from; ++j)
                    pos += 1.0 - (double) (j + 1) / (double) (tail + 1);
                const double sp = 1.0 - (double) (i - from + 1) / (double) (tail + 1);
                s.src   = (float) (pos - i);
                s.speed = (float) sp;
                s.gate  = (float) std::min (3.0, 1.0 / std::max (0.15, sp));
                s.vel   = (float) (0.55 + 0.45 * sp);
            }
            break;
        }
        case 8: // GATE: a rhythmic chop, like a trance gate. A fixed pattern of
                // silences that eats more of the bar with depth (about 8 %
                // per step of depth), never the downbeat. Notes shorten too.
        {
            const int i = (int) floorMod (cell, 16);
            s.gate = std::max (0.2f, 1.0f - 0.08f * (float) depth);
            if (i != 0 && scatterDice (type, i, 0) < depth * 0.085)
                s.mute = true;
            s.vel = i % 4 == 0 ? 1.0f : 0.85f;
            break;
        }
        case 9: // SCRAMBLE: steps drawn from anywhere in the bar; a new draw each
                // bar, repeating every four bars so it still reads as a pattern.
        {
            const int64_t bar = floorMod (floorDiv (cell, 16), 4);
            const int i = (int) floorMod (cell, 16);
            if (scatterDice (type, bar * 16 + i, 1) < depth / 10.0)
            {
                const int to = std::min (15, (int) (scatterDice (type, bar * 16 + i, 2) * 16.0));
                s.src = (float) (to - i);
            }
            break;
        }
        default: break;
    }
    return s;
}

void Sequencer::scatterCell (const Globals& g, TrackRt& t, double cell) noexcept
{
    const int64_t index = (int64_t) std::llround ((cell - anchor) / kScatterCell);
    const ScatterStep st = scatterStep (g.scatterType, g.scatterDepth, index);
    if (st.mute || t.loopBeats <= 0.0)
        return;

    const double L      = t.loopBeats;
    const double srcLen = std::min (L, std::max (0.0, (double) st.speed) * kScatterCell);
    if (srcLen <= 0.0)
        return;

    // The source span, as a phase in the current pass (wrapping round the loop,
    // so a short loop scatters against itself).
    double sa = std::fmod (cell + st.src * kScatterCell - t.loopStart, L);
    if (sa < 0.0)
        sa += L;

    const int    reps = std::clamp (st.reps, 1, 8);
    const double slot = kScatterCell / reps;
    const double end  = cell + kScatterCell - 1.0e-6;

    // Compact the pending list so there's room at the back.
    if (t.pendHead > 0)
    {
        for (int i = t.pendHead; i < t.pendCount; ++i)
            t.pending[(size_t) (i - t.pendHead)] = t.pending[(size_t) i];
        t.pendCount -= t.pendHead;
        t.pendHead = 0;
    }
    const int first = t.pendCount;

    for (int k = 0; k < t.hitCount; ++k)
    {
        const Hit& h = t.hits[(size_t) k];
        double d = (h.beat - t.loopStart) - sa;
        if (d < 0.0) d += L;
        if (d < 0.0 || d >= srcLen)
            continue;

        const double u = d / srcLen; // 0..1 through the source span
        double gate = (h.offBeat - h.beat) * st.gate;
        if (reps > 1)
            gate = std::min (gate, slot * 0.9);
        gate = std::max (gate, 1.0e-3);
        const int vel = std::clamp ((int) std::lround (h.velocity * st.vel), 1, 127);

        for (int r = 0; r < reps && t.pendCount < kMaxPending; ++r)
        {
            Hit& o = t.pending[(size_t) t.pendCount++];
            o.beat     = std::min (end, cell + (u + r) * slot);
            o.offBeat  = o.beat + gate;
            o.velocity = (uint8_t) vel;
            o.step     = h.step;
        }
    }

    // Keep this cell's hits in time order (the source can wrap round the loop).
    for (int i = first + 1; i < t.pendCount; ++i)
    {
        const Hit key = t.pending[(size_t) i];
        int j = i - 1;
        while (j >= first && t.pending[(size_t) j].beat > key.beat)
        {
            t.pending[(size_t) (j + 1)] = t.pending[(size_t) j];
            --j;
        }
        t.pending[(size_t) (j + 1)] = key;
    }
}

// ---------------------------------------------------------------------------
// Sequencer
// ---------------------------------------------------------------------------
void Sequencer::reset() noexcept
{
    for (auto& t : rt)
    {
        t.pattern = 0; t.chainSlot = 0; t.repeatsDone = 0;
        t.loopStart = 0.0; t.loopBeats = 4.0; t.pass = 0; t.dir = 1;
        t.silent = false; t.activeNote = -1; t.activeChan = 10; t.offAt = 0.0;
        t.uiStep = -1; t.hitCount = 0; t.hitCursor = 0;
        t.pendHead = t.pendCount = 0;
        t.scatterNext = t.scatterFrom = t.scatterUntil = -1.0e300;
    }
    wasPlaying = false;
    expectedPpq = 0.0;
    anchor = 0.0;
    restartRequested = false;
    restartAt = kNone;
    sceneSlot = 0;
    sceneIndex = 0;
    sceneActive = false;
}

double Sequencer::boundary (double from, uint8_t quantise, const TrackRt& t, double stepBeats) const noexcept
{
    auto nextOnGrid = [] (double x, double origin, double grid)
    {
        if (grid <= 0.0)
            return x;
        const double k = std::ceil ((x - origin) / grid - 1.0e-9);
        return origin + k * grid;
    };

    switch (quantise)
    {
        case kInstant:   return from;
        case kNextStep:  return nextOnGrid (from, t.loopStart, stepBeats);
        case kNextBeat:  return nextOnGrid (from, anchor, 1.0);
        case kNextBar:   return nextOnGrid (from, anchor, 4.0);
        case kTwoBars:   return nextOnGrid (from, anchor, 8.0);
        case kFourBars:  return nextOnGrid (from, anchor, 16.0);
        case kEightBars: return nextOnGrid (from, anchor, 32.0);
        case kEndOfLoop:
        default:         return t.loopStart + t.loopBeats;
    }
}

int Sequencer::patternForPass (const Track& tr, const Globals& g, int index, TrackRt& t) const noexcept
{
    int want;
    if (tr.chainOn)
    {
        const int len = std::clamp ((int) tr.chainLen, 1, kMaxChain);
        want = tr.chainPattern[(size_t) (t.chainSlot % len)];
    }
    else if (sceneActive)
        want = currentSceneData.pattern[(size_t) index];
    else
        want = g.selected[(size_t) index];

    return std::clamp (want, 0, kPatternsPerTrack - 1);
}

void Sequencer::beginPass (const Track& tr, const Globals& g, int index, TrackRt& t, double at) noexcept
{
    const Pattern& pat = tr.patterns[(size_t) std::clamp (t.pattern, 0, kPatternsPerTrack - 1)];

    t.loopStart = at;
    t.loopBeats = pat.loopBeats();
    t.hitCount  = 0;
    t.hitCursor = 0;

    const int      n     = std::clamp ((int) pat.steps, 1, kMaxSteps);
    const double   stepB = pat.stepBeats();
    const uint32_t mask  = pat.mask();
    const double   end   = at + t.loopBeats - 1.0e-9;

    const double patChance = std::clamp ((int) pat.chance, 0, 100) / 100.0
                           * std::clamp (g.chance, 0, 100) / 100.0;
    const double velScale  = std::clamp ((int) g.trackVel[(size_t) index], 0, 200) / 100.0
                           * std::clamp (g.velTrim, 0, 200) / 100.0;
    const double gateFrac  = std::clamp ((int) pat.gate, 1, 200) / 100.0;
    const double humanAmt  = std::clamp (g.humanise, 0, 100) / 100.0;

    const uint32_t pass = (uint32_t) t.pass;

    for (int i = 0; i < n; ++i)
    {
        // Which step of the pattern plays in time slot i.
        int s = i;
        switch (pat.direction)
        {
            case kReverse:  s = n - 1 - i; break;
            case kPendulum: s = t.dir > 0 ? i : n - 1 - i; break;
            case kRandom:
                s = std::min (n - 1, (int) (rand01 (g.seed, (uint32_t) index, pass * kMaxSteps + (uint32_t) i,
                                                    kFieldRandomStep) * n));
                break;
            default: break;
        }

        if (((mask >> s) & 1u) == 0)
            continue;

        // Chance: the pattern's chance times the step's own.
        const int sc = pat.stepChance[(size_t) s];
        const double chance = patChance * (sc == 0 ? 1.0 : std::clamp (sc, 1, 100) / 100.0);
        if (chance < 1.0
            && rand01 (g.seed, (uint32_t) index, pass * kMaxSteps + (uint32_t) i, kFieldChance) >= chance)
            continue;

        // Timing: the grid slot, swung, nudged, humanised.
        const double nominal = at + i * stepB;
        double beat = applySwing (nominal, anchor, g);
        beat += std::clamp ((int) pat.nudge[(size_t) s], -50, 50) / 100.0 * stepB;
        if (humanAmt > 0.0)
        {
            const double r = rand01 (g.seed, (uint32_t) index, pass * kMaxSteps + (uint32_t) i, kFieldHumanise);
            // At 100 % a hit can wander a quarter of a 1/16 either side.
            beat += (r - 0.5) * 2.0 * humanAmt * 0.25 * std::min (stepB, 0.25);
        }
        beat = std::clamp (beat, at, end);

        // Velocity.
        const int sv   = pat.stepVel[(size_t) s];
        double    vel  = sv == 0 ? pat.velocity : sv;
        if ((pat.accents >> s) & 1u)
            vel += pat.accent;
        vel *= velScale * swingVelocityScale (nominal, anchor, g);
        const int velocity = std::clamp ((int) std::lround (vel), 1, 127);

        // Ratchet: n evenly spaced hits inside the step.
        const int    reps = std::clamp ((int) pat.ratchet[(size_t) s], 1, kMaxRatchet);
        const double sub  = stepB / reps;
        double gateBeats  = sub * gateFrac;
        if (reps > 1)
            gateBeats = std::min (gateBeats, sub * 0.9); // leave a gap so each hit retriggers
        gateBeats = std::max (gateBeats, 1.0e-3);

        for (int r = 0; r < reps && t.hitCount < kMaxHits; ++r)
        {
            Hit& h    = t.hits[(size_t) t.hitCount++];
            h.beat    = std::clamp (beat + r * sub, at, end);
            h.offBeat = h.beat + gateBeats;
            h.velocity = (uint8_t) velocity;
            h.step     = (uint8_t) s;
        }
    }

    // Nudge and swing can move a hit past its neighbour; keep the list in time
    // order. At most 256 entries, nearly sorted already: insertion sort.
    for (int i = 1; i < t.hitCount; ++i)
    {
        const Hit key = t.hits[(size_t) i];
        int j = i - 1;
        while (j >= 0 && t.hits[(size_t) j].beat > key.beat)
        {
            t.hits[(size_t) (j + 1)] = t.hits[(size_t) j];
            --j;
        }
        t.hits[(size_t) (j + 1)] = key;
    }
}

void Sequencer::seekAll (const Globals& g, const Project& p, double at) noexcept
{
    updateScene (p, at);

    for (int i = 0; i < kNumTracks; ++i)
    {
        const Track& tr = p.tracks[(size_t) i];
        TrackRt&     t  = rt[(size_t) i];

        t.chainSlot   = 0;
        t.repeatsDone = 0;
        t.silent      = false;
        t.uiStep      = -1;
        t.pendHead    = t.pendCount = 0;
        t.scatterNext = t.scatterFrom = t.scatterUntil = -1.0e300;
        t.pattern     = patternForPass (tr, g, i, t);

        const double L = tr.patterns[(size_t) t.pattern].loopBeats();
        const double k = std::floor ((at - anchor) / L + 1.0e-9);
        t.pass = (int) k;
        t.dir  = (t.pass % 2 == 0) ? 1 : -1;

        beginPass (tr, g, i, t, anchor + k * L);
        while (t.hitCursor < t.hitCount && t.hits[(size_t) t.hitCursor].beat < at - 1.0e-9)
            ++t.hitCursor;
    }
}

int Sequencer::sceneAt (const Project& p, double beat, int* slotOut) const noexcept
{
    if (p.sceneMode == kPerform)
        return -1;

    const int len = std::clamp ((int) p.sceneChainLen, 1, kMaxSceneChain);
    auto barsOf = [&] (int i) { return 4.0 * std::clamp ((int) p.sceneChainBars[(size_t) i], 1, 64); };

    double total = 0.0;
    for (int i = 0; i < len; ++i)
        total += barsOf (i);

    // A hair of tolerance so a hit sitting exactly on a slot's first beat
    // belongs to that slot despite floating point.
    double pos = std::max (0.0, beat - anchor) + 1.0e-9;

    int slot = len - 1;                          // PLAY: hold the last slot
    if (p.sceneMode == kPlayLoop || pos < total)
    {
        pos = std::fmod (pos, total);
        double acc = 0.0;
        for (int i = 0; i < len; ++i)
        {
            acc += barsOf (i);
            if (pos < acc) { slot = i; break; }
        }
    }

    if (slotOut != nullptr)
        *slotOut = slot;
    const int scene = std::clamp ((int) p.sceneChainScene[(size_t) slot], 0, kNumScenes - 1);
    return p.scenes[(size_t) scene].used ? scene : -1;
}

void Sequencer::updateScene (const Project& p, double beat) noexcept
{
    int slot = 0;
    const int scene = sceneAt (p, beat, &slot);
    if (p.sceneMode == kPerform)
    {
        sceneActive = false;
        return;
    }
    sceneSlot   = slot;
    sceneIndex  = std::clamp ((int) p.sceneChainScene[(size_t) slot], 0, kNumScenes - 1);
    sceneActive = scene >= 0;
    if (sceneActive)
        currentSceneData = p.scenes[(size_t) scene];
}

} // namespace beadwork
