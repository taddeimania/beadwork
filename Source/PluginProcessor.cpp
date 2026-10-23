#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace beadwork;

BeadworkProcessor::BeadworkProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "BeadworkState", createParameterLayout())
{
    for (auto* p : getParameters())
        if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (p))
            params[withId->paramID] = apvts.getRawParameterValue (withId->paramID);

    for (auto& a : uiStep) a.store (-1);
    for (auto& a : uiPattern) a.store (0);
    for (auto& a : uiChain) a.store (0);
    for (auto& a : uiHits) a.store (0);

    seed.store ((uint64_t) juce::Random().nextInt64());
}

bool BeadworkProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void BeadworkProcessor::prepareToPlay (double, int)
{
    // Twelve lanes of 8x ratchets on 32 steps is the worst case per pass; a
    // block never holds more than a few hundred events, but the buffer keeps
    // its capacity once grown so size it generously up front.
    generated.ensureSize (8192);
}

// ---------------------------------------------------------------------------
void BeadworkProcessor::processBlock (juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = audio.getNumSamples();
    audio.clear(); // MIDI only; the audio bus exists because hosts want instruments to have one

    // Incoming MIDI passes straight through, so you can still play the rack
    // from a keyboard on Beadwork's track.

    const Project& project = store.beginRead();

    // --- globals -------------------------------------------------------------
    auto geti = [this] (const juce::String& id) { return juce::roundToInt (getParam (id)); };

    Globals g;
    g.rate       = ids::clockRateFromIndex (geti (ids::clockRate));
    g.channel    = juce::jlimit (1, 16, geti (ids::channel));
    g.swing      = geti (ids::swing);
    g.swingGrid  = ids::swingGridDivision (geti (ids::swingGrid));
    g.swingVel   = geti (ids::swingVel);
    g.humanise   = geti (ids::humanise);
    g.chance     = geti (ids::chance);
    g.velTrim    = geti (ids::velTrim);
    g.quantise   = (uint8_t) juce::jlimit (0, 7, geti (ids::quantise));
    g.masterMute = getParam (ids::masterMute) > 0.5f;
    g.scatter      = getParam (ids::scatter) > 0.5f;
    g.scatterType  = juce::jlimit (0, kNumScatterTypes - 1, geti (ids::scatterType));
    g.scatterDepth = juce::jlimit (1, kMaxScatterDepth, geti (ids::scatterDepth));
    g.seed       = seed.load();
    for (int t = 0; t < kNumTracks; ++t)
    {
        g.muted[(size_t) t]    = getParam (ids::mute (t)) > 0.5f;
        g.soloed[(size_t) t]   = getParam (ids::solo (t)) > 0.5f;
        g.selected[(size_t) t] = (uint8_t) juce::jlimit (0, kPatternsPerTrack - 1, geti (ids::pattern (t)) - 1);
        g.trackVel[(size_t) t] = (uint8_t) juce::jlimit (0, 200, geti (ids::level (t)));
    }

    if (restartRequested.exchange (false))
        sequencer.requestRestart();

    // --- host transport ------------------------------------------------------
    BlockInfo info;
    info.numSamples = numSamples;
    info.sampleRate = getSampleRate();
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            info.playing = pos->getIsPlaying();
            info.bpm     = pos->getBpm().orFallback (120.0);
            info.ppq     = pos->getPpqPosition().orFallback (0.0);
        }

    // --- generate ------------------------------------------------------------
    generated.clear();
    sequencer.process (info, g, project, [&] (int offset, const NoteEvent& e)
    {
        if (e.on)
        {
            generated.addEvent (juce::MidiMessage::noteOn (e.channel, e.note, (juce::uint8) e.velocity), offset);
            uiHits[(size_t) e.track].fetch_add (1, std::memory_order_relaxed);
        }
        else
        {
            generated.addEvent (juce::MidiMessage::noteOff (e.channel, e.note), offset);
        }
    });
    midi.addEvents (generated, 0, numSamples, 0);

    // --- UI state --------------------------------------------------------------
    for (int t = 0; t < kNumTracks; ++t)
    {
        uiStep[(size_t) t].store (sequencer.playingStep (t), std::memory_order_relaxed);
        uiPattern[(size_t) t].store (sequencer.playingPattern (t), std::memory_order_relaxed);
        uiChain[(size_t) t].store (sequencer.chainSlot (t), std::memory_order_relaxed);
    }
    uiSceneSlot.store (sequencer.currentSceneSlot(), std::memory_order_relaxed);
    uiScene.store (sequencer.currentScene(), std::memory_order_relaxed);
    uiSceneRunning.store (sequencer.isSceneChainRunning(), std::memory_order_relaxed);
    uiRunning.store (sequencer.isRunning(), std::memory_order_relaxed);
}

// ---------------------------------------------------------------------------
// Actions
// ---------------------------------------------------------------------------
void BeadworkProcessor::setParam (const juce::String& id, float plainValue)
{
    if (auto* p = apvts.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (plainValue));
        p->endChangeGesture();
    }
}

void BeadworkProcessor::reroll()
{
    seed.store ((uint64_t) juce::Random::getSystemRandom().nextInt64());
}

void BeadworkProcessor::recallScene (int s)
{
    if (! juce::isPositiveAndBelow (s, kNumScenes))
        return;
    const Scene& sc = store.view().scenes[(size_t) s];
    if (! sc.used)
        return;
    lastScene.store (s);
    for (int t = 0; t < kNumTracks; ++t)
    {
        setParam (ids::pattern (t), (float) (sc.pattern[(size_t) t] + 1));
        setParam (ids::mute (t), ((sc.muted >> t) & 1u) ? 1.0f : 0.0f);
    }
}

void BeadworkProcessor::storeScene (int s)
{
    if (! juce::isPositiveAndBelow (s, kNumScenes))
        return;
    Scene& sc = store.edit().scenes[(size_t) s];
    lastScene.store (s);
    sc.used  = 1;
    sc.muted = 0;
    for (int t = 0; t < kNumTracks; ++t)
    {
        sc.pattern[(size_t) t] = (uint8_t) juce::jlimit (0, kPatternsPerTrack - 1,
                                                         juce::roundToInt (getParam (ids::pattern (t))) - 1);
        if (getParam (ids::mute (t)) > 0.5f)
            sc.muted = (uint16_t) (sc.muted | (1u << t));
    }
    store.markDirty();
}

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------
juce::ValueTree BeadworkProcessor::captureState()
{
    store.publishIfDirty();

    auto state = apvts.copyState();
    for (int i = state.getNumChildren(); --i >= 0;)
        if (state.getChild (i).hasType (projectTag))
            state.removeChild (i, nullptr);

    state.appendChild (projectToTree (store.view()), nullptr);
    state.setProperty ("seed", juce::String::toHexString ((juce::int64) seed.load()), nullptr);
    state.setProperty ("presetName", presets.currentName(), nullptr);
    state.setProperty ("stateVersion", 1, nullptr);
    return state;
}

void BeadworkProcessor::restoreState (const juce::ValueTree& raw)
{
    if (! raw.hasType (apvts.state.getType()))
        return;

    auto state = raw.createCopy();

    auto project = state.getChildWithName (projectTag);
    if (project.isValid())
    {
        projectFromTree (project, store.edit());
        state.removeChild (project, nullptr);
    }
    store.markDirty();
    store.publishNow();

    if (state.hasProperty ("seed"))
        seed.store ((uint64_t) state["seed"].toString().getHexValue64());

    presets.restoreName (state["presetName"].toString());
    apvts.replaceState (state);
    lastScene.store (-1);
}

void BeadworkProcessor::applyFactoryPreset (const FactoryPreset& fp)
{
    Project& p = store.edit();
    initKit (p);
    if (fp.build)
        fp.build (p);
    lastScene.store (-1);
    store.markDirty();
    store.publishNow();

    setParam (ids::swing, (float) fp.swing);
    setParam (ids::swingGrid, (float) fp.swingGrid);
    for (int t = 0; t < kNumTracks; ++t)
    {
        setParam (ids::pattern (t), 1.0f);
        setParam (ids::mute (t), 0.0f);
        setParam (ids::solo (t), 0.0f);
    }
}

void BeadworkProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = captureState().createXml())
        copyXmlToBinary (*xml, destData);
}

void BeadworkProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        restoreState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* BeadworkProcessor::createEditor()
{
    return new BeadworkEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BeadworkProcessor();
}
