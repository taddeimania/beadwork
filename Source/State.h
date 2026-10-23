// State.h — the Project as the editor sees it, how it reaches the audio
// thread, and how it is written into a session or preset file.
#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include <array>
#include <atomic>

#include "Engine/Engine.h"

namespace beadwork
{
// ---------------------------------------------------------------------------
// ProjectStore
//
// The editor owns one editable Project. Edits are coalesced (markDirty) and
// published at most ~30 times a second into a three-slot buffer the audio
// thread reads without locking:
//
//   * publish() writes into a slot that is neither the published one nor the
//     one the audio thread last said it was reading, then flips `published`.
//   * The audio thread calls beginRead() at the top of a block, which records
//     the slot it is about to use, and reads that slot for the whole block.
//
// With three slots there is always a free one, and with publishing throttled
// to the editor's timer the audio thread never sees a slot being written.
// ---------------------------------------------------------------------------
class ProjectStore
{
public:
    ProjectStore();

    // --- message thread ------------------------------------------------------
    Project&       edit() noexcept { return editable; }
    const Project& view() const noexcept { return editable; }

    void markDirty() noexcept { dirty = true; ++editCount; }
    bool publishIfDirty();          // returns true if it published
    void publishNow();              // unconditional, e.g. after loading state

    // A counter that moves on every edit; the editor uses it to decide whether
    // to repaint and the preset manager to flag "edited".
    uint32_t editVersion() const noexcept { return editCount.load(); }

    // --- audio thread -------------------------------------------------------
    const Project& beginRead() noexcept
    {
        const int p = published.load (std::memory_order_acquire);
        reading.store (p, std::memory_order_release);
        return slots[(size_t) p];
    }

private:
    Project editable;
    std::array<Project, 3> slots;
    std::atomic<int> published { 0 };
    std::atomic<int> reading { 0 };
    std::atomic<bool> dirty { false };
    std::atomic<uint32_t> editCount { 0 };
};

// ---------------------------------------------------------------------------
// Serialisation
// ---------------------------------------------------------------------------
// The project is stored as readable XML under one <Project> child: lanes with
// their sixteen patterns, the scenes, and the scene chain. Per-step layers are
// comma-separated lists so a saved session diffs sensibly.
juce::ValueTree projectToTree (const Project&);
void projectFromTree (const juce::ValueTree&, Project&);

inline const juce::Identifier projectTag { "Project" };
} // namespace beadwork
