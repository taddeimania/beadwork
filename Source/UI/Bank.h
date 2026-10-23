// Bank.h — sixteen-pad grids (patterns, scenes) and the chain strip that
// strings pads together.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "UI/Widgets.h"

namespace beadwork::ui
{
// ---------------------------------------------------------------------------
// PadGrid: sixteen pads in two rows of eight.
// ---------------------------------------------------------------------------
class PadGrid final : public juce::Component, public juce::SettableTooltipClient
{
public:
    struct PadState
    {
        bool selected = false;  // the one on screen / last recalled
        bool playing  = false;  // what the engine is playing now
        bool used     = true;   // has content (scenes) / differs from blank
        bool dim      = false;
    };

    static constexpr int numPads = 16;

    std::function<PadState (int)> getState;
    std::function<void (int, const juce::ModifierKeys&)> onClick;
    std::function<void (int)> onMenu;               // right-click / ctrl-click
    std::function<juce::String (int)> label;        // default: 1..16
    juce::Colour colour = colours::brand;

    void setPulse (float p) { if (std::abs (p - pulse) > 0.02f) { pulse = p; repaint(); } }

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> padBounds (int i) const;
    int padAt (juce::Point<float>) const;
    float pulse = 0.0f;
};

// ---------------------------------------------------------------------------
// ChainStrip: an editable list of (item, count) slots.
//   drag a slot's upper half   change the item (pattern / scene)
//   drag its lower half        change the count (repeats / bars)
//   click a slot               menu: pick item, insert, delete
//   + / -                      lengthen / shorten the chain
// ---------------------------------------------------------------------------
class ChainStrip final : public juce::Component, public juce::SettableTooltipClient
{
public:
    std::function<int()> getLength;
    int maxLength = 16;
    std::function<juce::String (int)> itemText;      // "P3"
    std::function<juce::String (int)> countText;     // "x2"
    std::function<int()> currentSlot;                // -1 when not running
    std::function<void (int slot, int delta)> nudgeItem;
    std::function<void (int slot, int delta)> nudgeCount;
    std::function<void (int newLength)> setLength;
    std::function<void (int slot)> onMenu;
    bool enabled = true;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    juce::Rectangle<float> slotBounds (int i) const;

private:
    float slotWidth() const;
    juce::Rectangle<float> plusBounds() const;
    juce::Rectangle<float> minusBounds() const;
    int slotAt (juce::Point<float>) const;

    int   dragSlot = -1;
    bool  dragCount = false;
    float dragStartY = 0.0f;
    int   dragApplied = 0;
    bool  dragMoved = false;
    float wheelAccum = 0.0f;
};
} // namespace beadwork::ui
