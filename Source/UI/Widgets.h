// Widgets.h — Beadwork panel controls, in the same flat hardware language as
// Dicey and Stepkin: charcoal panel, cream knob caps, outlined sections, flat
// LEDs and amber LCD readouts.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

namespace beadwork::ui
{
// ---------------------------------------------------------------------------
// Palette
// ---------------------------------------------------------------------------
namespace colours
{
    inline const juce::Colour panel      { 0xff2a2a2c };
    inline const juce::Colour panelEdge  { 0xff1c1c1e };
    inline const juce::Colour ink        { 0xfff3f0e6 };
    inline const juce::Colour inkDim     { 0xff9a978e };
    inline const juce::Colour cream      { 0xfff7f1dc };
    inline const juce::Colour creamShade { 0xffd9d1b4 };
    inline const juce::Colour black      { 0xff111112 };
    inline const juce::Colour socket     { 0xff3a3a3d };   // an unlit LED
    inline const juce::Colour brand      { 0xfff0a830 };   // Beadwork amber
    inline const juce::Colour lcd        { 0xffffb238 };   // LCD text
    inline const juce::Colour ledRed     { 0xffff2b3d };
    inline const juce::Colour ledGreen   { 0xff3fe060 };
    inline const juce::Colour ledYellow  { 0xffffd21f };
}

juce::Font panelFont (float height, bool bold = true);
void drawText (juce::Graphics&, const juce::String&, juce::Rectangle<float>, float height,
               juce::Colour = colours::ink, bool bold = true,
               juce::Justification = juce::Justification::centred);

// A flat LED. Unlit is a dark socket; `level` 0..1 brightens a lit one.
void drawLed (juce::Graphics&, juce::Point<float> centre, float radius, juce::Colour colour,
              bool lit, float level = 0.0f);

// An outlined panel section with its name in an inverse tab, Stepkin style.
void drawSection (juce::Graphics&, juce::Rectangle<float>, const juce::String& title);

// Mounting-screw slot, as on Dicey's corners.
void drawScrewSlot (juce::Graphics&, juce::Point<float> centre);

// ---------------------------------------------------------------------------
// Look and feel
// ---------------------------------------------------------------------------
class PanelLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    PanelLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    juce::Font getSliderPopupFont (juce::Slider&) override { return panelFont (15.0f); }
    int getSliderPopupPlacement (juce::Slider&) override { return juce::BubbleComponent::above; }
    juce::Font getPopupMenuFont() override { return panelFont (15.0f, false); }
    juce::Font getAlertWindowMessageFont() override { return panelFont (15.0f, false); }
};

// ---------------------------------------------------------------------------
// Knob: a cream cap with a pointer line inside a scale arc, caption below.
// The value shows in a bubble while you drag.
// ---------------------------------------------------------------------------
class Knob : public juce::Slider
{
public:
    Knob();
    juce::String caption;
    bool bipolar = false;
    std::function<juce::String (double)> format;   // bubble text; default: the value

    void paint (juce::Graphics&) override;
    juce::String getTextFromValue (double v) override { return format ? format (v) : Slider::getTextFromValue (v); }

private:
    double snapValue (double attempted, DragMode) override;
};

// A knob bound to a Project field rather than a host parameter. The editor
// calls sync() from its timer so the knob follows changes made elsewhere.
class ModelKnob final : public Knob
{
public:
    std::function<int()>     get;
    std::function<void (int)> set;

    void sync();

private:
    void valueChanged() override;
    bool syncing = false;
};

// ---------------------------------------------------------------------------
// Rectangular button. Lit = inverse (cream face, dark text).
// ---------------------------------------------------------------------------
class PanelButton final : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit PanelButton (juce::String label = {});

    std::function<void (const juce::ModifierKeys&)> onClick;
    std::function<void()> onPress;     // mouse down
    std::function<void()> onRelease;   // mouse up, wherever the mouse is

    void setLabel (const juce::String& t) { if (t != label) { label = t; repaint(); } }
    void setLit (bool l) { if (l != lit) { lit = l; repaint(); } }
    void setFontHeight (float h) { fontH = h; repaint(); }
    bool isLit() const noexcept { return lit; }

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    juce::String label;
    bool lit = false, pressed = false;
    float fontH = 12.0f;
};

// ---------------------------------------------------------------------------
// LCD readout that opens a menu when clicked.
// ---------------------------------------------------------------------------
class ChoiceDisplay final : public juce::Component, public juce::SettableTooltipClient
{
public:
    std::function<juce::String()> getText;
    std::function<void()> onOpen;
    juce::String caption;
    float fontH = 15.0f;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { if (onOpen) onOpen(); }

    void refresh();   // repaints only when the text changes

private:
    juce::String shown;
};

// Note names the way Ableton prints them: MIDI 60 = C3.
juce::String noteName (int midiNote);
// General MIDI drum name for a note, or empty.
juce::String drumName (int midiNote);
} // namespace beadwork::ui
