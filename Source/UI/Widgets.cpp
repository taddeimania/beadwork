#include "Widgets.h"

namespace beadwork::ui
{
using namespace juce;

// ---------------------------------------------------------------------------
// Type
// ---------------------------------------------------------------------------
Font panelFont (float height, bool bold)
{
   #if JUCE_MAC
    Font f (FontOptions ("Helvetica Neue", height, bold ? Font::bold : Font::plain));
   #else
    Font f (FontOptions (height, bold ? Font::bold : Font::plain));
   #endif
    f.setHorizontalScale (0.86f);
    return f;
}

void drawText (Graphics& g, const String& text, Rectangle<float> r, float height, Colour colour,
               bool bold, Justification j)
{
    g.setColour (colour);
    g.setFont (panelFont (height, bold));
    g.drawText (text, r, j, false);
}

// ---------------------------------------------------------------------------
// Panel furniture
// ---------------------------------------------------------------------------
void drawLed (Graphics& g, Point<float> c, float r, Colour colour, bool lit, float level)
{
    const auto body = Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c);
    if (! lit)
    {
        g.setColour (colours::socket);
        g.fillEllipse (body);
        g.setColour (colours::panelEdge);
        g.drawEllipse (body, 1.0f);
        return;
    }
    g.setColour (level > 0.0f ? colour.brighter (0.45f * level) : colour.darker (0.08f));
    g.fillEllipse (body);
    g.setColour (colour.darker (0.5f));
    g.drawEllipse (body, 1.0f);
}

void drawSection (Graphics& g, Rectangle<float> r, const String& title)
{
    g.setColour (colours::ink.withAlpha (0.85f));
    g.drawRect (r, 1.5f);

    if (title.isEmpty())
        return;
    const auto f = panelFont (12.0f, true);
    const float w = GlyphArrangement::getStringWidth (f, title) + 12.0f;
    const auto tab = Rectangle<float> (r.getX() + 14.0f, r.getY() - 8.0f, w, 16.0f);
    g.setColour (colours::ink);
    g.fillRect (tab);
    g.setColour (colours::panel);
    g.setFont (f);
    g.drawText (title, tab, Justification::centred, false);
}

void drawScrewSlot (Graphics& g, Point<float> c)
{
    const auto r = Rectangle<float> (64.0f, 34.0f).withCentre (c);
    g.setColour (colours::cream);
    g.fillRoundedRectangle (r, 17.0f);
    g.setColour (colours::creamShade);
    g.drawRoundedRectangle (r, 17.0f, 1.2f);
}

// ---------------------------------------------------------------------------
// Look and feel
// ---------------------------------------------------------------------------
PanelLookAndFeel::PanelLookAndFeel()
{
    setColour (PopupMenu::backgroundColourId, colours::panel);
    setColour (PopupMenu::textColourId, colours::ink);
    setColour (PopupMenu::headerTextColourId, colours::brand);
    setColour (PopupMenu::highlightedBackgroundColourId, colours::cream);
    setColour (PopupMenu::highlightedTextColourId, colours::black);
    setColour (BubbleComponent::backgroundColourId, colours::black);
    setColour (BubbleComponent::outlineColourId, colours::ink);
    setColour (TooltipWindow::backgroundColourId, colours::black);
    setColour (TooltipWindow::textColourId, colours::ink);
    setColour (TooltipWindow::outlineColourId, colours::inkDim);
    setColour (AlertWindow::backgroundColourId, colours::panel);
    setColour (AlertWindow::textColourId, colours::ink);
    setColour (AlertWindow::outlineColourId, colours::ink);
    setColour (TextEditor::backgroundColourId, colours::black);
    setColour (TextEditor::textColourId, colours::lcd);
    setColour (TextEditor::outlineColourId, colours::inkDim);
    setColour (TextEditor::focusedOutlineColourId, colours::ink);
    setColour (TextButton::buttonColourId, colours::panel);
    setColour (TextButton::textColourOffId, colours::ink);
    setColour (Slider::textBoxTextColourId, colours::ink);
}

void PanelLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos,
                                         float a0, float a1, Slider& s)
{
    const auto* k = dynamic_cast<Knob*> (&s);
    const bool bip = k != nullptr && k->bipolar;

    const auto bounds = Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const float r = jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f - 4.0f;
    const auto  c = bounds.getCentre();

    // Scale arc.
    Path arc;
    arc.addCentredArc (c.x, c.y, r, r, 0.0f, a0, a1, true);
    g.setColour (colours::ink);
    g.strokePath (arc, PathStrokeType (jmax (1.5f, r * 0.06f), PathStrokeType::curved, PathStrokeType::butt));
    if (bip)
    {
        const float am = (a0 + a1) * 0.5f;
        g.drawLine (Line<float> (c.getPointOnCircumference (r - 1.0f, am), c.getPointOnCircumference (r + 4.0f, am)), 2.0f);
    }

    // Cap: a flat cream heptagon on a shade ring.
    const float cr = r * 0.74f;
    Path cap;
    for (int i = 0; i < 7; ++i)
    {
        const auto p = c.getPointOnCircumference (cr, MathConstants<float>::twoPi * (float) i / 7.0f + 0.2f);
        if (i == 0) cap.startNewSubPath (p); else cap.lineTo (p);
    }
    cap.closeSubPath();
    cap = cap.createPathWithRoundedCorners (cr * 0.18f);
    g.setColour (colours::creamShade);
    g.fillEllipse (Rectangle<float> (cr * 2.12f, cr * 2.12f).withCentre (c));
    g.setColour (colours::cream);
    g.fillPath (cap);
    g.setColour (colours::creamShade.darker (0.25f));
    g.strokePath (cap, PathStrokeType (1.0f));

    // Pointer.
    const float ang = a0 + pos * (a1 - a0);
    g.setColour (colours::black);
    g.drawLine (Line<float> (c.getPointOnCircumference (cr * 0.12f, ang), c.getPointOnCircumference (cr * 0.86f, ang)),
                jmax (2.0f, r * 0.08f));
}

// ---------------------------------------------------------------------------
// Knob
// ---------------------------------------------------------------------------
Knob::Knob()
{
    setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    setTextBoxStyle (Slider::NoTextBox, true, 0, 0);
    setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
    setPopupDisplayEnabled (true, true, nullptr, 900);
    setMouseDragSensitivity (220);
    setScrollWheelEnabled (true);
}

double Knob::snapValue (double attempted, DragMode)
{
    if (bipolar)
    {
        const double mid = (getMinimum() + getMaximum()) * 0.5;
        if (std::abs (attempted - mid) < 0.02 * (getMaximum() - getMinimum()))
            return mid;
    }
    return attempted;
}

void Knob::paint (Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    const float capH = caption.isNotEmpty() ? 18.0f : 0.0f;
    const auto dial = area.withTrimmedBottom (capH);
    const float side = jmin (dial.getWidth(), dial.getHeight());
    const auto sq = Rectangle<float> (side, side).withCentre (dial.getCentre());

    getLookAndFeel().drawRotarySlider (g, (int) sq.getX(), (int) sq.getY(), (int) sq.getWidth(), (int) sq.getHeight(),
                                       (float) valueToProportionOfLength (getValue()),
                                       getRotaryParameters().startAngleRadians,
                                       getRotaryParameters().endAngleRadians, *this);

    if (capH > 0.0f)
        drawText (g, caption, area.removeFromBottom (capH), 13.0f, colours::ink, true);
}

void ModelKnob::sync()
{
    if (! get || isMouseButtonDown())
        return;
    const int v = get();
    if (juce::roundToInt (getValue()) != v)
    {
        syncing = true;
        setValue (v, dontSendNotification);
        syncing = false;
        repaint();
    }
}

void ModelKnob::valueChanged()
{
    if (! syncing && set)
        set (juce::roundToInt (getValue()));
    repaint();
}

// ---------------------------------------------------------------------------
// PanelButton
// ---------------------------------------------------------------------------
PanelButton::PanelButton (String l) : label (std::move (l)) {}

void PanelButton::paint (Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced (1.0f);
    const bool on = lit != pressed;   // pressing a lit button shows it releasing

    g.setColour (on ? colours::cream : colours::black);
    g.fillRect (r);
    g.setColour (colours::ink);
    g.drawRect (r, 1.5f);

    drawText (g, label, r, fontH, on ? colours::black : colours::ink, true);
}

void PanelButton::mouseDown (const MouseEvent&)
{
    pressed = true;
    repaint();
    if (onPress)
        onPress();
}

void PanelButton::mouseUp (const MouseEvent& e)
{
    pressed = false;
    repaint();
    if (onRelease)
        onRelease();
    if (getLocalBounds().contains (e.getPosition()) && onClick)
        onClick (e.mods);
}

// ---------------------------------------------------------------------------
// ChoiceDisplay
// ---------------------------------------------------------------------------
void ChoiceDisplay::refresh()
{
    const auto t = getText ? getText() : String();
    if (t != shown)
    {
        shown = t;
        repaint();
    }
}

void ChoiceDisplay::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    if (caption.isNotEmpty())
        drawText (g, caption, r.removeFromBottom (16.0f), 12.0f, colours::ink, true);

    r = r.reduced (1.0f);
    g.setColour (colours::black);
    g.fillRect (r);
    g.setColour (colours::inkDim);
    g.drawRect (r, 1.0f);

    // Menu arrow.
    const float ax = r.getRight() - 12.0f, ay = r.getCentreY();
    Path tri;
    tri.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
    g.setColour (colours::inkDim);
    g.fillPath (tri);

    shown = getText ? getText() : String();
    drawText (g, shown, r.withTrimmedRight (16.0f).reduced (6.0f, 0.0f), fontH, colours::lcd, true);
}

// ---------------------------------------------------------------------------
// Names
// ---------------------------------------------------------------------------
String noteName (int n)
{
    static const char* names[] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    n = jlimit (0, 127, n);
    return String (names[n % 12]) + String (n / 12 - 2);
}

String drumName (int n)
{
    static const char* gm[] {
        "Kick 2", "Kick", "Side Stick", "Snare", "Clap", "Snare 2", "Low Tom 2", "Closed Hat",
        "Low Tom", "Pedal Hat", "Mid Tom 2", "Open Hat", "Mid Tom", "High Tom 2", "Crash", "High Tom",
        "Ride", "China", "Ride Bell", "Tambourine", "Splash", "Cowbell", "Crash 2", "Vibraslap",
        "Ride 2", "Hi Bongo", "Lo Bongo", "Mute Conga", "Open Conga", "Lo Conga", "Hi Timbale",
        "Lo Timbale", "Hi Agogo", "Lo Agogo", "Cabasa", "Maracas", "Short Whistle", "Long Whistle",
        "Short Guiro", "Long Guiro", "Claves", "Hi Wood Block", "Lo Wood Block", "Mute Cuica",
        "Open Cuica", "Mute Triangle", "Open Triangle"
    };
    if (n < 35 || n > 81)
        return {};
    return gm[n - 35];
}
} // namespace beadwork::ui
