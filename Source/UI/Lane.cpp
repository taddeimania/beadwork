#include "Lane.h"

namespace beadwork::ui
{
using namespace juce;

// ---------------------------------------------------------------------------
// Step editing
// ---------------------------------------------------------------------------
bool beadLit (const Pattern& p, int s)
{
    return s >= 0 && s < p.steps && ((p.mask() >> s) & 1u) != 0;
}

void setBead (Pattern& p, int s, bool on)
{
    if (s < 0 || s >= kMaxSteps)
        return;
    const uint32_t b = 1u << s;
    const bool fromFigure = ((euclid (p.steps, p.fills, p.rotate) >> s) & 1u) != 0;

    // Keep the layers minimal: only record the difference from the figure.
    if (on)
    {
        p.forceOff &= ~b;
        if (fromFigure) p.forceOn &= ~b; else p.forceOn |= b;
    }
    else
    {
        p.forceOn &= ~b;
        if (fromFigure) p.forceOff |= b; else p.forceOff &= ~b;
    }
}

int layerValue (const Pattern& p, int s, Layer l)
{
    s = jlimit (0, kMaxSteps - 1, s);
    switch (l)
    {
        case Layer::ratchet:  return jmax (1, (int) p.ratchet[(size_t) s]);
        case Layer::chance:   return p.stepChance[(size_t) s] == 0 ? 100 : p.stepChance[(size_t) s];
        case Layer::nudge:    return p.nudge[(size_t) s];
        case Layer::velocity: return p.stepVel[(size_t) s] == 0 ? p.velocity : p.stepVel[(size_t) s];
        case Layer::accent:   return (p.accents >> s) & 1u ? 1 : 0;
        case Layer::beads:    return beadLit (p, s) ? 1 : 0;
    }
    return 0;
}

void setLayerValue (Pattern& p, int s, Layer l, int v)
{
    if (s < 0 || s >= kMaxSteps)
        return;
    switch (l)
    {
        case Layer::ratchet:  p.ratchet[(size_t) s] = (uint8_t) jlimit (1, 8, v); break;
        case Layer::chance:   { const int c = jlimit (1, 100, v); p.stepChance[(size_t) s] = (uint8_t) (c == 100 ? 0 : c); break; }
        case Layer::nudge:    p.nudge[(size_t) s] = (int8_t) jlimit (-50, 50, v); break;
        case Layer::velocity: { const int x = jlimit (1, 127, v); p.stepVel[(size_t) s] = (uint8_t) (x == p.velocity ? 0 : x); break; }
        case Layer::accent:   if (v) p.accents |= 1u << s; else p.accents &= ~(1u << s); break;
        case Layer::beads:    setBead (p, s, v != 0); break;
    }
}

void clickLayer (Pattern& p, int s, Layer l, const ModifierKeys& mods)
{
    const bool reset = mods.isAltDown() || mods.isCommandDown();
    switch (l)
    {
        case Layer::accent:   setLayerValue (p, s, l, layerValue (p, s, l) ? 0 : 1); break;
        case Layer::ratchet:
        {
            static constexpr int cycle[] { 1, 2, 3, 4, 6, 8 };
            int next = 1;
            const int cur = layerValue (p, s, l);
            for (int i = 0; i < 6; ++i)
                if (cycle[i] == cur) { next = cycle[(i + 1) % 6]; break; }
            setLayerValue (p, s, l, reset ? 1 : next);
            break;
        }
        case Layer::chance:   setLayerValue (p, s, l, reset || layerValue (p, s, l) < 100 ? 100 : 50); break;
        case Layer::nudge:    setLayerValue (p, s, l, 0); break;
        case Layer::velocity: p.stepVel[(size_t) jlimit (0, kMaxSteps - 1, s)] = 0; break;
        case Layer::beads:    break;
    }
}

float layerPixelsPerUnit (Layer l)
{
    switch (l)
    {
        case Layer::ratchet:  return 14.0f;
        case Layer::chance:   return 1.2f;
        case Layer::nudge:    return 1.6f;
        case Layer::velocity: return 1.0f;
        case Layer::accent:
        case Layer::beads:    break;
    }
    return 10.0f;
}

void drawLayerOverlay (Graphics& g, Point<float> c, float r, const Pattern& p, int s, Layer l, Colour colour)
{
    const auto ink = colours::ink;
    switch (l)
    {
        case Layer::ratchet:
        {
            const int n = layerValue (p, s, l);
            if (n <= 1) return;
            // n small beads in an arc above the main one.
            for (int i = 0; i < n; ++i)
            {
                const float a = MathConstants<float>::pi * (-0.85f + 0.7f * (n == 1 ? 0.5f : (float) i / (float) (n - 1)));
                const auto  q = c.getPointOnCircumference (r * 1.9f, a + MathConstants<float>::halfPi);
                g.setColour (ink);
                g.fillEllipse (Rectangle<float> (r * 0.42f, r * 0.42f).withCentre (q));
            }
            break;
        }
        case Layer::chance:
        {
            const int ch = layerValue (p, s, l);
            if (ch >= 100) return;
            Path arc;
            arc.addCentredArc (c.x, c.y, r * 1.6f, r * 1.6f, 0.0f, 0.0f,
                               MathConstants<float>::twoPi * (float) ch / 100.0f, true);
            g.setColour (ink);
            g.strokePath (arc, PathStrokeType (jmax (1.5f, r * 0.28f), PathStrokeType::curved, PathStrokeType::rounded));
            g.setColour (colours::socket);
            Path rest;
            rest.addCentredArc (c.x, c.y, r * 1.6f, r * 1.6f, 0.0f, MathConstants<float>::twoPi * (float) ch / 100.0f,
                                MathConstants<float>::twoPi, true);
            g.strokePath (rest, PathStrokeType (jmax (1.0f, r * 0.14f)));
            break;
        }
        case Layer::nudge:
        {
            const int n = layerValue (p, s, l);
            if (n == 0) return;
            const float dx = r * 1.7f * (float) n / 50.0f;
            g.setColour (ink);
            g.drawLine (c.x, c.y + r * 1.6f, c.x + dx, c.y + r * 1.6f, jmax (1.5f, r * 0.25f));
            g.fillEllipse (Rectangle<float> (r * 0.4f, r * 0.4f).withCentre ({ c.x + dx, c.y + r * 1.6f }));
            break;
        }
        case Layer::velocity:
        {
            const int v = layerValue (p, s, l);
            const float h = r * 2.6f * (float) v / 127.0f;
            const auto bar = Rectangle<float> (c.x + r * 1.35f, c.y + r * 1.3f - h, jmax (2.0f, r * 0.32f), h);
            g.setColour (p.stepVel[(size_t) jlimit (0, kMaxSteps - 1, s)] == 0 ? colour.withAlpha (0.6f) : ink);
            g.fillRect (bar);
            break;
        }
        case Layer::accent:
        case Layer::beads:
            break;
    }
}

// ---------------------------------------------------------------------------
// Shared gesture handling
// ---------------------------------------------------------------------------
namespace
{
void gestureDown (BeadGesture& gs, Pattern& p, int step, Layer l, const MouseEvent& e)
{
    gs = {};
    gs.step   = step;
    gs.startY = e.position.y;
    if (step < 0)
        return;

    if (l == Layer::beads)
    {
        gs.paintOn = ! beadLit (p, step);
        setBead (p, step, gs.paintOn);
        gs.painted[(size_t) step] = true;
    }
    else if (l == Layer::accent)
    {
        gs.paintOn = layerValue (p, step, l) == 0;
        setLayerValue (p, step, l, gs.paintOn ? 1 : 0);
        gs.painted[(size_t) step] = true;
    }
    else
    {
        gs.startValue = layerValue (p, step, l);
    }
}

// Returns true if anything changed.
bool gestureDrag (BeadGesture& gs, Pattern& p, int stepUnderMouse, Layer l, const MouseEvent& e)
{
    if (gs.step < 0)
        return false;

    if (l == Layer::beads || l == Layer::accent)
    {
        // Paint the same state across every bead the mouse passes over.
        if (stepUnderMouse >= 0 && stepUnderMouse < p.steps && ! gs.painted[(size_t) stepUnderMouse])
        {
            gs.painted[(size_t) stepUnderMouse] = true;
            setLayerValue (p, stepUnderMouse, l, gs.paintOn ? 1 : 0);
            return true;
        }
        return false;
    }

    const float dy = gs.startY - e.position.y;
    if (! gs.moved && std::abs (dy) < 3.0f)
        return false;
    gs.moved = true;
    const int v = gs.startValue + roundToInt (dy / layerPixelsPerUnit (l));
    const int before = layerValue (p, gs.step, l);
    setLayerValue (p, gs.step, l, v);
    return layerValue (p, gs.step, l) != before;
}

void gestureUp (BeadGesture& gs, Pattern& p, Layer l, const MouseEvent& e)
{
    if (gs.step >= 0 && ! gs.moved && l != Layer::beads && l != Layer::accent)
        clickLayer (p, gs.step, l, e.mods);
    gs.step = -1;
}

String laneName (LaneHost& h, int lane)
{
    const auto& n = h.project().tracks[(size_t) lane].name;
    const auto name = String::fromUTF8 (n.data(), (int) strnlen (n.data(), kNameChars));
    return name.isNotEmpty() ? name : "Lane " + String (lane + 1);   // the Empty kit's lanes have no names
}
} // namespace

// ---------------------------------------------------------------------------
// LaneRow
// ---------------------------------------------------------------------------
LaneRow::LaneRow (LaneHost& h, int i) : host (h), lane (i) {}

Pattern& LaneRow::pattern()
{
    return host.project().tracks[(size_t) lane].patterns[(size_t) host.editPattern (lane)];
}

float LaneRow::spacing() const
{
    const float avail = (float) getWidth() - beadsX - 14.0f;
    auto& self = const_cast<LaneRow&> (*this);
    const int n = jmax (1, (int) self.pattern().steps);
    return jmin (34.0f, avail / (float) n);
}

Point<float> LaneRow::beadCentre (int s) const
{
    const float sp = spacing();
    return { beadsX + sp * ((float) s + 0.5f), (float) getHeight() * 0.5f };
}

int LaneRow::stepAt (Point<float> p) const
{
    if (p.x < beadsX)
        return -1;
    auto& self = const_cast<LaneRow&> (*this);
    const int s = (int) std::floor ((p.x - beadsX) / spacing());
    return s >= 0 && s < self.pattern().steps ? s : -1;
}

void LaneRow::paint (Graphics& g)
{
    const auto  b      = getLocalBounds().toFloat();
    const bool  focus  = host.focusedLane() == lane;
    const auto& tr     = host.project().tracks[(size_t) lane];
    const Pattern& p   = pattern();
    const bool  muted  = host.isMuted (lane);
    const float flash  = host.flash (lane);

    // Row rule.
    g.setColour (colours::panelEdge);
    g.drawHorizontalLine ((int) b.getBottom() - 1, 6.0f, b.getRight() - 6.0f);

    // Gate LED: blinks on every hit the lane sends.
    drawLed (g, { 14.0f, b.getCentreY() }, 5.0f, colours::ledRed, flash > 0.05f, flash);

    // Name; the focused lane is shown inverse, like a selected table cell.
    auto name = nameArea().withTrimmedLeft (10.0f).reduced (0.0f, 10.0f);
    if (focus)
    {
        g.setColour (colours::cream);
        g.fillRect (name);
    }
    drawText (g, laneName (host, lane).toUpperCase(), name.withTrimmedLeft (6.0f), 14.0f,
              focus ? colours::black : (muted ? colours::inkDim : colours::ink), true, Justification::centredLeft);

    // LCD chips and the square M / S buttons.
    auto lcdChip = [&] (Rectangle<float> r, const String& t)
    {
        g.setColour (colours::black);
        g.fillRect (r);
        g.setColour (colours::inkDim);
        g.drawRect (r, 1.0f);
        drawText (g, t, r, 13.0f, colours::lcd, true);
    };
    auto squareButton = [&] (Rectangle<float> r, const String& t, bool lit)
    {
        g.setColour (lit ? colours::cream : colours::black);
        g.fillRect (r);
        g.setColour (colours::ink);
        g.drawRect (r, 1.2f);
        drawText (g, t, r, 12.0f, lit ? colours::black : colours::ink, true);
    };
    lcdChip (noteArea(), noteName (tr.note));
    squareButton (muteArea(), "M", muted);
    squareButton (soloArea(), "S", host.isSoloed (lane));
    lcdChip (patArea(), tr.chainOn ? "CH" + String (tr.chainLen)
                                   : "P" + String (host.editPattern (lane) + 1).paddedLeft ('0', 2));

    // Thread and beat ticks.
    const int n = jmax (1, (int) p.steps);
    const auto c0 = beadCentre (0), c1 = beadCentre (n - 1);
    g.setColour (colours::socket);
    g.drawLine (c0.x, c0.y, c1.x, c1.y, 1.0f);

    const float sp = spacing();
    for (int s = 0; s < n; s += 4)
    {
        const float x = beadsX + sp * ((float) s + 0.5f);
        g.setColour (colours::inkDim);
        g.drawLine (x, b.getBottom() - 7.0f, x, b.getBottom() - 3.0f, 1.0f);
    }

    const float r = jlimit (3.5f, 8.0f, sp * 0.27f);
    const int playing = host.playingShown (lane) ? host.playingStep (lane) : -1;
    const Layer l = host.layer();

    for (int s = 0; s < n; ++s)
    {
        const auto c   = beadCentre (s);
        const bool lit = beadLit (p, s);
        const bool acc = ((p.accents >> s) & 1u) != 0;
        const auto led = muted ? colours::inkDim : (acc ? colours::ledYellow : colours::ledRed);
        drawLed (g, c, r, led, lit, s == playing ? 0.5f + 0.5f * flash : 0.0f);
        if (s == playing)
        {
            g.setColour (colours::ink);
            g.drawEllipse (Rectangle<float> (r * 2.0f + 7.0f, r * 2.0f + 7.0f).withCentre (c), 1.5f);
        }
        if (l != Layer::beads && l != Layer::accent && (lit || l == Layer::chance))
            drawLayerOverlay (g, c, r, p, s, l, colours::ledRed);
    }
}

void LaneRow::mouseDown (const MouseEvent& e)
{
    const auto pos = e.position;
    host.focusLane (lane);

    if (muteArea().contains (pos)) { host.toggleMute (lane); return; }
    if (soloArea().contains (pos)) { host.toggleSolo (lane); return; }
    if (noteArea().contains (pos)) { host.openNoteMenu (lane, *this); return; }

    const int s = stepAt (pos);
    gestureDown (gesture, pattern(), s, host.layer(), e);
    if (s >= 0)
        host.edited();
    repaint();
}

void LaneRow::mouseDrag (const MouseEvent& e)
{
    if (gestureDrag (gesture, pattern(), stepAt (e.position), host.layer(), e))
    {
        host.edited();
        repaint();
    }
}

void LaneRow::mouseUp (const MouseEvent& e)
{
    const bool had = gesture.step >= 0;
    gestureUp (gesture, pattern(), host.layer(), e);
    if (had)
    {
        host.edited();
        repaint();
    }
}

void LaneRow::mouseDoubleClick (const MouseEvent& e)
{
    if (nameArea().contains (e.position))
        host.renameLane (lane);
}

void LaneRow::mouseMove (const MouseEvent& e)
{
    const auto pos = e.position;
    String tip;
    if (nameArea().contains (pos))      tip = "Double-click to rename this lane.";
    else if (noteArea().contains (pos)) tip = "The MIDI note this lane plays. Click to choose.";
    else if (muteArea().contains (pos)) tip = "Mute: the lane keeps its place, it just stops speaking.";
    else if (soloArea().contains (pos)) tip = "Solo.";
    else if (patArea().contains (pos))  tip = "The pattern on screen for this lane (a chain glyph when it is chaining).";
    else if (stepAt (pos) >= 0)
    {
        switch (host.layer())
        {
            case Layer::beads:    tip = "Click or drag across beads to add or remove them."; break;
            case Layer::accent:   tip = "Click or drag to accent beads."; break;
            case Layer::ratchet:  tip = "Drag up/down for 1-8 hits in the step; click to cycle."; break;
            case Layer::chance:   tip = "Drag up/down to set this step's chance; click toggles 50/100%."; break;
            case Layer::nudge:    tip = "Drag up/down to push the hit early or late; click to centre."; break;
            case Layer::velocity: tip = "Drag up/down for this step's velocity; click to follow the pattern."; break;
        }
    }
    if (tip != getTooltip())
        setTooltip (tip);
}

// ---------------------------------------------------------------------------
// Necklace
// ---------------------------------------------------------------------------
Pattern& Necklace::pattern()
{
    const int lane = host.focusedLane();
    return host.project().tracks[(size_t) lane].patterns[(size_t) host.editPattern (lane)];
}

float Necklace::beadRadius (int steps) const
{
    const float R = ringRadius();
    return jlimit (4.0f, 12.0f, MathConstants<float>::pi * R / (float) jmax (1, steps) * 0.5f);
}

Point<float> Necklace::beadCentre (int s, int steps) const
{
    // Step 1 at twelve o'clock, clockwise.
    const float a = MathConstants<float>::twoPi * (float) s / (float) jmax (1, steps);
    return centre().getPointOnCircumference (ringRadius(), a);
}

int Necklace::stepAt (Point<float> p) const
{
    auto& self = const_cast<Necklace&> (*this);
    const int n = jmax (1, (int) self.pattern().steps);
    const float br = beadRadius (n);
    for (int s = 0; s < n; ++s)
        if (p.getDistanceFrom (beadCentre (s, n)) <= br * 1.5f)
            return s;
    return -1;
}

void Necklace::paint (Graphics& g)
{
    const int lane     = host.focusedLane();
    const Pattern& p   = pattern();
    const int n        = jmax (1, (int) p.steps);
    const auto c       = centre();
    const float R      = ringRadius();
    const float br     = beadRadius (n);
    const float flash  = host.flash (lane);
    const int playing  = host.playingShown (lane) ? host.playingStep (lane) : -1;
    const Layer l      = host.layer();

    // Hub: a flat black disc.
    const float hub = R * 0.6f;
    const auto hubR = Rectangle<float> (hub * 2, hub * 2).withCentre (c);
    g.setColour (colours::black);
    g.fillEllipse (hubR);
    g.setColour (colours::ink.withAlpha (0.8f));
    g.drawEllipse (hubR, 1.5f);

    // Thread.
    g.setColour (colours::socket);
    g.drawEllipse (Rectangle<float> (R * 2, R * 2).withCentre (c), 1.0f);

    // LEDs and step numbers, Dicey-ring style.
    for (int s = 0; s < n; ++s)
    {
        const auto bc  = beadCentre (s, n);
        const bool lit = beadLit (p, s);
        const bool acc = ((p.accents >> s) & 1u) != 0;
        drawLed (g, bc, br, acc ? colours::ledYellow : colours::ledRed, lit, s == playing ? 0.5f + 0.5f * flash : 0.0f);
        if (s == playing)
        {
            g.setColour (colours::ink);
            g.drawEllipse (Rectangle<float> (br * 2.0f + 8.0f, br * 2.0f + 8.0f).withCentre (bc), 1.8f);
        }
        if (l != Layer::beads && l != Layer::accent && (lit || l == Layer::chance))
            drawLayerOverlay (g, bc, br, p, s, l, colours::ledRed);

        if (n <= 16 || s % 4 == 0)
        {
            const auto t = c.getPointOnCircumference (R + br + 13.0f, MathConstants<float>::twoPi * (float) s / (float) n);
            const uint32_t bit = 1u << s;
            const bool edited = ((p.forceOn | p.forceOff) & bit) != 0;
            drawText (g, String (s + 1), Rectangle<float> (26.0f, 16.0f).withCentre (t), 12.0f,
                      edited ? colours::brand : colours::ink, true);
        }
    }

    // Hub readout.
    drawText (g, laneName (host, lane).toUpperCase(), hubR.withSizeKeepingCentre (hub * 2, 16.0f).translated (0, -40),
              12.0f, colours::inkDim, true);
    drawText (g, String (p.fills) + " / " + String (p.steps), hubR.withSizeKeepingCentre (hub * 2, 40.0f).translated (0, -6),
              34.0f, colours::ink, true);
    String sub = "E(" + String (p.fills) + "," + String (p.steps) + ")";
    if (p.rotate != 0)
        sub << "  " << (p.rotate > 0 ? "+" : "") << String (p.rotate);
    drawText (g, sub, hubR.withSizeKeepingCentre (hub * 2, 18.0f).translated (0, 24), 14.0f, colours::lcd, true);
}

void Necklace::mouseDown (const MouseEvent& e)
{
    const auto pos = e.position;
    const int s = stepAt (pos);

    if (s < 0 && inHub (pos))
    {
        twisting = true;
        twistStartAngle  = centre().getAngleToPoint (pos);
        twistStartRotate = pattern().rotate;
        return;
    }

    gestureDown (gesture, pattern(), s, host.layer(), e);
    if (s >= 0)
    {
        host.edited();
        repaint();
    }
}

void Necklace::mouseDrag (const MouseEvent& e)
{
    if (twisting)
    {
        Pattern& p = pattern();
        const int n = jmax (1, (int) p.steps);
        float d = centre().getAngleToPoint (e.position) - twistStartAngle;
        while (d >  MathConstants<float>::pi) d -= MathConstants<float>::twoPi;
        while (d < -MathConstants<float>::pi) d += MathConstants<float>::twoPi;
        // One step of rotation per bead's worth of angle; wrap into the length.
        int r = twistStartRotate + roundToInt (d / MathConstants<float>::twoPi * (float) n);
        r = ((r % n) + n) % n;
        if (r != p.rotate)
        {
            p.rotate = (int8_t) r;
            host.edited();
            repaint();
        }
        return;
    }

    if (gestureDrag (gesture, pattern(), stepAt (e.position), host.layer(), e))
    {
        host.edited();
        repaint();
    }
}

void Necklace::mouseUp (const MouseEvent& e)
{
    if (twisting)
    {
        twisting = false;
        // Keep the twist anchor sensible for the next grab.
        return;
    }
    const bool had = gesture.step >= 0;
    gestureUp (gesture, pattern(), host.layer(), e);
    if (had)
    {
        host.edited();
        repaint();
    }
}

void Necklace::mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& w)
{
    if (! inHub (e.position))
        return;

    wheelAccum += (w.isReversed ? -w.deltaY : w.deltaY) * 8.0f;
    const int d = (int) wheelAccum;
    if (d == 0)
        return;
    wheelAccum -= (float) d;

    Pattern& p = pattern();
    if (e.mods.isShiftDown())
    {
        p.steps = (uint8_t) jlimit (1, kMaxSteps, (int) p.steps + d);
        p.fills = (uint8_t) jmin ((int) p.fills, (int) p.steps);
    }
    else
    {
        p.fills = (uint8_t) jlimit (0, (int) p.steps, (int) p.fills + d);
    }
    host.edited();
    repaint();
}
} // namespace beadwork::ui
