#include "Bank.h"

namespace beadwork::ui
{
using namespace juce;

// ---------------------------------------------------------------------------
// PadGrid
// ---------------------------------------------------------------------------
Rectangle<float> PadGrid::padBounds (int i) const
{
    const float w = (float) getWidth() / 8.0f;
    const float h = (float) getHeight() / 2.0f;
    return Rectangle<float> (w * (float) (i % 8), h * (float) (i / 8), w, h);
}

int PadGrid::padAt (Point<float> p) const
{
    for (int i = 0; i < numPads; ++i)
        if (padBounds (i).contains (p))
            return i;
    return -1;
}

void PadGrid::paint (Graphics& g)
{
    // A table of cells, like Dicey's options grid: the selected cell is filled,
    // what's playing carries a lit LED.
    for (int i = 0; i < numPads; ++i)
    {
        const auto st = getState ? getState (i) : PadState {};
        const auto r  = padBounds (i);

        if (st.selected)
        {
            g.setColour (colour);
            g.fillRect (r);
        }

        const auto text = label ? label (i) : String (i + 1);
        Colour ink = st.selected ? colours::black : (st.used && ! st.dim ? colours::ink : colours::inkDim);
        drawText (g, text, r, 15.0f, ink, true);

        if (st.playing)
            drawLed (g, { r.getRight() - 9.0f, r.getY() + 9.0f }, 4.0f, colours::ledGreen, pulse > 0.3f, 0.0f);
    }

    g.setColour (colours::ink);
    const auto b = getLocalBounds().toFloat().reduced (0.75f);
    g.drawRect (b, 1.5f);
    for (int c = 1; c < 8; ++c)
    {
        const float x = b.getX() + b.getWidth() * (float) c / 8.0f;
        g.drawLine (x, b.getY(), x, b.getBottom(), 1.0f);
    }
    g.drawLine (b.getX(), b.getCentreY(), b.getRight(), b.getCentreY(), 1.0f);
}

void PadGrid::mouseDown (const MouseEvent& e)
{
    const int i = padAt (e.position);
    if (i < 0)
        return;
    if (e.mods.isPopupMenu())
    {
        if (onMenu) onMenu (i);
        return;
    }
    if (onClick)
        onClick (i, e.mods);
    repaint();
}

// ---------------------------------------------------------------------------
// ChainStrip
// ---------------------------------------------------------------------------
float ChainStrip::slotWidth() const
{
    const float avail = (float) getWidth() - 64.0f;
    const int   len   = getLength ? jmax (1, getLength()) : 1;
    return jlimit (26.0f, 58.0f, avail / (float) len);
}

Rectangle<float> ChainStrip::slotBounds (int i) const
{
    const float w = slotWidth();
    return Rectangle<float> (w * (float) i, 0.0f, w, (float) getHeight()).reduced (2.0f, 1.0f);
}

Rectangle<float> ChainStrip::plusBounds() const
{
    return { (float) getWidth() - 60.0f, 2.0f, 28.0f, (float) getHeight() - 4.0f };
}

Rectangle<float> ChainStrip::minusBounds() const
{
    return { (float) getWidth() - 30.0f, 2.0f, 28.0f, (float) getHeight() - 4.0f };
}

int ChainStrip::slotAt (Point<float> p) const
{
    const int len = getLength ? getLength() : 0;
    for (int i = 0; i < len; ++i)
        if (slotBounds (i).contains (p))
            return i;
    return -1;
}

void ChainStrip::paint (Graphics& g)
{
    const int len = getLength ? getLength() : 0;
    const int cur = currentSlot ? currentSlot() : -1;
    const float alpha = enabled ? 1.0f : 0.4f;

    for (int i = 0; i < len; ++i)
    {
        auto r = slotBounds (i);
        const bool isCur = enabled && i == cur;
        if (isCur)
        {
            g.setColour (colours::cream);
            g.fillRect (r);
        }
        g.setColour (colours::ink.withAlpha (alpha));
        g.drawRect (r, 1.2f);

        auto top = r.removeFromTop (r.getHeight() * 0.56f);
        drawText (g, itemText ? itemText (i) : String(), top, 15.0f,
                  (isCur ? colours::black : colours::ink).withAlpha (alpha), true);
        drawText (g, countText ? countText (i) : String(), r, 12.0f,
                  (isCur ? colours::black : colours::inkDim).withAlpha (alpha), true);
    }

    auto square = [&] (Rectangle<float> r, const String& t, bool active)
    {
        g.setColour (colours::black);
        g.fillRect (r);
        g.setColour (colours::ink.withAlpha (active ? 1.0f : 0.35f));
        g.drawRect (r, 1.2f);
        drawText (g, t, r, 16.0f, colours::ink.withAlpha (active ? 1.0f : 0.35f), true);
    };
    square (plusBounds(),  "+", len < maxLength);
    square (minusBounds(), String::fromUTF8 ("\xe2\x88\x92"), len > 1);
}

void ChainStrip::mouseDown (const MouseEvent& e)
{
    const int len = getLength ? getLength() : 0;
    if (plusBounds().contains (e.position))
    {
        if (len < maxLength && setLength) setLength (len + 1);
        repaint();
        return;
    }
    if (minusBounds().contains (e.position))
    {
        if (len > 1 && setLength) setLength (len - 1);
        repaint();
        return;
    }

    dragSlot = slotAt (e.position);
    if (dragSlot < 0)
        return;
    if (e.mods.isPopupMenu())
    {
        if (onMenu) onMenu (dragSlot);
        dragSlot = -1;
        return;
    }
    const auto r = slotBounds (dragSlot);
    dragCount   = e.position.y > r.getY() + r.getHeight() * 0.56f;
    dragStartY  = e.position.y;
    dragApplied = 0;
    dragMoved   = false;
}

void ChainStrip::mouseDrag (const MouseEvent& e)
{
    if (dragSlot < 0)
        return;
    const float dy = dragStartY - e.position.y;
    if (! dragMoved && std::abs (dy) < 3.0f)
        return;
    dragMoved = true;
    const int want = roundToInt (dy / 12.0f);
    if (want != dragApplied)
    {
        auto& f = dragCount ? nudgeCount : nudgeItem;
        if (f) f (dragSlot, want - dragApplied);
        dragApplied = want;
        repaint();
    }
}

void ChainStrip::mouseUp (const MouseEvent&)
{
    if (dragSlot >= 0 && ! dragMoved && onMenu)
        onMenu (dragSlot);
    dragSlot = -1;
}

void ChainStrip::mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& w)
{
    const int s = slotAt (e.position);
    if (s < 0)
        return;
    wheelAccum += (w.isReversed ? -w.deltaY : w.deltaY) * 6.0f;
    const int d = (int) wheelAccum;
    if (d == 0)
        return;
    wheelAccum -= (float) d;
    const auto r = slotBounds (s);
    const bool count = e.mods.isShiftDown() || e.position.y > r.getY() + r.getHeight() * 0.56f;
    auto& f = count ? nudgeCount : nudgeItem;
    if (f) f (s, d);
    repaint();
}
} // namespace beadwork::ui
