#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>

#include "Engine/Engine.h"
#include "Params.h"
#include "DrumMaps.h"
#include "PresetManager.h"
#include "State.h"

class BeadworkProcessor final : public juce::AudioProcessor
{
public:
    BeadworkProcessor();
    ~BeadworkProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return presets.currentName(); }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // --- state (message thread) -------------------------------------------
    juce::ValueTree captureState();
    void restoreState (const juce::ValueTree&);    // a session: groove and kit map
    void restoreGroove (const juce::ValueTree&);   // a groove file: leaves the kit map's name alone
    void applyFactoryPreset (const beadwork::FactoryPreset&);

    // --- actions (message thread) -----------------------------------------
    void requestRestart() { restartRequested.store (true); }
    void reroll();                         // new dice for chance, humanise, random direction
    void recallScene (int scene);          // writes the lane pattern / mute parameters
    void storeScene (int scene);           // snapshots every lane into the scene
    void storeSceneLane (int scene, int lane); // updates just one lane in a stored scene

    // The scene last recalled or stored (-1: none). Lit amber on the panel, the
    // way a picked pattern is. Kept here so it survives closing the editor.
    int  selectedScene() const noexcept { return lastScene.load(); }
    void setSelectedScene (int s) noexcept { lastScene.store (s); }
    void setParam (const juce::String& id, float plainValue);
    float getParam (const juce::String& id) const { return params.at (id)->load(); }

    // --- read by the editor ------------------------------------------------
    int  playingStep (int t) const noexcept    { return uiStep[(size_t) t].load(); }
    int  playingPattern (int t) const noexcept { return uiPattern[(size_t) t].load(); }
    int  chainSlot (int t) const noexcept      { return uiChain[(size_t) t].load(); }
    int  hitCount (int t) const noexcept       { return uiHits[(size_t) t].load(); }
    int  sceneSlot() const noexcept            { return uiSceneSlot.load(); }
    int  scene() const noexcept                { return uiScene.load(); }
    bool sceneChainRunning() const noexcept    { return uiSceneRunning.load(); }
    bool isRunning() const noexcept            { return uiRunning.load(); }

    juce::AudioProcessorValueTreeState apvts;
    beadwork::ProjectStore  store;
    beadwork::PresetManager presets { *this };   // grooves
    beadwork::MapManager    maps { store };       // kit maps

private:
    std::map<juce::String, std::atomic<float>*> params;

    beadwork::Sequencer sequencer;
    juce::MidiBuffer    generated;

    std::atomic<uint64_t> seed { 0x9E3779B97F4A7C15ull };
    std::atomic<bool>     restartRequested { false };
    std::atomic<int>      lastScene { -1 };

    std::array<std::atomic<int>, beadwork::kNumTracks> uiStep {}, uiPattern {}, uiChain {}, uiHits {};
    std::atomic<int>  uiSceneSlot { 0 }, uiScene { 0 };
    std::atomic<bool> uiSceneRunning { false }, uiRunning { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BeadworkProcessor)
};
