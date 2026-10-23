// Lane.h — the two views of a lane: a row of beads in the lane list, and the
// big necklace for the lane in focus. Both edit the same per-step layers.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Engine/Engine.h"
#include "UI/Widgets.h"

namespace beadwork::ui
{
// What a click or drag on a bead edits.
enum class Layer { beads, accent, ratchet, chance, nudge, velocity };

inline const char* layerName (Layer l)
{
    switch (l)
    {
        case Layer::beads:    return "BEADS";
        case Layer::accent:   return "ACCENT";
        case Layer::ratchet:  return "RATCHET";
        case Layer::chance:   return "CHANCE";
        case Layer::nudge:    return "NUDGE";
        case Layer::velocity: return "VELO";
    }
    return "";
}

// ---------------------------------------------------------------------------
// The editor implements this; the views ask it for everything.
// ---------------------------------------------------------------------------
class LaneHost
{
public:
    virtual ~LaneHost() = default;

    virtual Project& project() = 0;          // the editable project
    virtual void edited() = 0;               // something in project() changed
    virtual int  editPattern (int lane) = 0; // which of the lane's patterns is on screen
    virtual int  playingStep (int lane) = 0; // -1 when stopped
    virtual bool playingShown (int lane) = 0;// the playing pattern is the one on screen
    virtual float flash (int lane) = 0;      // 0..1, decays after each hit
    virtual Layer layer() = 0;

    virtual int  focusedLane() = 0;
    virtual void focusLane (int lane) = 0;
    virtual bool isMuted (int lane) = 0;
    virtual bool isSoloed (int lane) = 0;
    virtual void toggleMute (int lane) = 0;
    virtual void toggleSolo (int lane) = 0;
    virtual void openNoteMenu (int lane, juce::Component& target) = 0;
    virtual void renameLane (int lane) = 0;
};

// ---------------------------------------------------------------------------
// Step editing, shared by both views
// ---------------------------------------------------------------------------
bool beadLit (const Pattern&, int step);
void setBead (Pattern&, int step, bool on);
int  layerValue (const Pattern&, int step, Layer);     // the value a drag starts from
void setLayerValue (Pattern&, int step, Layer, int v); // clamps to the layer's range
void clickLayer (Pattern&, int step, Layer, const juce::ModifierKeys&);
float layerPixelsPerUnit (Layer);

// Draws the value of a non-bead layer around a bead.
void drawLayerOverlay (juce::Graphics&, juce::Point<float> c, float r, const Pattern&, int step,
                       Layer, juce::Colour);

// ---------------------------------------------------------------------------
// Bead-drag state machine both views use.
// ---------------------------------------------------------------------------
struct BeadGesture
{
    int   step = -1;
    bool  paintOn = false;   // BEADS: the state being painted across beads
    int   startValue = 0;    // other layers: value under the mouse at mouse-down
    float startY = 0.0f;
    bool  moved = false;
    std::array<bool, kMaxSteps> painted {};
};

// ---------------------------------------------------------------------------
// One row of the lane list.
// ---------------------------------------------------------------------------
class LaneRow final : public juce::Component, public juce::SettableTooltipClient
{
public:
    LaneRow (LaneHost& h, int laneIndex);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;

    static constexpr float beadsX = 300.0f;

private:
    juce::Rectangle<float> nameArea() const   { return { 16.0f, 0.0f, 104.0f, (float) getHeight() }; }
    juce::Rectangle<float> noteArea() const   { return { 122.0f, 9.0f, 58.0f, (float) getHeight() - 18.0f }; }
    juce::Rectangle<float> muteArea() const   { return { 186.0f, 9.0f, 28.0f, (float) getHeight() - 18.0f }; }
    juce::Rectangle<float> soloArea() const   { return { 218.0f, 9.0f, 28.0f, (float) getHeight() - 18.0f }; }
    juce::Rectangle<float> patArea() const    { return { 252.0f, 9.0f, 40.0f, (float) getHeight() - 18.0f }; }

    float spacing() const;
    juce::Point<float> beadCentre (int step) const;
    int  stepAt (juce::Point<float>) const;

    Pattern& pattern();

    LaneHost& host;
    int lane;
    BeadGesture gesture;
};

// ---------------------------------------------------------------------------
// The necklace: the focused lane's pattern as a ring of beads.
//   click / drag a bead   edit the current layer
//   drag the centre       twist the necklace (ROTATE)
//   wheel on the centre   FILLS; with shift, STEPS
// ---------------------------------------------------------------------------
class Necklace final : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit Necklace (LaneHost& h) : host (h) {}

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    juce::Point<float> centre() const { return getLocalBounds().toFloat().getCentre(); }
    float ringRadius() const { return juce::jmin (getWidth(), getHeight()) * 0.5f - 38.0f; }
    float beadRadius (int steps) const;
    juce::Point<float> beadCentre (int step, int steps) const;
    int  stepAt (juce::Point<float>) const;
    bool inHub (juce::Point<float> p) const { return p.getDistanceFrom (centre()) < ringRadius() * 0.62f; }

    Pattern& pattern();

    LaneHost& host;
    BeadGesture gesture;

    bool  twisting = false;
    float twistStartAngle = 0.0f;
    int   twistStartRotate = 0;
    float wheelAccum = 0.0f;
};
} // namespace beadwork::ui
