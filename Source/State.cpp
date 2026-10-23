#include "State.h"

namespace beadwork
{
// ---------------------------------------------------------------------------
// ProjectStore
// ---------------------------------------------------------------------------
ProjectStore::ProjectStore()
{
    initKit (editable);
    for (auto& s : slots)
        s = editable;
}

bool ProjectStore::publishIfDirty()
{
    if (! dirty.exchange (false))
        return false;
    publishNow();
    return true;
}

void ProjectStore::publishNow()
{
    const int pub  = published.load (std::memory_order_relaxed);
    const int busy = reading.load (std::memory_order_acquire);

    int next = 0;
    while (next == pub || next == busy)
        ++next;

    slots[(size_t) next] = editable;
    published.store (next, std::memory_order_release);
}

// ---------------------------------------------------------------------------
// Serialisation helpers
// ---------------------------------------------------------------------------
namespace
{
template <typename T, size_t N>
juce::String joinArray (const std::array<T, N>& a)
{
    juce::String s;
    s.preallocateBytes (N * 4);
    for (size_t i = 0; i < N; ++i)
    {
        if (i > 0) s << ',';
        s << (int) a[i];
    }
    return s;
}

template <typename T, size_t N>
void splitArray (const juce::String& text, std::array<T, N>& a, int lo, int hi)
{
    juce::StringArray parts;
    parts.addTokens (text, ",", {});
    for (size_t i = 0; i < N; ++i)
        a[i] = (T) juce::jlimit (lo, hi, i < (size_t) parts.size() ? parts[(int) i].getIntValue() : 0);
}

juce::String nameOf (const std::array<char, kNameChars>& n)
{
    return juce::String::fromUTF8 (n.data(), (int) strnlen (n.data(), kNameChars));
}

void setName (std::array<char, kNameChars>& n, const juce::String& s)
{
    n.fill (0);
    s.copyToUTF8 (n.data(), kNameChars - 1);
}

int intProp (const juce::ValueTree& v, const char* id, int fallback, int lo, int hi)
{
    return juce::jlimit (lo, hi, (int) v.getProperty (id, fallback));
}

uint32_t maskProp (const juce::ValueTree& v, const char* id)
{
    return (uint32_t) v.getProperty (id, "0").toString().getHexValue64();
}

juce::String hex (uint32_t m) { return juce::String::toHexString ((juce::int64) m); }
} // namespace

// ---------------------------------------------------------------------------
juce::ValueTree projectToTree (const Project& p)
{
    juce::ValueTree root (projectTag);
    root.setProperty ("version", 1, nullptr);
    root.setProperty ("sceneMode", (int) p.sceneMode, nullptr);
    root.setProperty ("sceneChainLen", (int) p.sceneChainLen, nullptr);
    root.setProperty ("sceneChainScene", joinArray (p.sceneChainScene), nullptr);
    root.setProperty ("sceneChainBars", joinArray (p.sceneChainBars), nullptr);

    for (int t = 0; t < kNumTracks; ++t)
    {
        const Track& tr = p.tracks[(size_t) t];
        juce::ValueTree lane ("Lane");
        lane.setProperty ("index", t, nullptr);
        lane.setProperty ("name", nameOf (tr.name), nullptr);
        lane.setProperty ("note", (int) tr.note, nullptr);
        lane.setProperty ("channel", (int) tr.channel, nullptr);
        lane.setProperty ("runMode", (int) tr.runMode, nullptr);
        lane.setProperty ("chainOn", (int) tr.chainOn, nullptr);
        lane.setProperty ("chainLen", (int) tr.chainLen, nullptr);
        lane.setProperty ("chainPattern", joinArray (tr.chainPattern), nullptr);
        lane.setProperty ("chainRepeats", joinArray (tr.chainRepeats), nullptr);

        for (int i = 0; i < kPatternsPerTrack; ++i)
        {
            const Pattern& pt = tr.patterns[(size_t) i];
            juce::ValueTree v ("Pattern");
            v.setProperty ("index", i, nullptr);
            v.setProperty ("steps", (int) pt.steps, nullptr);
            v.setProperty ("fills", (int) pt.fills, nullptr);
            v.setProperty ("rotate", (int) pt.rotate, nullptr);
            v.setProperty ("division", (int) pt.division, nullptr);
            v.setProperty ("fit", (int) pt.fit, nullptr);
            v.setProperty ("fitBars", (int) pt.fitBars, nullptr);
            v.setProperty ("velocity", (int) pt.velocity, nullptr);
            v.setProperty ("accent", (int) pt.accent, nullptr);
            v.setProperty ("chance", (int) pt.chance, nullptr);
            v.setProperty ("gate", (int) pt.gate, nullptr);
            v.setProperty ("direction", (int) pt.direction, nullptr);
            v.setProperty ("forceOn", hex (pt.forceOn), nullptr);
            v.setProperty ("forceOff", hex (pt.forceOff), nullptr);
            v.setProperty ("accents", hex (pt.accents), nullptr);
            v.setProperty ("ratchet", joinArray (pt.ratchet), nullptr);
            v.setProperty ("stepChance", joinArray (pt.stepChance), nullptr);
            v.setProperty ("stepVel", joinArray (pt.stepVel), nullptr);
            v.setProperty ("nudge", joinArray (pt.nudge), nullptr);
            lane.appendChild (v, nullptr);
        }
        root.appendChild (lane, nullptr);
    }

    for (int s = 0; s < kNumScenes; ++s)
    {
        const Scene& sc = p.scenes[(size_t) s];
        juce::ValueTree v ("Scene");
        v.setProperty ("index", s, nullptr);
        v.setProperty ("used", (int) sc.used, nullptr);
        v.setProperty ("name", nameOf (sc.name), nullptr);
        v.setProperty ("pattern", joinArray (sc.pattern), nullptr);
        v.setProperty ("muted", (int) sc.muted, nullptr);
        root.appendChild (v, nullptr);
    }

    return root;
}

void projectFromTree (const juce::ValueTree& root, Project& p)
{
    if (! root.hasType (projectTag))
        return;

    Project fresh;
    initKit (fresh);

    fresh.sceneMode     = (uint8_t) intProp (root, "sceneMode", kPerform, 0, 2);
    fresh.sceneChainLen = (uint8_t) intProp (root, "sceneChainLen", 1, 1, kMaxSceneChain);
    splitArray (root.getProperty ("sceneChainScene").toString(), fresh.sceneChainScene, 0, kNumScenes - 1);
    splitArray (root.getProperty ("sceneChainBars").toString(), fresh.sceneChainBars, 1, 64);

    for (const auto lane : root)
    {
        if (lane.hasType ("Lane"))
        {
            const int t = (int) lane.getProperty ("index", -1);
            if (! juce::isPositiveAndBelow (t, kNumTracks))
                continue;

            Track& tr = fresh.tracks[(size_t) t];
            setName (tr.name, lane.getProperty ("name").toString());
            tr.note     = (uint8_t) intProp (lane, "note", tr.note, 0, 127);
            tr.channel  = (uint8_t) intProp (lane, "channel", 0, 0, 16);
            tr.runMode  = (uint8_t) intProp (lane, "runMode", kFree, 0, 2);
            tr.chainOn  = (uint8_t) intProp (lane, "chainOn", 0, 0, 1);
            tr.chainLen = (uint8_t) intProp (lane, "chainLen", 1, 1, kMaxChain);
            splitArray (lane.getProperty ("chainPattern").toString(), tr.chainPattern, 0, kPatternsPerTrack - 1);
            splitArray (lane.getProperty ("chainRepeats").toString(), tr.chainRepeats, 1, 16);

            for (const auto v : lane)
            {
                const int i = (int) v.getProperty ("index", -1);
                if (! v.hasType ("Pattern") || ! juce::isPositiveAndBelow (i, kPatternsPerTrack))
                    continue;

                Pattern& pt = tr.patterns[(size_t) i];
                pt.steps     = (uint8_t) intProp (v, "steps", 16, 1, kMaxSteps);
                pt.fills     = (uint8_t) intProp (v, "fills", 0, 0, pt.steps);
                pt.rotate    = (int8_t)  intProp (v, "rotate", 0, -(kMaxSteps - 1), kMaxSteps - 1);
                pt.division  = (uint8_t) intProp (v, "division", kSixteenth, 0, kNumDivisions - 1);
                pt.fit       = (uint8_t) intProp (v, "fit", kPolymeter, 0, 1);
                pt.fitBars   = (uint8_t) intProp (v, "fitBars", 1, 1, 16);
                pt.velocity  = (uint8_t) intProp (v, "velocity", 100, 1, 127);
                pt.accent    = (uint8_t) intProp (v, "accent", 26, 0, 127);
                pt.chance    = (uint8_t) intProp (v, "chance", 100, 0, 100);
                pt.gate      = (uint8_t) intProp (v, "gate", 50, 1, 200);
                pt.direction = (uint8_t) intProp (v, "direction", kForward, 0, 3);
                pt.forceOn   = maskProp (v, "forceOn");
                pt.forceOff  = maskProp (v, "forceOff");
                pt.accents   = maskProp (v, "accents");
                splitArray (v.getProperty ("ratchet").toString(),    pt.ratchet,    0, 8);
                splitArray (v.getProperty ("stepChance").toString(), pt.stepChance, 0, 100);
                splitArray (v.getProperty ("stepVel").toString(),    pt.stepVel,    0, 127);
                splitArray (v.getProperty ("nudge").toString(),      pt.nudge,      -50, 50);
            }
        }
        else if (lane.hasType ("Scene"))
        {
            const int s = (int) lane.getProperty ("index", -1);
            if (! juce::isPositiveAndBelow (s, kNumScenes))
                continue;
            Scene& sc = fresh.scenes[(size_t) s];
            sc.used  = (uint8_t) intProp (lane, "used", 0, 0, 1);
            sc.muted = (uint16_t) intProp (lane, "muted", 0, 0, 0xFFFF);
            setName (sc.name, lane.getProperty ("name").toString());
            splitArray (lane.getProperty ("pattern").toString(), sc.pattern, 0, kPatternsPerTrack - 1);
        }
    }

    p = fresh;
}
} // namespace beadwork
