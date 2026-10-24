#include "DrumMaps.h"
#include <juce_audio_basics/juce_audio_basics.h>

namespace beadwork
{
namespace
{
juce::String laneNameOf (const Track& t)
{
    return juce::String::fromUTF8 (t.name.data(), (int) strnlen (t.name.data(), kNameChars));
}

DrumMap generalMidi()
{
    // The kit Beadwork has always opened with: General MIDI drum notes, which
    // is where Live's drum racks put their pads (C1 = 36 is the bottom left).
    Project p;
    initKit (p);
    return mapFromProject (p);
}

DrumMap chromatic (int first)
{
    // Sixteen pads in a row, labelled with their notes, Live's way (C1 = 36).
    DrumMap m;
    for (int i = 0; i < kNumTracks; ++i)
    {
        m[(size_t) i].note = first + i;
        m[(size_t) i].name = juce::MidiMessage::getMidiNoteName (first + i, true, true, 3);
    }
    return m;
}
} // namespace

DrumMap mapFromProject (const Project& p)
{
    DrumMap m;
    for (int i = 0; i < kNumTracks; ++i)
    {
        const Track& t = p.tracks[(size_t) i];
        m[(size_t) i] = { laneNameOf (t), (int) t.note, (int) t.channel };
    }
    return m;
}

void applyMap (Project& p, const DrumMap& m)
{
    for (int i = 0; i < kNumTracks; ++i)
    {
        Track& t = p.tracks[(size_t) i];
        const LaneMapping& l = m[(size_t) i];
        t.name.fill (0);
        l.name.copyToUTF8 (t.name.data(), kNameChars - 1);
        t.note    = (uint8_t) juce::jlimit (0, 127, l.note);
        t.channel = (uint8_t) juce::jlimit (0, 16, l.channel);
    }
}

juce::ValueTree mapToTree (const DrumMap& m, const juce::String& name)
{
    juce::ValueTree root ("BeadworkKitMap");
    root.setProperty ("version", 1, nullptr);
    root.setProperty ("name", name, nullptr);
    for (int i = 0; i < kNumTracks; ++i)
    {
        juce::ValueTree lane ("Lane");
        lane.setProperty ("index", i, nullptr);
        lane.setProperty ("name", m[(size_t) i].name, nullptr);
        lane.setProperty ("note", m[(size_t) i].note, nullptr);
        lane.setProperty ("channel", m[(size_t) i].channel, nullptr);
        root.appendChild (lane, nullptr);
    }
    return root;
}

bool mapFromTree (const juce::ValueTree& root, DrumMap& out)
{
    if (! root.hasType ("BeadworkKitMap"))
        return false;
    DrumMap m = out;
    for (const auto lane : root)
    {
        const int i = (int) lane.getProperty ("index", -1);
        if (! lane.hasType ("Lane") || ! juce::isPositiveAndBelow (i, kNumTracks))
            continue;
        auto& l = m[(size_t) i];
        l.name    = lane.getProperty ("name").toString().substring (0, kNameChars - 1);
        l.note    = juce::jlimit (0, 127, (int) lane.getProperty ("note", l.note));
        l.channel = juce::jlimit (0, 16, (int) lane.getProperty ("channel", 0));
    }
    out = m;
    return true;
}

const std::vector<FactoryMap>& factoryMaps()
{
    static const std::vector<FactoryMap> maps {
        { defaultMapName,     generalMidi() },
        { "Chromatic C1-D#2", chromatic (36) },   // Live's default drum rack, all 16 pads
        { "Chromatic C2-D#3", chromatic (48) },
    };
    return maps;
}

// ---------------------------------------------------------------------------
juce::File MapManager::mapFolder()
{
    auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);
   #if JUCE_MAC
    dir = dir.getChildFile ("Application Support");
   #endif
    return dir.getChildFile ("Beadwork").getChildFile ("Kit Maps");
}

juce::Array<MapManager::Entry> MapManager::list() const
{
    juce::Array<Entry> out;
    const auto& factory = factoryMaps();
    for (int i = 0; i < (int) factory.size(); ++i)
        out.add ({ factory[(size_t) i].name, true, i, {} });

    auto files = mapFolder().findChildFiles (juce::File::findFiles, false, juce::String ("*") + extension);
    std::sort (files.begin(), files.end(), [] (const juce::File& a, const juce::File& b)
    {
        return a.getFileNameWithoutExtension().compareNatural (b.getFileNameWithoutExtension()) < 0;
    });
    for (auto& f : files)
        out.add ({ f.getFileNameWithoutExtension(), false, -1, f });
    return out;
}

std::optional<DrumMap> MapManager::read (const Entry& e) const
{
    if (e.factory)
    {
        const auto& factory = factoryMaps();
        if (! juce::isPositiveAndBelow (e.factoryIndex, (int) factory.size()))
            return std::nullopt;
        return factory[(size_t) e.factoryIndex].map;
    }
    auto xml = juce::XmlDocument::parse (e.file);
    if (xml == nullptr)
        return std::nullopt;
    DrumMap m = mapFromProject (store.view());
    if (! mapFromTree (juce::ValueTree::fromXml (*xml), m))
        return std::nullopt;
    return m;
}

bool MapManager::load (const Entry& e)
{
    auto m = read (e);
    if (! m)
        return false;
    applyMap (store.edit(), *m);
    store.markDirty();
    store.publishNow();
    loaded = *m;
    current = e.name;
    currentUser = ! e.factory;
    return true;
}

bool MapManager::loadIndex (int index)
{
    const auto entries = list();
    return juce::isPositiveAndBelow (index, entries.size()) && load (entries[index]);
}

bool MapManager::saveAs (const juce::String& rawName)
{
    const auto name = juce::File::createLegalFileName (rawName.trim()).substring (0, 64).trim();
    if (name.isEmpty())
        return false;
    auto folder = mapFolder();
    if (! folder.isDirectory() && ! folder.createDirectory())
        return false;

    const auto m = mapFromProject (store.view());
    auto xml = mapToTree (m, name).createXml();
    if (xml == nullptr || ! xml->writeTo (folder.getChildFile (name + extension)))
        return false;

    loaded = m;
    current = name;
    currentUser = true;
    return true;
}

bool MapManager::remove (const Entry& e)
{
    if (e.factory || ! e.file.existsAsFile())
        return false;
    const bool ok = e.file.moveToTrash() || e.file.deleteFile();
    if (ok && current == e.name && currentUser)
        currentUser = false;
    return ok;
}

bool MapManager::currentIsUser() const
{
    return currentUser && mapFolder().getChildFile (current + extension).existsAsFile();
}

int MapManager::currentIndex() const
{
    const auto entries = list();
    for (int i = 0; i < entries.size(); ++i)
        if (entries[i].name == current && entries[i].factory != currentUser)
            return i;
    for (int i = 0; i < entries.size(); ++i)
        if (entries[i].name == current)
            return i;
    return -1;
}

void MapManager::restore (const juce::String& name)
{
    current = name.isNotEmpty() ? name : juce::String (defaultMapName);
    currentUser = mapFolder().getChildFile (current + extension).existsAsFile();

    // Compare against what the named map holds now, if it's still around;
    // otherwise treat the lanes as they came out of the session as the map.
    loaded = mapFromProject (store.view());
    const int i = currentIndex();
    const auto entries = list();
    if (juce::isPositiveAndBelow (i, entries.size()))
        if (auto m = read (entries[i]))
            loaded = *m;
}
} // namespace beadwork
