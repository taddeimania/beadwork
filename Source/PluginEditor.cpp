#include "PluginEditor.h"

using namespace beadwork;
using namespace beadwork::ui;
using juce::Rectangle;
using juce::String;

namespace
{
// Layout (design space).
constexpr int kLanesX = 20, kLanesY = 92, kRowH = 50, kLanesW = 876;
constexpr int kLayerY = kLanesY + kNumTracks * kRowH + 6;   // 698
constexpr int kBankY  = 748;
constexpr int kRightX = 916;

// Section boxes.
const juce::Rectangle<float> kLanesBox     { 12.0f,  80.0f, 892.0f, 656.0f };
const juce::Rectangle<float> kNecklaceBox  { 912.0f, 80.0f, 576.0f, 452.0f };
const juce::Rectangle<float> kFeelBox      { 912.0f, 544.0f, 576.0f, 192.0f };
const juce::Rectangle<float> kPatternsBox  { 12.0f,  748.0f, 440.0f, 180.0f };
const juce::Rectangle<float> kScenesBox    { 462.0f, 748.0f, 442.0f, 180.0f };
const juce::Rectangle<float> kTransportBox { 912.0f, 748.0f, 576.0f, 180.0f };
const juce::Rectangle<float> kLcd          { 932.0f, 770.0f, 240.0f, 66.0f };
constexpr float kScatterX = 1286.0f;  // the SCATTER cluster sits right of this line

String laneNameOf (const Track& t)
{
    return String::fromUTF8 (t.name.data(), (int) strnlen (t.name.data(), kNameChars));
}
} // namespace

// ===========================================================================
BeadworkPanel::BeadworkPanel (BeadworkProcessor& p)
    : proc (p), apvts (p.apvts)
{
    setLookAndFeel (&lnf);
    setSize (designW, designH);

    // --- lanes -----------------------------------------------------------------
    for (int i = 0; i < kNumTracks; ++i)
    {
        rows.push_back (std::make_unique<LaneRow> (*this, i));
        addAndMakeVisible (*rows.back());
        rows.back()->setBounds (kLanesX, kLanesY + i * kRowH, kLanesW, kRowH);
    }

    // --- layer bar ---------------------------------------------------------------
    const Layer layers[] { Layer::beads, Layer::accent, Layer::ratchet, Layer::chance, Layer::nudge, Layer::velocity };
    const char* layerTips[] {
        "Click or drag to add and remove beads. Edits sit on top of the Euclidean figure.",
        "Click or drag to accent beads (velocity + the pattern's ACCENT).",
        "Drag a bead up/down for 1-8 hits inside its step. Click to cycle.",
        "Drag a bead up/down to set the chance it plays.",
        "Drag a bead up/down to push it early or late by up to half a step.",
        "Drag a bead up/down to set its own velocity."
    };
    for (int i = 0; i < 6; ++i)
    {
        auto& b = layerButtons[(size_t) i];
        b.setLabel (layerName (layers[i]));
        const Layer l = layers[i];
        addButton (b, { kLanesX + 296 + i * 82, kLayerY, 78, 30 }, layerTips[i],
                   [this, l] (const juce::ModifierKeys&) { currentLayer = l; repaint(); });
    }

    addButton (diceBtn,   { kLanesX + 0,   kLayerY, 62, 30 },
               "Roll a new figure for this pattern: random FILLS and ROTATE, your edits kept.",
               [this] (const juce::ModifierKeys&) { diceFigure(); });
    addButton (freezeBtn, { kLanesX + 68,  kLayerY, 72, 30 },
               "Freeze: turn the current figure into hand-placed beads, so FILLS and ROTATE start from scratch.",
               [this] (const juce::ModifierKeys&)
               {
                   auto& pt = editing();
                   const uint32_t m = pt.mask();
                   pt.fills = 0; pt.rotate = 0; pt.forceOff = 0; pt.forceOn = m;
                   edited();
               });
    addButton (clearBtn,  { kLanesX + 146, kLayerY, 64, 30 },
               "Clear every per-step edit (beads, accents, ratchets, chance, nudge, velocity) and keep the figure.",
               [this] (const juce::ModifierKeys&)
               {
                   auto& pt = editing();
                   pt.forceOn = pt.forceOff = pt.accents = 0;
                   pt.ratchet.fill (0); pt.stepChance.fill (0); pt.stepVel.fill (0); pt.nudge.fill (0);
                   edited();
               });
    addButton (copyBtn,   { kLanesX + 790, kLayerY, 42, 30 }, "Copy this pattern.",
               [this] (const juce::ModifierKeys&) { clipboard = editing(); statusText = "Copied"; statusTicks = 45; });
    addButton (pasteBtn,  { kLanesX + 836, kLayerY, 40, 30 }, "Paste over this pattern.",
               [this] (const juce::ModifierKeys&)
               {
                   if (clipboard) { editing() = *clipboard; edited(); statusText = "Pasted"; statusTicks = 45; }
               });
    copyBtn.setFontHeight (10.5f);
    pasteBtn.setFontHeight (10.5f);

    // --- necklace and pattern knobs -------------------------------------------
    addAndMakeVisible (necklace);
    necklace.setBounds (1020, 88, 360, 360);
    necklace.setTooltip ("The focused lane's pattern. Drag the centre to twist it (ROTATE); scroll the centre for "
                         "FILLS, shift-scroll for STEPS. Click beads to edit them.");

    const int lkx = kRightX + 8, rkx = 1390;
    addModelKnob (stepsKnob, "STEPS", 1, kMaxSteps, { lkx, 92, 88, 84 },
                  "How many slots on the necklace (1-32).",
                  [this] { return (int) editing().steps; },
                  [this] (int v) { auto& pt = editing(); pt.steps = (uint8_t) v; pt.fills = (uint8_t) juce::jmin ((int) pt.fills, v); edited(); });
    addModelKnob (fillsKnob, "FILLS", 0, kMaxSteps, { lkx, 180, 88, 84 },
                  "How many beads, spread as evenly as possible over the steps.",
                  [this] { return (int) editing().fills; },
                  [this] (int v) { auto& pt = editing(); pt.fills = (uint8_t) juce::jlimit (0, (int) pt.steps, v); edited(); });
    addModelKnob (rotateKnob, "ROTATE", -(kMaxSteps - 1), kMaxSteps - 1, { lkx, 268, 88, 84 },
                  "Turn the necklace. Positive moves every bead later.",
                  [this] { return (int) editing().rotate; },
                  [this] (int v) { editing().rotate = (int8_t) v; edited(); });
    rotateKnob.bipolar = true;
    rotateKnob.format = [] (double v) { const int i = juce::roundToInt (v); return (i > 0 ? "+" : "") + String (i); };

    levelKnob.caption = "LEVEL";
    levelKnob.format = [] (double v) { return String (juce::roundToInt (v)) + "%"; };
    levelKnob.setTooltip ("This lane's level: scales every velocity it sends. Automatable.");
    levelKnob.setBounds (lkx, 356, 88, 84);
    addAndMakeVisible (levelKnob);
    attachLevelKnob();

    addModelKnob (velocityKnob, "VELOCITY", 1, 127, { rkx, 92, 88, 84 },
                  "The velocity of an ordinary bead.",
                  [this] { return (int) editing().velocity; },
                  [this] (int v) { editing().velocity = (uint8_t) v; edited(); });
    addModelKnob (accentKnob, "ACCENT", 0, 127, { rkx, 180, 88, 84 },
                  "How much louder an accented bead plays.",
                  [this] { return (int) editing().accent; },
                  [this] (int v) { editing().accent = (uint8_t) v; edited(); });
    accentKnob.format = [] (double v) { return "+" + String (juce::roundToInt (v)); };
    addModelKnob (gateKnob, "GATE", 1, 200, { rkx, 268, 88, 84 },
                  "Note length as a share of the step. Most drum racks ignore it; samplers in gate mode don't.",
                  [this] { return (int) editing().gate; },
                  [this] (int v) { editing().gate = (uint8_t) v; edited(); });
    gateKnob.format = [] (double v) { return String (juce::roundToInt (v)) + "%"; };
    addModelKnob (chanceKnob, "CHANCE", 0, 100, { rkx, 356, 88, 84 },
                  "The chance each bead of this pattern plays. Per-step chance multiplies with it.",
                  [this] { return (int) editing().chance; },
                  [this] (int v) { editing().chance = (uint8_t) v; edited(); });
    chanceKnob.format = [] (double v) { return String (juce::roundToInt (v)) + "%"; };

    // Choices under the necklace.
    const int cy = 470, cw = 88, cs = 94;
    addChoice (divisionChoice, "STEP", { kRightX + 4, cy, cw, 50 },
               "Step length. Different lengths on different lanes drift against each other (polymeter).",
               [this] { return String (kDivisionNames[(size_t) editing().division]); },
               [this]
               {
                   juce::StringArray items;
                   for (auto* n : kDivisionNames) items.add (n);
                   chooseFromMenu (divisionChoice, items, editing().division,
                                   [this] (int i) { editing().division = (uint8_t) i; edited(); });
               });
    addChoice (fitChoice, "FIT", { kRightX + 4 + cs, cy, cw, 50 },
               "METER: steps are a fixed note value and loops drift. RHYTHM: the steps are squeezed into BARS, "
               "so every lane comes round together (polyrhythm).",
               [this] { return String (editing().fit == kPolyrhythm ? "RHYTHM" : "METER"); },
               [this]
               {
                   chooseFromMenu (fitChoice, { "METER (polymeter)", "RHYTHM (polyrhythm)" }, editing().fit,
                                   [this] (int i) { editing().fit = (uint8_t) i; edited(); });
               });
    addChoice (barsChoice, "BARS", { kRightX + 4 + cs * 2, cy, cw, 50 },
               "In RHYTHM fit, how many bars the steps are squeezed into.",
               [this]
               {
                   // Only meaningful when the steps are squeezed into bars.
                   return editing().fit == kPolyrhythm ? String (editing().fitBars) + (editing().fitBars == 1 ? " BAR" : " BARS")
                                                        : String::fromUTF8 ("\xe2\x80\x94");
               },
               [this]
               {
                   juce::StringArray items;
                   for (int i = 1; i <= 16; ++i) items.add (String (i) + (i == 1 ? " bar" : " bars"));
                   chooseFromMenu (barsChoice, items, editing().fitBars - 1,
                                   [this] (int i) { editing().fitBars = (uint8_t) (i + 1); edited(); });
               });
    addChoice (directionChoice, "DIRECTION", { kRightX + 4 + cs * 3, cy, cw, 50 },
               "Which way the playhead walks the necklace. RAND picks a step at random each slot.",
               [this] { return String (kDirectionNames[(size_t) editing().direction]); },
               [this]
               {
                   chooseFromMenu (directionChoice, { "Forward", "Reverse", "Pendulum", "Random" }, editing().direction,
                                   [this] (int i) { editing().direction = (uint8_t) i; edited(); });
               });
    addChoice (runChoice, "ON CHANGE", { kRightX + 4 + cs * 4, cy, cw, 50 },
               "What the lane does when its pattern changes. FREE keeps its place, RELATCH restarts the loop, "
               "ONCE plays the new pattern once then waits.",
               [this] { return String (kRunModeNames[(size_t) focusedTrack().runMode]); },
               [this]
               {
                   chooseFromMenu (runChoice, { "FREE", "RELATCH", "ONCE" }, focusedTrack().runMode,
                                   [this] (int i) { focusedTrack().runMode = (uint8_t) i; edited(); });
               });
    addChoice (laneChannelChoice, "LANE CH", { kRightX + 4 + cs * 5, cy, cw, 50 },
               "MIDI channel for this lane. GLOBAL follows the channel below.",
               [this] { const int c = focusedTrack().channel; return c == 0 ? String ("GLOBAL") : String (c); },
               [this]
               {
                   juce::StringArray items { "Global" };
                   for (int i = 1; i <= 16; ++i) items.add ("Channel " + String (i));
                   chooseFromMenu (laneChannelChoice, items, focusedTrack().channel,
                                   [this] (int i) { focusedTrack().channel = (uint8_t) i; edited(); });
               });

    // --- feel ---------------------------------------------------------------------
    const int fy = 560, fw = 100, fs = 112;
    addParamKnob (swingKnob, ids::swing, "SWING", { kRightX + 10, fy, fw, 92 },
                  "Delays every second slot of the swing grid. 50% is straight.");
    addParamKnob (swingVelKnob, ids::swingVel, "SWING ACC", { kRightX + 10 + fs, fy, fw, 92 },
                  "Below the middle, the first of each swung pair is accented; above, the second.");
    swingVelKnob.bipolar = true;
    swingVelKnob.format = [] (double v)
    {
        const int i = juce::roundToInt (v);
        return i == 50 ? String ("OFF") : (i < 50 ? "1st " : "2nd ") + String (std::abs (i - 50) * 2);
    };
    addParamKnob (humaniseKnob, ids::humanise, "HUMANISE", { kRightX + 10 + fs * 2, fy, fw, 92 },
                  "Random timing spread. The same dice every loop until you REROLL.");
    addParamKnob (chanceGlobalKnob, ids::chance, "CHANCE", { kRightX + 10 + fs * 3, fy, fw, 92 },
                  "Scales the chance of every bead on every lane. Ride it for fills and breakdowns.");
    addParamKnob (velTrimKnob, ids::velTrim, "VELOCITY", { kRightX + 10 + fs * 4, fy, fw, 92 },
                  "Scales every velocity Beadwork sends.");
    for (auto* k : { &swingKnob, &humaniseKnob, &chanceGlobalKnob, &velTrimKnob })
        k->format = [] (double v) { return String (juce::roundToInt (v)) + "%"; };

    const int gy = 676, gw = 128, gs = 140;
    addChoice (swingGridChoice, "SWING GRID", { kRightX + 10, gy, gw, 50 }, "The note value the swing works on.",
               [this] { return ids::swingGridNames()[juce::roundToInt (proc.getParam (ids::swingGrid))]; },
               [this] { chooseParam (swingGridChoice, ids::swingGrid); });
    addChoice (quantiseChoice, "PATTERN CHANGE", { kRightX + 10 + gs, gy, gw, 50 },
               "When a newly picked pattern takes over: at once, next step, beat, bar(s), or end of the loop.",
               [this] { return String (kQuantiseNames[(size_t) juce::roundToInt (proc.getParam (ids::quantise))]); },
               [this] { chooseParam (quantiseChoice, ids::quantise); });
    addChoice (clockChoice, "CLOCK", { kRightX + 10 + gs * 2, gy, gw, 50 }, "Run faster or slower than Live.",
               [this] { return ids::clockRateNames()[juce::roundToInt (proc.getParam (ids::clockRate))]; },
               [this] { chooseParam (clockChoice, ids::clockRate); });
    addChoice (channelChoice, "MIDI CH", { kRightX + 10 + gs * 3, gy, gw, 50 },
               "The MIDI channel lanes send on unless they have their own.",
               [this] { return String (juce::roundToInt (proc.getParam (ids::channel))); },
               [this] { chooseParam (channelChoice, ids::channel); });

    // --- pattern bank -----------------------------------------------------------------
    patternPads.setBounds (24, kBankY + 30, 416, 80);
    patternPads.setTooltip ("This lane's sixteen patterns. Click to play and edit one; shift-click to add it to "
                            "the lane's chain; right-click to copy, paste or clear, for this lane or all lanes at once.");
    patternPads.getState = [this] (int i)
    {
        PadGrid::PadState s;
        s.selected = editPattern (focus) == i;
        s.playing  = proc.isRunning() && proc.playingPattern (focus) == i;
        const auto& pats = focusedTrack().patterns;
        s.used = ! pats[(size_t) i].sameAs (pats[0]) || i == 0;
        return s;
    };
    patternPads.onClick = [this] (int i, const juce::ModifierKeys& m)
    {
        if (m.isShiftDown())
        {
            auto& t = focusedTrack();
            if (! t.chainOn) { t.chainOn = 1; t.chainLen = 1; t.chainPattern[0] = (uint8_t) i; t.chainRepeats[0] = 1; }
            else if (t.chainLen < kMaxChain)
            {
                t.chainPattern[t.chainLen] = (uint8_t) i;
                t.chainRepeats[t.chainLen] = 1;
                ++t.chainLen;
            }
            edited();
            return;
        }
        proc.setParam (ids::pattern (focus), (float) (i + 1));
        pinned[(size_t) focus] = true;   // stay on the pad you clicked, even while a chain plays
    };
    patternPads.onMenu = [this] (int i) { patternMenu (i); };
    addAndMakeVisible (patternPads);

    addButton (chainBtn, { 370, kBankY + 6, 70, 20 },
               "Chain this lane's patterns: each slot plays its pattern N times, then moves on.",
               [this] (const juce::ModifierKeys&)
               {
                   auto& t = focusedTrack();
                   t.chainOn = t.chainOn ? 0 : 1;
                   pinned[(size_t) focus] = false;   // turning a chain on means "show me the chain"
                   edited();
               });
    chainBtn.setFontHeight (11.0f);

    laneChain.setBounds (24, kBankY + 118, 416, 54);
    laneChain.setTooltip ("The lane's chain. + adds the highlighted pattern to the end, - removes the last slot. "
                          "Drag a slot's top half for the pattern, its bottom half for how many times it repeats. "
                          "Click a slot for more.");
    laneChain.maxLength  = kMaxChain;
    laneChain.getLength  = [this] { return juce::jlimit (1, kMaxChain, (int) focusedTrack().chainLen); };
    laneChain.itemText   = [this] (int s) { return "P" + String (focusedTrack().chainPattern[(size_t) s] + 1); };
    laneChain.countText  = [this] (int s) { return String::fromUTF8 ("\xc3\x97") + String (focusedTrack().chainRepeats[(size_t) s]); };
    laneChain.currentSlot = [this] { return focusedTrack().chainOn && proc.isRunning() ? proc.chainSlot (focus) : -1; };
    laneChain.nudgeItem  = [this] (int s, int d)
    {
        auto& v = focusedTrack().chainPattern[(size_t) s];
        v = (uint8_t) (((int) v + d % kPatternsPerTrack + kPatternsPerTrack) % kPatternsPerTrack);
        edited();
    };
    laneChain.nudgeCount = [this] (int s, int d)
    {
        auto& v = focusedTrack().chainRepeats[(size_t) s];
        v = (uint8_t) juce::jlimit (1, 16, (int) v + d);
        edited();
    };
    laneChain.setLength = [this] (int n)
    {
        // + adds the pattern that's highlighted (the amber pad); - drops the last slot.
        auto& t = focusedTrack();
        const auto highlighted = (uint8_t) editPattern (focus);
        for (int i = t.chainLen; i < n; ++i) { t.chainPattern[(size_t) i] = highlighted; t.chainRepeats[(size_t) i] = 1; }
        t.chainLen = (uint8_t) n;
        edited();
    };
    laneChain.onMenu = [this] (int s) { laneChainMenu (s); };
    addAndMakeVisible (laneChain);

    // --- scenes -------------------------------------------------------------------------
    const int sx = 474;
    scenePads.setBounds (sx, kBankY + 30, 418, 80);
    scenePads.colour = colours::brand;   // same amber as the pattern pads
    scenePads.setTooltip ("Scenes: every lane's pattern and mute in one pad. Click an empty pad to store, a full "
                          "one to recall. Shift-click overwrites. Right-click for more.");
    scenePads.label = [this] (int i)
    {
        const auto& sc = project().scenes[(size_t) i];
        const auto n = String::fromUTF8 (sc.name.data(), (int) strnlen (sc.name.data(), kNameChars));
        return n.isNotEmpty() ? n : String (i + 1);
    };
    scenePads.getState = [this] (int i)
    {
        // Lit the same way as the pattern pads: amber for the scene you picked
        // (last recalled or stored), and a green light once that's what the
        // kit is actually playing. While the scene chain runs, both follow it.
        PadGrid::PadState s;
        s.used = project().scenes[(size_t) i].used != 0;
        s.dim  = ! s.used;
        if (proc.sceneChainRunning())
        {
            s.selected = proc.scene() == i;
            s.playing  = s.selected;
        }
        else
        {
            s.selected = proc.selectedScene() == i && s.used;
            s.playing  = s.selected && proc.isRunning() && scenePlaying (i);
        }
        return s;
    };
    scenePads.onClick = [this] (int i, const juce::ModifierKeys& m)
    {
        const bool used = project().scenes[(size_t) i].used != 0;
        if (m.isShiftDown() || ! used)
        {
            proc.storeScene (i);
            statusText = "Stored scene " + String (i + 1);
        }
        else
        {
            proc.recallScene (i);
            statusText = "Scene " + String (i + 1);
        }
        statusTicks = 45;
    };
    scenePads.onMenu = [this] (int i) { sceneMenu (i); };
    addAndMakeVisible (scenePads);

    auto sceneModeButton = [this] (PanelButton& b, int x, uint8_t mode, const String& tip)
    {
        addButton (b, { x, kBankY + 6, 70, 20 }, tip,
                   [this, mode] (const juce::ModifierKeys&) { project().sceneMode = mode; edited(); });
        b.setFontHeight (11.0f);
    };
    sceneModeButton (performBtn, sx + 200, kPerform, "Scenes change only when you click them.");
    sceneModeButton (playBtn,    sx + 274, kPlayOnce, "Play the scene chain once from the song start, then hold the last scene.");
    sceneModeButton (loopBtn,    sx + 348, kPlayLoop, "Loop the scene chain for as long as Live plays.");

    sceneChain.setBounds (sx, kBankY + 118, 418, 54);
    sceneChain.setTooltip ("The scene chain, measured from the start of the song. Drag a slot's top half for the "
                           "scene, its bottom half for its length in bars.");
    sceneChain.maxLength  = kMaxSceneChain;
    sceneChain.getLength  = [this] { return juce::jlimit (1, kMaxSceneChain, (int) project().sceneChainLen); };
    sceneChain.itemText   = [this] (int s) { return "S" + String (project().sceneChainScene[(size_t) s] + 1); };
    sceneChain.countText  = [this] (int s) { const int b = project().sceneChainBars[(size_t) s]; return String (b) + (b == 1 ? " bar" : " bars"); };
    sceneChain.currentSlot = [this] { return proc.sceneChainRunning() ? proc.sceneSlot() : -1; };
    sceneChain.nudgeItem  = [this] (int s, int d)
    {
        auto& v = project().sceneChainScene[(size_t) s];
        v = (uint8_t) (((int) v + d % kNumScenes + kNumScenes) % kNumScenes);
        edited();
    };
    sceneChain.nudgeCount = [this] (int s, int d)
    {
        auto& v = project().sceneChainBars[(size_t) s];
        v = (uint8_t) juce::jlimit (1, 64, (int) v + d);
        edited();
    };
    sceneChain.setLength = [this] (int n)
    {
        auto& pr = project();
        for (int i = pr.sceneChainLen; i < n; ++i) { pr.sceneChainScene[(size_t) i] = (uint8_t) juce::jmin (kNumScenes - 1, i); pr.sceneChainBars[(size_t) i] = 4; }
        pr.sceneChainLen = (uint8_t) n;
        edited();
    };
    sceneChain.onMenu = [this] (int s) { sceneChainMenu (s); };
    addAndMakeVisible (sceneChain);

    // --- header -------------------------------------------------------------------------
    addAndMakeVisible (presetDisplay);
    presetDisplay.setBounds (1046, 20, 300, 34);
    presetDisplay.getText = [this] { return proc.presets.currentName(); };
    presetDisplay.onOpen  = [this] { openPresetMenu(); };
    presetDisplay.setTooltip ("Kits: factory starting points and your own saves. Each one holds all twelve lanes, "
                              "every pattern, the chains and the scenes.");
    addButton (prevBtn, { 1008, 20, 34, 34 }, "Previous kit", [this] (const juce::ModifierKeys&) { proc.presets.step (-1); });
    addButton (nextBtn, { 1350, 20, 34, 34 }, "Next kit",     [this] (const juce::ModifierKeys&) { proc.presets.step (1); });

    addButton (restartBtn, { 932, 852, 108, 40 }, "Restart every lane and chain from the top on the next beat.",
               [this] (const juce::ModifierKeys&) { proc.requestRestart(); });
    addButton (rerollBtn, { 1048, 852, 108, 40 },
               "New dice for chance, humanise and random direction. Until you press this, the 'random' parts "
               "repeat exactly every time round.",
               [this] (const juce::ModifierKeys&) { proc.reroll(); statusText = "Rerolled"; statusTicks = 45; });
    addButton (muteAllBtn, { 1164, 852, 108, 40 }, "Silence every lane; they keep running underneath.",
               [this] (const juce::ModifierKeys&)
               {
                   proc.setParam (ids::masterMute, proc.getParam (ids::masterMute) > 0.5f ? 0.0f : 1.0f);
               });

    // --- scatter ---------------------------------------------------------------
    // Tap to latch on or off; hold for a momentary throw that ends when you let go.
    addButton (scatterBtn, { 1298, 766, 176, 38 },
               "SCATTER rearranges what you hear: steps swapped, reversed, repeated, slowed or chopped, on a 1/16 "
               "grid. The pattern keeps running underneath and nothing stored changes. Tap to latch it, hold it "
               "for a throw that stops when you let go.",
               [] (const juce::ModifierKeys&) {});
    scatterBtn.setFontHeight (15.0f);
    scatterBtn.onPress = [this]
    {
        scatterWasOn   = proc.getParam (ids::scatter) > 0.5f;
        scatterPressMs = juce::Time::getMillisecondCounter();
        if (! scatterWasOn)
            proc.setParam (ids::scatter, 1.0f);
    };
    scatterBtn.onRelease = [this]
    {
        const bool held = juce::Time::getMillisecondCounter() - scatterPressMs > 350;
        if (held || scatterWasOn)
            proc.setParam (ids::scatter, 0.0f);   // a throw ends, or a latched scatter is tapped off
    };

    addParamKnob (scatterTypeKnob, ids::scatterType, "TYPE", { 1298, 812, 84, 98 },
                  "1 STUTTER  2 REVERSE  3 HALF  4 DOUBLE  5 SKIP  6 SWAP  7 ROLL  8 TAPE STOP  9 GATE  10 SCRAMBLE");
    scatterTypeKnob.format = [] (double v) { return ids::scatterTypeNames()[juce::roundToInt (v)]; };
    addParamKnob (scatterDepthKnob, ids::scatterDepth, "DEPTH", { 1390, 812, 84, 98 },
                  "How far the scatter goes, 1 to 10: a bigger window, more steps moved, longer rolls.");

    // Show the right lit states from the first paint, not one timer tick later.
    timerCallback();

    startTimerHz (30);
}

BeadworkPanel::~BeadworkPanel()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

// ===========================================================================
// Helpers
// ===========================================================================
void BeadworkPanel::addParamKnob (Knob& k, const String& id, const String& caption, Rectangle<int> r, const String& tip)
{
    k.caption = caption;
    k.setTooltip (tip);
    k.setBounds (r);
    addAndMakeVisible (k);
    attachments.push_back (std::make_unique<SliderAttachment> (apvts, id, k));
}

void BeadworkPanel::addModelKnob (ModelKnob& k, const String& caption, int lo, int hi, Rectangle<int> r,
                                  const String& tip, std::function<int()> get, std::function<void (int)> set)
{
    k.caption = caption;
    k.setTooltip (tip);
    k.setRange (lo, hi, 1);
    k.get = std::move (get);
    k.set = std::move (set);
    k.setBounds (r);
    addAndMakeVisible (k);
    k.sync();
}

void BeadworkPanel::addChoice (ChoiceDisplay& c, const String& caption, Rectangle<int> r, const String& tip,
                               std::function<String()> text, std::function<void()> open)
{
    c.caption = caption;
    c.fontH = 13.0f;
    c.setTooltip (tip);
    c.getText = std::move (text);
    c.onOpen  = std::move (open);
    c.setBounds (r);
    addAndMakeVisible (c);
}

void BeadworkPanel::addButton (PanelButton& b, Rectangle<int> r, const String& tip,
                               std::function<void (const juce::ModifierKeys&)> click)
{
    b.setTooltip (tip);
    b.onClick = std::move (click);
    b.setBounds (r);
    addAndMakeVisible (b);
}

void BeadworkPanel::chooseFromMenu (juce::Component& target, const juce::StringArray& items, int current,
                                    std::function<void (int)> chosen)
{
    juce::PopupMenu m;
    for (int i = 0; i < items.size(); ++i)
        m.addItem (i + 1, items[i], true, i == current);
    auto safe = juce::Component::SafePointer<BeadworkPanel> (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&target).withMinimumWidth (target.getWidth()),
                     [safe, chosen] (int r)
                     {
                         if (safe != nullptr && r > 0)
                             chosen (r - 1);
                     });
}

void BeadworkPanel::chooseParam (juce::Component& target, const String& id)
{
    auto* param = apvts.getParameter (id);
    if (param == nullptr)
        return;

    juce::StringArray items;
    int current = 0, offset = 0;
    if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (param))
    {
        items = c->choices;
        current = c->getIndex();
    }
    else if (auto* n = dynamic_cast<juce::AudioParameterInt*> (param))
    {
        for (int v = n->getRange().getStart(); v <= n->getRange().getEnd(); ++v)
            items.add (String (v));
        offset = n->getRange().getStart();
        current = n->get() - offset;
    }
    chooseFromMenu (target, items, current, [this, id, offset] (int i) { proc.setParam (id, (float) (i + offset)); });
}

Pattern& BeadworkPanel::editing()
{
    return focusedTrack().patterns[(size_t) editPattern (focus)];
}

void BeadworkPanel::attachLevelKnob()
{
    levelAttachment.reset();
    levelAttachment = std::make_unique<SliderAttachment> (apvts, ids::level (focus), levelKnob);
}

// ===========================================================================
// LaneHost
// ===========================================================================
void BeadworkPanel::edited()
{
    proc.store.markDirty();
}

int BeadworkPanel::editPattern (int lane)
{
    // While a chain (the lane's own, or the scene chain) is choosing the
    // pattern, the screen follows what's playing so there's a playhead to
    // watch and the beads you touch are the ones you hear -- until you click
    // one of the lane's pads, which pins the screen to that pad until the next
    // time you press play. Otherwise it shows the pattern you picked, even
    // while that change waits for its bar line.
    const bool chained = project().tracks[(size_t) lane].chainOn != 0 || proc.sceneChainRunning();
    if (chained && proc.isRunning() && ! pinned[(size_t) lane])
        return juce::jlimit (0, kPatternsPerTrack - 1, proc.playingPattern (lane));
    return juce::jlimit (0, kPatternsPerTrack - 1, juce::roundToInt (proc.getParam (ids::pattern (lane))) - 1);
}

bool BeadworkPanel::scenePlaying (int scene)
{
    // The kit is playing a scene when every lane is on the scene's pattern and
    // its mute matches. A lane that's chaining follows its chain, not the
    // scene, so only its mute counts.
    const Scene& sc = project().scenes[(size_t) scene];
    if (! sc.used)
        return false;
    for (int t = 0; t < kNumTracks; ++t)
    {
        const bool chained = project().tracks[(size_t) t].chainOn != 0;
        if (! chained && proc.playingPattern (t) != sc.pattern[(size_t) t])
            return false;
        const bool sceneMute = ((sc.muted >> t) & 1u) != 0;
        if ((proc.getParam (ids::mute (t)) > 0.5f) != sceneMute)
            return false;
    }
    return true;
}

bool BeadworkPanel::playingShown (int lane)
{
    return proc.isRunning() && proc.playingPattern (lane) == editPattern (lane);
}

void BeadworkPanel::focusLane (int lane)
{
    if (lane == focus)
        return;
    focus = juce::jlimit (0, kNumTracks - 1, lane);
    attachLevelKnob();
    for (auto* k : { &stepsKnob, &fillsKnob, &rotateKnob, &velocityKnob, &accentKnob, &gateKnob, &chanceKnob })
    {
        k->sync();
    }
    repaint();
}

bool BeadworkPanel::isMuted (int lane)  { return proc.getParam (ids::mute (lane)) > 0.5f; }
bool BeadworkPanel::isSoloed (int lane) { return proc.getParam (ids::solo (lane)) > 0.5f; }
void BeadworkPanel::toggleMute (int lane) { proc.setParam (ids::mute (lane), isMuted (lane) ? 0.0f : 1.0f); }
void BeadworkPanel::toggleSolo (int lane) { proc.setParam (ids::solo (lane), isSoloed (lane) ? 0.0f : 1.0f); }

void BeadworkPanel::openNoteMenu (int lane, juce::Component& target)
{
    const int current = project().tracks[(size_t) lane].note;

    juce::PopupMenu m, other;
    m.addSectionHeader ("Drum rack pads");
    for (int n = 35; n <= 81; ++n)
    {
        if (n == 52 || n == 66)
            m.addColumnBreak();
        m.addItem (n + 1, noteName (n) + "   " + drumName (n), true, n == current);
    }
    for (int oct = 0; oct < 11; ++oct)
    {
        juce::PopupMenu sub;
        for (int k = 0; k < 12; ++k)
        {
            const int n = oct * 12 + k;
            if (n > 127) break;
            sub.addItem (n + 1, noteName (n), true, n == current);
        }
        other.addSubMenu (noteName (oct * 12) + " - " + noteName (juce::jmin (127, oct * 12 + 11)), sub);
    }
    m.addSeparator();
    m.addSubMenu ("Any note", other);

    auto safe = juce::Component::SafePointer<BeadworkPanel> (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&target),
                     [safe, lane] (int r)
                     {
                         if (safe == nullptr || r <= 0) return;
                         safe->project().tracks[(size_t) lane].note = (uint8_t) (r - 1);
                         safe->edited();
                         safe->repaint();
                     });
}

void BeadworkPanel::renameLane (int lane)
{
    auto* w = new juce::AlertWindow ("Rename lane", {}, juce::MessageBoxIconType::NoIcon, this);
    w->addTextEditor ("name", laneNameOf (project().tracks[(size_t) lane]), "Name");
    w->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    auto safe = juce::Component::SafePointer<BeadworkPanel> (this);
    w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w, lane] (int result)
    {
        const auto name = w->getTextEditorContents ("name").trim();
        std::unique_ptr<juce::AlertWindow> owned (w);
        if (safe == nullptr || result == 0)
            return;
        auto& n = safe->project().tracks[(size_t) lane].name;
        n.fill (0);
        name.copyToUTF8 (n.data(), kNameChars - 1);
        safe->edited();
        safe->repaint();
    }), false);
}

// ===========================================================================
// Menus
// ===========================================================================
void BeadworkPanel::openPresetMenu()
{
    juce::PopupMenu m;
    const auto entries = proc.presets.list();
    const int cur = proc.presets.currentIndex();

    m.addSectionHeader ("Factory");
    bool inUser = false;
    for (int i = 0; i < entries.size(); ++i)
    {
        if (! entries[i].factory && ! inUser)
        {
            m.addSectionHeader ("Yours");
            inUser = true;
        }
        m.addItem (i + 1, entries[i].name, true, i == cur);
    }
    if (! inUser)
    {
        m.addSectionHeader ("Yours");
        m.addItem (-1, "(none saved yet)", false, false);
    }
    m.addSeparator();
    m.addItem (9001, "Save as...");
    m.addItem (9002, "Delete this kit", proc.presets.currentIsUser());
    m.addItem (9003, "Reveal kit folder");

    auto safe = juce::Component::SafePointer<BeadworkPanel> (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetDisplay).withMinimumWidth (260),
                     [safe] (int r)
                     {
                         if (safe == nullptr || r <= 0)
                             return;
                         auto& pm = safe->proc.presets;
                         if (r == 9001) { safe->savePresetAs(); return; }
                         if (r == 9002)
                         {
                             const auto all = pm.list();
                             const int i = pm.currentIndex();
                             if (juce::isPositiveAndBelow (i, all.size()) && pm.remove (all[i]))
                             {
                                 safe->statusText = "Deleted";
                                 safe->statusTicks = 45;
                             }
                             return;
                         }
                         if (r == 9003)
                         {
                             auto folder = PresetManager::presetFolder();
                             folder.createDirectory();
                             folder.revealToUser();
                             return;
                         }
                         pm.loadIndex (r - 1);
                     });
}

void BeadworkPanel::savePresetAs()
{
    auto* w = new juce::AlertWindow ("Save kit", "Name these twelve lanes, their patterns, chains and scenes.",
                                     juce::MessageBoxIconType::NoIcon, this);
    w->addTextEditor ("name", proc.presets.currentName(), "Name");
    w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    auto safe = juce::Component::SafePointer<BeadworkPanel> (this);
    w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w] (int result)
    {
        const auto name = w->getTextEditorContents ("name");
        std::unique_ptr<juce::AlertWindow> owned (w);
        if (safe == nullptr || result == 0)
            return;
        safe->statusText = safe->proc.presets.saveAs (name) ? "Saved" : "Could not save";
        safe->statusTicks = 60;
    }), false);
}

void BeadworkPanel::patternMenu (int slot)
{
    juce::PopupMenu m;
    m.addSectionHeader ("Pattern " + String (slot + 1));
    m.addItem (1, "Copy");
    m.addItem (2, "Paste", clipboard.has_value());
    m.addItem (3, "Clear (empty figure, no edits)");
    m.addSeparator();
    m.addItem (4, "Copy to every slot on this lane");
    m.addItem (5, "Add to the chain");
    m.addSeparator();
    m.addSectionHeader ("All twelve lanes");
    m.addItem (6, "Copy all lanes (pattern " + String (slot + 1) + " of every lane)");
    m.addItem (7, kitClipboard ? "Paste all lanes (from pattern " + String (kitClipboardFrom + 1) + ")"
                               : String ("Paste all lanes"),
               kitClipboard.has_value());

    auto safe = juce::Component::SafePointer<BeadworkPanel> (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&patternPads),
                     [safe, slot] (int r)
                     {
                         if (safe != nullptr && r > 0)
                             safe->patternAction (slot, r);
                     });
}

void BeadworkPanel::patternAction (int slot, int choice)
{
    auto& t = focusedTrack();
    auto& p = t.patterns[(size_t) slot];
    switch (choice)
    {
        case 1: clipboard = p; break;
        case 2: if (clipboard) p = *clipboard; break;
        case 3: { Pattern blank; blank.fills = 0; blank.velocity = p.velocity; p = blank; break; }
        case 4: t.patterns.fill (p); break;
        case 5:
            if (! t.chainOn) { t.chainOn = 1; t.chainLen = 1; t.chainPattern[0] = (uint8_t) slot; t.chainRepeats[0] = 1; }
            else if (t.chainLen < kMaxChain) { t.chainPattern[t.chainLen] = (uint8_t) slot; t.chainRepeats[t.chainLen] = 1; ++t.chainLen; }
            break;
        case 6:
        {
            // The whole kit's pattern N: this slot from every lane.
            std::array<Pattern, kNumTracks> all {};
            for (int i = 0; i < kNumTracks; ++i)
                all[(size_t) i] = project().tracks[(size_t) i].patterns[(size_t) slot];
            kitClipboard     = all;
            kitClipboardFrom = slot;
            statusText  = "Copied P" + String (slot + 1) + " of all lanes";
            statusTicks = 60;
            return;
        }
        case 7:
            if (! kitClipboard)
                return;
            for (int i = 0; i < kNumTracks; ++i)
                project().tracks[(size_t) i].patterns[(size_t) slot] = (*kitClipboard)[(size_t) i];
            statusText  = "Pasted all lanes into P" + String (slot + 1);
            statusTicks = 60;
            break;
        default: return;
    }
    edited();
}

void BeadworkPanel::sceneMenu (int scene)
{
    const bool used = project().scenes[(size_t) scene].used != 0;
    juce::PopupMenu m;
    m.addSectionHeader ("Scene " + String (scene + 1));
    m.addItem (1, "Recall", used);
    m.addItem (2, used ? "Overwrite with the current lanes" : "Store the current lanes");
    m.addItem (3, "Rename...", used);
    m.addItem (4, "Clear", used);
    m.addSeparator();
    m.addItem (5, "Add to the scene chain");

    auto safe = juce::Component::SafePointer<BeadworkPanel> (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&scenePads),
                     [safe, scene] (int r)
                     {
                         if (safe == nullptr || r <= 0) return;
                         auto& p = safe->project();
                         switch (r)
                         {
                             case 1: safe->proc.recallScene (scene); break;
                             case 2: safe->proc.storeScene (scene); break;
                             case 3:
                             {
                                 auto* w = new juce::AlertWindow ("Rename scene", {}, juce::MessageBoxIconType::NoIcon, safe.getComponent());
                                 const auto& n = p.scenes[(size_t) scene].name;
                                 w->addTextEditor ("name", String::fromUTF8 (n.data(), (int) strnlen (n.data(), kNameChars)), "Name");
                                 w->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
                                 w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
                                 w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w, scene] (int res)
                                 {
                                     const auto name = w->getTextEditorContents ("name").trim().substring (0, 5);
                                     std::unique_ptr<juce::AlertWindow> owned (w);
                                     if (safe == nullptr || res == 0) return;
                                     auto& nm = safe->project().scenes[(size_t) scene].name;
                                     nm.fill (0);
                                     name.copyToUTF8 (nm.data(), kNameChars - 1);
                                     safe->edited();
                                 }), false);
                                 return;
                             }
                             case 4:
                                 p.scenes[(size_t) scene] = Scene {};
                                 if (safe->proc.selectedScene() == scene)
                                     safe->proc.setSelectedScene (-1);
                                 break;
                             case 5:
                                 if (p.sceneChainLen < kMaxSceneChain)
                                 {
                                     // A chain of one default slot is "empty": replace rather than append.
                                     const bool fresh = p.sceneChainLen == 1 && p.sceneMode == kPerform;
                                     const int at = fresh ? 0 : p.sceneChainLen;
                                     p.sceneChainScene[(size_t) at] = (uint8_t) scene;
                                     p.sceneChainBars[(size_t) at]  = 4;
                                     p.sceneChainLen = (uint8_t) (at + 1);
                                 }
                                 break;
                             default: break;
                         }
                         safe->edited();
                     });
}

void BeadworkPanel::laneChainMenu (int slot)
{
    juce::PopupMenu m, pats, reps;
    for (int i = 0; i < kPatternsPerTrack; ++i)
        pats.addItem (100 + i, "Pattern " + String (i + 1), true, focusedTrack().chainPattern[(size_t) slot] == i);
    for (int i = 1; i <= 16; ++i)
        reps.addItem (200 + i, String::fromUTF8 ("\xc3\x97") + String (i), true, focusedTrack().chainRepeats[(size_t) slot] == i);
    m.addSubMenu ("Pattern", pats);
    m.addSubMenu ("Repeats", reps);
    m.addSeparator();
    m.addItem (1, "Insert a slot after", focusedTrack().chainLen < kMaxChain);
    m.addItem (2, "Delete this slot", focusedTrack().chainLen > 1);

    auto safe = juce::Component::SafePointer<BeadworkPanel> (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&laneChain),
                     [safe, slot] (int r)
                     {
                         if (safe == nullptr || r <= 0) return;
                         auto& t = safe->focusedTrack();
                         if (r >= 200)      t.chainRepeats[(size_t) slot] = (uint8_t) (r - 200);
                         else if (r >= 100) t.chainPattern[(size_t) slot] = (uint8_t) (r - 100);
                         else if (r == 1)
                         {
                             for (int i = t.chainLen; i > slot + 1; --i)
                             {
                                 t.chainPattern[(size_t) i] = t.chainPattern[(size_t) (i - 1)];
                                 t.chainRepeats[(size_t) i] = t.chainRepeats[(size_t) (i - 1)];
                             }
                             t.chainPattern[(size_t) (slot + 1)] = t.chainPattern[(size_t) slot];
                             t.chainRepeats[(size_t) (slot + 1)] = 1;
                             ++t.chainLen;
                         }
                         else if (r == 2)
                         {
                             for (int i = slot; i < t.chainLen - 1; ++i)
                             {
                                 t.chainPattern[(size_t) i] = t.chainPattern[(size_t) (i + 1)];
                                 t.chainRepeats[(size_t) i] = t.chainRepeats[(size_t) (i + 1)];
                             }
                             --t.chainLen;
                         }
                         safe->edited();
                     });
}

void BeadworkPanel::sceneChainMenu (int slot)
{
    auto& p = project();
    juce::PopupMenu m, scenes, bars;
    for (int i = 0; i < kNumScenes; ++i)
        scenes.addItem (100 + i, "Scene " + String (i + 1) + (p.scenes[(size_t) i].used ? "" : "  (empty)"),
                        true, p.sceneChainScene[(size_t) slot] == i);
    for (int b : { 1, 2, 4, 8, 12, 16, 24, 32, 48, 64 })
        bars.addItem (200 + b, String (b) + (b == 1 ? " bar" : " bars"), true, p.sceneChainBars[(size_t) slot] == b);
    m.addSubMenu ("Scene", scenes);
    m.addSubMenu ("Length", bars);
    m.addSeparator();
    m.addItem (1, "Insert a slot after", p.sceneChainLen < kMaxSceneChain);
    m.addItem (2, "Delete this slot", p.sceneChainLen > 1);

    auto safe = juce::Component::SafePointer<BeadworkPanel> (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&sceneChain),
                     [safe, slot] (int r)
                     {
                         if (safe == nullptr || r <= 0) return;
                         auto& q = safe->project();
                         if (r >= 200)      q.sceneChainBars[(size_t) slot] = (uint8_t) (r - 200);
                         else if (r >= 100) q.sceneChainScene[(size_t) slot] = (uint8_t) (r - 100);
                         else if (r == 1)
                         {
                             for (int i = q.sceneChainLen; i > slot + 1; --i)
                             {
                                 q.sceneChainScene[(size_t) i] = q.sceneChainScene[(size_t) (i - 1)];
                                 q.sceneChainBars[(size_t) i]  = q.sceneChainBars[(size_t) (i - 1)];
                             }
                             q.sceneChainScene[(size_t) (slot + 1)] = q.sceneChainScene[(size_t) slot];
                             q.sceneChainBars[(size_t) (slot + 1)]  = q.sceneChainBars[(size_t) slot];
                             ++q.sceneChainLen;
                         }
                         else if (r == 2)
                         {
                             for (int i = slot; i < q.sceneChainLen - 1; ++i)
                             {
                                 q.sceneChainScene[(size_t) i] = q.sceneChainScene[(size_t) (i + 1)];
                                 q.sceneChainBars[(size_t) i]  = q.sceneChainBars[(size_t) (i + 1)];
                             }
                             --q.sceneChainLen;
                         }
                         safe->edited();
                     });
}

void BeadworkPanel::diceFigure()
{
    auto& pt = editing();
    auto& rnd = juce::Random::getSystemRandom();
    const int n = juce::jmax (1, (int) pt.steps);
    // Musical density: between an eighth and five eighths of the steps.
    const int lo = juce::jmax (1, n / 8), hi = juce::jmax (lo, (n * 5) / 8);
    pt.fills  = (uint8_t) (lo + rnd.nextInt (hi - lo + 1));
    pt.rotate = (int8_t) rnd.nextInt (n);
    edited();
}

// ===========================================================================
// Timer
// ===========================================================================
void BeadworkPanel::timerCallback()
{
    proc.store.publishIfDirty();
    ++tick;

    // Pressing play goes back to following the chains.
    const bool runningNow = proc.isRunning();
    if (runningNow && ! wasRunning)
        pinned.fill (false);
    wasRunning = runningNow;

    // Hit flashes.
    for (int t = 0; t < kNumTracks; ++t)
    {
        const int h = proc.hitCount (t);
        if (h != lastHits[(size_t) t])
        {
            lastHits[(size_t) t] = h;
            flashes[(size_t) t] = 1.0f;
        }
        else
        {
            flashes[(size_t) t] *= 0.78f;
        }
    }
    pulse = 0.5f + 0.5f * std::sin ((float) tick * 0.25f);

    for (auto& r : rows)
        r->repaint();
    necklace.repaint();

    patternPads.setPulse (pulse);
    patternPads.repaint();
    scenePads.setPulse (pulse);
    scenePads.repaint();
    laneChain.enabled = focusedTrack().chainOn != 0;
    laneChain.repaint();
    sceneChain.enabled = project().sceneMode != kPerform;
    sceneChain.repaint();

    for (auto* k : { &stepsKnob, &fillsKnob, &rotateKnob, &velocityKnob, &accentKnob, &gateKnob, &chanceKnob })
        k->sync();

    for (auto* c : { &divisionChoice, &fitChoice, &barsChoice, &directionChoice, &runChoice, &laneChannelChoice,
                     &swingGridChoice, &quantiseChoice, &clockChoice, &channelChoice, &presetDisplay })
        c->refresh();

    for (int i = 0; i < 6; ++i)
        layerButtons[(size_t) i].setLit ((int) currentLayer == i);
    chainBtn.setLit (focusedTrack().chainOn != 0);
    performBtn.setLit (project().sceneMode == kPerform);
    playBtn.setLit (project().sceneMode == kPlayOnce);
    loopBtn.setLit (project().sceneMode == kPlayLoop);
    muteAllBtn.setLit (proc.getParam (ids::masterMute) > 0.5f);
    scatterBtn.setLit (proc.getParam (ids::scatter) > 0.5f);
    pasteBtn.setLit (false);

    if (statusTicks > 0 && --statusTicks == 0)
        statusText.clear();

    // LCD, run LED, and the PATTERNS tab (it names the focused lane).
    repaint (kTransportBox.toNearestInt());
    repaint (12, kBankY - 10, 440, 22);
}

// ===========================================================================
// Paint
// ===========================================================================
void BeadworkPanel::paint (juce::Graphics& g)
{
    g.fillAll (colours::panel);

    // Mounting slots and the title, as on Dicey.
    drawScrewSlot (g, { 50.0f, 37.0f });
    drawScrewSlot (g, { (float) designW - 50.0f, 37.0f });
    drawText (g, "BEADWORK", { 100.0f, 14.0f, 220.0f, 46.0f }, 38.0f, colours::brand, true,
              juce::Justification::centredLeft);
    drawText (g, "euclidean drum sequencer  /  MIDI for Live", { 318.0f, 22.0f, 420.0f, 34.0f }, 19.0f,
              colours::ink, true, juce::Justification::centredLeft);
    drawText (g, "KIT", { 960.0f, 20.0f, 44.0f, 34.0f }, 13.0f, colours::ink, true, juce::Justification::centredRight);

    // Sections.
    const auto& tr = focusedTrack();
    drawSection (g, kLanesBox, "LANES");
    drawSection (g, kNecklaceBox, "NECKLACE");
    drawSection (g, kFeelBox, "FEEL");
    drawSection (g, kPatternsBox, "PATTERNS  /  " + laneNameOf (tr).toUpperCase());
    drawSection (g, kScenesBox, "SCENES");
    drawSection (g, kTransportBox, "TRANSPORT");

    drawText (g, "EDIT", { (float) kLanesX + 214.0f, (float) kLayerY, 76.0f, 30.0f }, 12.0f, colours::ink, true,
              juce::Justification::centredRight);

    // LCD.
    g.setColour (colours::black);
    g.fillRect (kLcd);
    g.setColour (colours::inkDim);
    g.drawRect (kLcd, 1.0f);

    const bool running = proc.isRunning();
    String big, small;
    if (statusText.isNotEmpty())
    {
        big = statusText;
        small = proc.presets.currentName();
    }
    else if (! running)
    {
        big = "--";
        small = "STOPPED";
    }
    else
    {
        const int step = proc.playingStep (focus);
        big = laneNameOf (tr).toUpperCase() + "  P" + String (proc.playingPattern (focus) + 1) + "  "
            + String (step + 1).paddedLeft ('0', 2);
        small = proc.sceneChainRunning() ? "SCENE " + String (proc.scene() + 1) + "   SLOT " + String (proc.sceneSlot() + 1)
                                         : String ("PLAYING");
    }
    if (proc.getParam (ids::scatter) > 0.5f && statusText.isEmpty())
        small = "SCATTER  " + ids::scatterTypeNames()[juce::roundToInt (proc.getParam (ids::scatterType))]
              + "  " + String::fromUTF8 ("\xc2\xb7") + "  " + String (juce::roundToInt (proc.getParam (ids::scatterDepth)));
    drawText (g, big, kLcd.withHeight (40.0f).translated (0.0f, 4.0f), 24.0f, colours::lcd, true);
    drawText (g, small, kLcd.withTrimmedTop (40.0f), 12.0f, colours::lcd.withAlpha (0.8f), true);

    // Run and mute LEDs.
    drawLed (g, { 1206.0f, 790.0f }, 9.0f, colours::ledGreen, running, running ? flashes[(size_t) focus] : 0.0f);
    drawText (g, "RUN", { 1186.0f, 802.0f, 40.0f, 20.0f }, 12.0f, colours::ink, true);
    drawLed (g, { 1252.0f, 790.0f }, 9.0f, colours::ledRed, proc.getParam (ids::masterMute) > 0.5f, 0.0f);
    drawText (g, "MUTED", { 1227.0f, 802.0f, 50.0f, 20.0f }, 12.0f, colours::ink, true);

    // The SCATTER cluster gets its own corner, divided off like a panel module.
    g.setColour (colours::ink.withAlpha (0.85f));
    g.drawLine (kScatterX, 762.0f, kScatterX, 916.0f, 1.2f);

    // One line of routing, the thing everyone asks first.
    drawText (g, "MIDI OUT  >  drum rack track, MIDI From: Beadwork",
              { 932.0f, 898.0f, 344.0f, 20.0f }, 12.0f, colours::inkDim, true, juce::Justification::centredLeft);
}

// ===========================================================================
BeadworkEditor::BeadworkEditor (BeadworkProcessor& p)
    : AudioProcessorEditor (p), panel (p)
{
    addAndMakeVisible (panel);
    setResizable (true, true);
    const double ratio = (double) BeadworkPanel::designW / BeadworkPanel::designH;
    setResizeLimits (960, (int) (960 / ratio), 2600, (int) (2600 / ratio));
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio (ratio);
    setSize (1260, (int) (1260 / ratio));
}

void BeadworkEditor::paint (juce::Graphics& g)
{
    g.fillAll (colours::panel);
}

void BeadworkEditor::resized()
{
    const float s = (float) getWidth() / (float) BeadworkPanel::designW;
    panel.setTransform (juce::AffineTransform::scale (s));
    panel.setBounds (0, 0, BeadworkPanel::designW, BeadworkPanel::designH);
}
