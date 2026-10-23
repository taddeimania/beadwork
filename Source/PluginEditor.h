#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <optional>

#include "PluginProcessor.h"
#include "UI/Bank.h"
#include "UI/Lane.h"
#include "UI/Widgets.h"

// The panel is laid out in a fixed design space and scaled to fit the window.
class BeadworkPanel final : public juce::Component,
                            public beadwork::ui::LaneHost,
                            private juce::Timer
{
public:
    static constexpr int designW = 1500;
    static constexpr int designH = 940;

    explicit BeadworkPanel (BeadworkProcessor&);
    ~BeadworkPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override {}

    // --- LaneHost ------------------------------------------------------------
    beadwork::Project& project() override { return proc.store.edit(); }
    void edited() override;
    int  editPattern (int lane) override;
    int  playingStep (int lane) override { return proc.playingStep (lane); }
    bool playingShown (int lane) override;
    float flash (int lane) override { return flashes[(size_t) lane]; }
    beadwork::ui::Layer layer() override { return currentLayer; }
    void setLayer (beadwork::ui::Layer l)
    {
        currentLayer = l;
        for (int i = 0; i < 6; ++i)
            layerButtons[(size_t) i].setLit ((int) l == i);
        repaint();
    }
    int  focusedLane() override { return focus; }
    void focusLane (int lane) override;
    bool isMuted (int lane) override;
    bool isSoloed (int lane) override;
    void toggleMute (int lane) override;
    void toggleSolo (int lane) override;
    void openNoteMenu (int lane, juce::Component& target) override;
    void renameLane (int lane) override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

    void timerCallback() override;

    beadwork::Pattern& editing();
    beadwork::Track&   focusedTrack() { return project().tracks[(size_t) focus]; }

    void addParamKnob (beadwork::ui::Knob&, const juce::String& id, const juce::String& caption,
                       juce::Rectangle<int>, const juce::String& tip);
    void addModelKnob (beadwork::ui::ModelKnob&, const juce::String& caption, int lo, int hi,
                       juce::Rectangle<int>, const juce::String& tip,
                       std::function<int()> get, std::function<void (int)> set);
    void addChoice (beadwork::ui::ChoiceDisplay&, const juce::String& caption, juce::Rectangle<int>,
                    const juce::String& tip, std::function<juce::String()> text, std::function<void()> open);
    void addButton (beadwork::ui::PanelButton&, juce::Rectangle<int>, const juce::String& tip,
                    std::function<void (const juce::ModifierKeys&)> click);

    void chooseFromMenu (juce::Component& target, const juce::StringArray& items, int current,
                         std::function<void (int)> chosen);
    void chooseParam (juce::Component& target, const juce::String& id);

    void attachLevelKnob();
    void openPresetMenu();
    void savePresetAs();
    void patternMenu (int slot);
    void sceneMenu (int scene);
    void laneChainMenu (int slot);
    void sceneChainMenu (int slot);
    void diceFigure();

    BeadworkProcessor& proc;
    juce::AudioProcessorValueTreeState& apvts;
    beadwork::ui::PanelLookAndFeel lnf;

    int focus = 0;
    beadwork::ui::Layer currentLayer = beadwork::ui::Layer::beads;
    std::optional<beadwork::Pattern> clipboard;

    // Lanes
    std::vector<std::unique_ptr<beadwork::ui::LaneRow>> rows;
    beadwork::ui::Necklace necklace { *this };

    // Layer bar & pattern tools
    std::array<beadwork::ui::PanelButton, 6> layerButtons;
    beadwork::ui::PanelButton diceBtn { "DICE" }, freezeBtn { "FREEZE" }, clearBtn { "CLEAR" },
                              copyBtn { "COPY" }, pasteBtn { "PASTE" };

    // Focused pattern
    beadwork::ui::ModelKnob stepsKnob, fillsKnob, rotateKnob, velocityKnob, accentKnob, gateKnob, chanceKnob;
    beadwork::ui::Knob levelKnob;
    beadwork::ui::ChoiceDisplay divisionChoice, fitChoice, barsChoice, directionChoice, runChoice, laneChannelChoice;

    // Banks
    beadwork::ui::PadGrid patternPads, scenePads;
    beadwork::ui::ChainStrip laneChain, sceneChain;
    beadwork::ui::PanelButton chainBtn { "CHAIN" };
    beadwork::ui::PanelButton performBtn { "PERFORM" }, playBtn { "PLAY" }, loopBtn { "LOOP" };

    // Feel
    beadwork::ui::Knob swingKnob, swingVelKnob, humaniseKnob, chanceGlobalKnob, velTrimKnob;
    beadwork::ui::ChoiceDisplay swingGridChoice, quantiseChoice, clockChoice, channelChoice;

    // Header
    beadwork::ui::ChoiceDisplay presetDisplay;
    beadwork::ui::PanelButton prevBtn { "<" }, nextBtn { ">" };
    beadwork::ui::PanelButton restartBtn { "RESTART" }, rerollBtn { "REROLL" }, muteAllBtn { "MUTE ALL" };

    // Scatter
    beadwork::ui::PanelButton scatterBtn { "SCATTER" };
    beadwork::ui::Knob scatterTypeKnob, scatterDepthKnob;
    bool scatterWasOn = false;
    juce::uint32 scatterPressMs = 0;

    std::vector<std::unique_ptr<SliderAttachment>> attachments;
    std::unique_ptr<SliderAttachment> levelAttachment;

    std::array<float, beadwork::kNumTracks> flashes {};
    std::array<bool, beadwork::kNumTracks> pinned {};   // a pad was clicked: don't follow the chain
    bool wasRunning = false;
    std::array<int, beadwork::kNumTracks> lastHits {};
    float pulse = 0.0f;
    int   tick = 0;
    juce::String statusText;
    int   statusTicks = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BeadworkPanel)
};

class BeadworkEditor final : public juce::AudioProcessorEditor
{
public:
    explicit BeadworkEditor (BeadworkProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    BeadworkPanel panel;
    juce::TooltipWindow tooltips { this, 700 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BeadworkEditor)
};
