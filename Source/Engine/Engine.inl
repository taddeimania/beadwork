// Engine.inl — the templated part of the sequencer.
//
// Included at the bottom of Engine.h; not a standalone header. Kept separate
// only so Engine.h stays readable as a description of the model.

#pragma once

namespace beadwork
{

// ---------------------------------------------------------------------------
// Close a track's hanging note.
// ---------------------------------------------------------------------------
template <typename Emit>
void Sequencer::closeNote (TrackRt& t, int index, double at, double bps, double p0,
                           int numSamples, Emit& emit)
{
    if (t.activeNote < 0)
        return;

    NoteEvent e;
    e.beat     = at;
    e.track    = index;
    e.channel  = t.activeChan;
    e.note     = t.activeNote;
    e.velocity = 0;
    e.on       = false;

    const long s = (long) std::lround ((at - p0) / bps);
    const int  offset = (int) (s < 0 ? 0 : (s > numSamples - 1 ? numSamples - 1 : s));
    emit (offset, e);

    t.activeNote = -1;
}

// ---------------------------------------------------------------------------
// Walk one track across [from, to).
// ---------------------------------------------------------------------------
template <typename Emit>
void Sequencer::runTrack (int index, const Track& tr, const Globals& g, const Project& p,
                          double from, double to, double bps, double p0, int numSamples, Emit& emit)
{
    TrackRt& t = rt[(size_t) index];

    auto offsetOf = [&] (double beat)
    {
        const long s = (long) std::lround ((beat - p0) / bps);
        return (int) (s < 0 ? 0 : (s > numSamples - 1 ? numSamples - 1 : s));
    };

    // Solo: if anything is soloed, only soloed tracks speak.
    bool anySolo = false;
    for (int i = 0; i < kNumTracks; ++i)
        anySolo = anySolo || g.soloed[(size_t) i];

    const bool gateOpen = ! g.masterMute
                       && ! g.muted[(size_t) index]
                       && (! anySolo || g.soloed[(size_t) index]);

    // A running scene chain can mute the lane; it's checked at each hit's own
    // beat so a scene change lands on its bar line, not at the next block.
    auto sceneMutedAt = [&] (double beat)
    {
        if (p.sceneMode == kPerform)
            return false;
        const int sc = sceneAt (p, beat, nullptr);
        return sc >= 0 && ((p.scenes[(size_t) sc].muted >> index) & 1u) != 0;
    };

    double cursor = from;
    int    guard  = 0;

    while (cursor < to - 1.0e-12 && guard++ < 512)
    {
        // A pattern change may be waiting on a quantise boundary.
        updateScene (p, cursor);
        const int wanted = patternForPass (tr, g, index, t);
        if (wanted != t.pattern)
        {
            const Pattern& cur   = tr.patterns[(size_t) t.pattern];
            const double   at    = boundary (cursor, g.quantise, t, cur.stepBeats());

            if (at <= cursor + 1.0e-12)
            {
                // Land it now. FREE keeps the playhead's phase in the loop;
                // RELATCH and ONCE start the new figure from step one.
                t.pattern = wanted;
                t.silent  = false;
                if (tr.runMode == kFree)
                {
                    // Keep the phase, wrapped into the new loop's length.
                    const double L    = tr.patterns[(size_t) wanted].loopBeats();
                    const double keep = t.loopStart + std::floor ((cursor - t.loopStart) / L + 1.0e-9) * L;
                    beginPass (tr, g, index, t, keep);
                    while (t.hitCursor < t.hitCount && t.hits[(size_t) t.hitCursor].beat < cursor)
                        ++t.hitCursor;
                }
                else
                {
                    closeNote (t, index, cursor, bps, p0, numSamples, emit);
                    beginPass (tr, g, index, t, cursor);
                }
            }
            else if (at < to)
            {
                // Run up to the boundary, then come round again and land it.
                runTrack (index, tr, g, p, cursor, at, bps, p0, numSamples, emit);
                cursor = at;
                continue;
            }
        }

        const double passEnd = t.loopStart + t.loopBeats;
        const double segEnd  = to < passEnd ? to : passEnd;

        // Sound one hit (a normal one or a scattered one).
        auto play = [&] (const Hit& h)
        {
            // A note-off that is due before this hit starts.
            if (t.activeNote >= 0 && t.offAt <= h.beat)
                closeNote (t, index, t.offAt, bps, p0, numSamples, emit);

            if (t.silent || ! gateOpen || h.velocity == 0 || sceneMutedAt (h.beat))
                return;

            // One note at a time per lane: retrigger closes the last hit.
            if (t.activeNote >= 0)
                closeNote (t, index, h.beat, bps, p0, numSamples, emit);

            NoteEvent e;
            e.beat     = h.beat;
            e.track    = index;
            e.channel  = tr.channel == 0 ? g.channel : tr.channel;
            e.note     = tr.note;
            e.velocity = h.velocity;
            e.on       = true;
            emit (offsetOf (h.beat), e);

            t.activeNote = e.note;
            t.activeChan = e.channel;
            t.offAt      = h.offBeat;
        };

        // --- scatter: lay out every 1/16 cell that starts in this window -----
        // Engaging waits for the next cell boundary, so a throw is always on
        // the grid; letting go finishes the cell that's playing.
        if (g.scatter)
        {
            const double gridNext = anchor + std::ceil ((cursor - anchor) / kScatterCell - 1.0e-9) * kScatterCell;
            double c = std::max (t.scatterNext, gridNext);
            if (c > t.scatterUntil + 1.0e-9)
                t.scatterFrom = c;                 // a new run starts here
            while (c < segEnd - 1.0e-12)
            {
                scatterCell (g, t, c);
                c += kScatterCell;
                t.scatterUntil = c;
            }
            t.scatterNext = c;
        }

        // Scattered hits come first: they all fall before scatterUntil, and
        // every normal hit that still sounds falls after it.
        while (t.pendHead < t.pendCount && t.pending[(size_t) t.pendHead].beat < segEnd)
            play (t.pending[(size_t) t.pendHead++]);
        if (t.pendHead >= t.pendCount)
            t.pendHead = t.pendCount = 0;

        // --- the hits of this pass that fall in the window ------------------
        while (t.hitCursor < t.hitCount)
        {
            const Hit& h = t.hits[(size_t) t.hitCursor];
            if (h.beat >= segEnd)
                break;

            // Inside a scattered run the pattern keeps walking, silently.
            if (! (h.beat >= t.scatterFrom && h.beat < t.scatterUntil))
                play (h);

            t.uiStep = h.step;
            ++t.hitCursor;
        }

        // A note-off with nothing after it in this window.
        if (t.activeNote >= 0 && t.offAt < segEnd)
            closeNote (t, index, t.offAt, bps, p0, numSamples, emit);

        cursor = segEnd;

        // --- end of the pass: advance the chain -----------------------------
        if (cursor >= passEnd - 1.0e-12)
        {
            ++t.pass;

            if (tr.runMode == kOneShot)
                t.silent = true;

            if (tr.chainOn)
            {
                const int len = tr.chainLen < 1 ? 1 : (tr.chainLen > kMaxChain ? kMaxChain : tr.chainLen);
                const int reps = tr.chainRepeats[(size_t) (t.chainSlot % len)];
                if (++t.repeatsDone >= (reps < 1 ? 1 : reps))
                {
                    t.repeatsDone = 0;
                    t.chainSlot   = (t.chainSlot + 1) % len;
                }
            }

            if (tr.patterns[(size_t) t.pattern].direction == kPendulum)
                t.dir = -t.dir;

            // A chain is its own quantisation. Otherwise the loop's end only
            // counts as a landing point if the quantise grid agrees, so a
            // "4 BAR" change on a one-bar loop still waits for its bar line.
            updateScene (p, passEnd);
            const int next = patternForPass (tr, g, index, t);
            if (next != t.pattern
                && (tr.chainOn || boundary (passEnd, g.quantise, t, tr.patterns[(size_t) t.pattern].stepBeats())
                                      <= passEnd + 1.0e-9))
            {
                t.pattern = next;
                t.silent  = false;
            }
            beginPass (tr, g, index, t, passEnd);
        }
    }
}

// ---------------------------------------------------------------------------
// Process one host block.
// ---------------------------------------------------------------------------
template <typename Emit>
void Sequencer::process (const BlockInfo& b, const Globals& g, const Project& p, Emit&& emit)
{
    if (b.numSamples <= 0)
        return;

    const double rate = g.rate > 0.0 ? g.rate : 1.0;
    const double bps  = b.bpm * rate / (60.0 * b.sampleRate); // beats per sample

    // --- stopped -----------------------------------------------------------
    if (! b.playing || b.bpm <= 0.0 || b.sampleRate <= 0.0)
    {
        if (wasPlaying)
            for (int i = 0; i < kNumTracks; ++i)
            {
                TrackRt& t = rt[(size_t) i];
                if (t.activeNote >= 0)
                {
                    NoteEvent e;
                    e.track = i; e.channel = t.activeChan; e.note = t.activeNote;
                    e.velocity = 0; e.on = false;
                    emit (0, e);
                    t.activeNote = -1;
                }
                t.uiStep = -1;
            }
        wasPlaying = false;
        return;
    }

    const double p0 = b.ppq * rate;
    const double p1 = p0 + b.numSamples * bps;

    const bool jumped = wasPlaying && std::abs (p0 - expectedPpq) > std::max (1.0e-3, 4.0 * bps);

    if (! wasPlaying || jumped)
    {
        for (int i = 0; i < kNumTracks; ++i)
        {
            TrackRt& t = rt[(size_t) i];
            if (t.activeNote >= 0)
            {
                NoteEvent e;
                e.track = i; e.channel = t.activeChan; e.note = t.activeNote;
                e.velocity = 0; e.on = false;
                emit (0, e);
                t.activeNote = -1;
            }
        }

        // Chains are measured from the song's start, so a pattern chain lines
        // up with the arrangement whichever bar you drop the playhead on.
        anchor    = 0.0;
        restartAt = kNone;
        restartRequested = false;
        seekAll (g, p, p0);
    }

    wasPlaying  = true;
    expectedPpq = p1;

    if (restartRequested)
    {
        restartAt        = std::ceil (b.ppq - 1.0e-9) * rate;
        restartRequested = false;
    }

    auto segment = [&] (double a, double z)
    {
        if (z <= a)
            return;
        for (int i = 0; i < kNumTracks; ++i)
            runTrack (i, p.tracks[(size_t) i], g, p, a, z, bps, p0, b.numSamples, emit);
        updateScene (p, z); // leave the editor's view at the end of the block
    };

    if (std::isfinite (restartAt) && restartAt >= p0 && restartAt < p1)
    {
        segment (p0, restartAt);
        for (int i = 0; i < kNumTracks; ++i)
        {
            TrackRt& t = rt[(size_t) i];
            closeNote (t, i, restartAt, bps, p0, b.numSamples, emit);
        }
        anchor    = restartAt;
        restartAt = kNone;
        seekAll (g, p, anchor);
        segment (anchor, p1);
    }
    else
    {
        segment (p0, p1);
    }
}

} // namespace beadwork
