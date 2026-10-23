#include "PresetManager.h"
#include "PluginProcessor.h"
#include "DrumMaps.h"

namespace beadwork
{
juce::File PresetManager::presetFolder()
{
    auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);
   #if JUCE_MAC
    dir = dir.getChildFile ("Application Support");
   #endif
    dir = dir.getChildFile ("Beadwork");
    auto grooves = dir.getChildFile ("Grooves");

    // Grooves used to be saved as "kits" in Presets/. Bring them across once,
    // copying rather than moving so an older Beadwork still finds them.
    auto old = dir.getChildFile ("Presets");
    if (! grooves.isDirectory() && old.isDirectory() && grooves.createDirectory())
        for (auto& f : old.findChildFiles (juce::File::findFiles, false, juce::String ("*") + extension))
            f.copyFileTo (grooves.getChildFile (f.getFileName()));
    return grooves;
}

juce::String PresetManager::sanitise (const juce::String& name)
{
    auto s = juce::File::createLegalFileName (name.trim());
    return s.substring (0, 64).trim();
}

juce::Array<PresetManager::Entry> PresetManager::list() const
{
    juce::Array<Entry> out;
    const auto& factory = factoryPresets();
    for (int i = 0; i < (int) factory.size(); ++i)
        out.add ({ factory[(size_t) i].name, true, i, {} });

    auto files = presetFolder().findChildFiles (juce::File::findFiles, false, juce::String ("*") + extension);
    std::sort (files.begin(), files.end(), [] (const juce::File& a, const juce::File& b)
    {
        return a.getFileNameWithoutExtension().compareNatural (b.getFileNameWithoutExtension()) < 0;
    });
    for (auto& f : files)
        out.add ({ f.getFileNameWithoutExtension(), false, -1, f });
    return out;
}

bool PresetManager::load (const Entry& e)
{
    // A groove is everything but the wiring: the lanes keep the notes, labels
    // and channels of whatever kit map is loaded.
    const DrumMap keep = mapFromProject (proc.store.view());

    if (e.factory)
    {
        const auto& factory = factoryPresets();
        if (! juce::isPositiveAndBelow (e.factoryIndex, (int) factory.size()))
            return false;
        proc.applyFactoryPreset (factory[(size_t) e.factoryIndex]);
    }
    else
    {
        auto xml = juce::XmlDocument::parse (e.file);
        if (xml == nullptr)
            return false;
        proc.restoreGroove (juce::ValueTree::fromXml (*xml));
    }
    applyMap (proc.store.edit(), keep);
    proc.store.markDirty();
    proc.store.publishNow();

    current = e.name;
    currentUser = ! e.factory;
    proc.updateHostDisplay (juce::AudioProcessor::ChangeDetails().withProgramChanged (true));
    return true;
}

bool PresetManager::loadIndex (int index)
{
    const auto entries = list();
    if (! juce::isPositiveAndBelow (index, entries.size()))
        return false;
    return load (entries[index]);
}

void PresetManager::step (int delta)
{
    const auto entries = list();
    if (entries.isEmpty())
        return;
    int i = juce::jmax (0, currentIndex());
    i = (i + delta + entries.size()) % entries.size();
    load (entries[i]);
}

bool PresetManager::saveAs (const juce::String& rawName)
{
    const auto name = sanitise (rawName);
    if (name.isEmpty())
        return false;

    auto folder = presetFolder();
    if (! folder.isDirectory() && ! folder.createDirectory())
        return false;

    auto state = proc.captureState();
    state.setProperty ("presetName", name, nullptr);
    auto xml = state.createXml();
    if (xml == nullptr || ! xml->writeTo (folder.getChildFile (name + extension)))
        return false;

    current = name;
    currentUser = true;
    proc.updateHostDisplay (juce::AudioProcessor::ChangeDetails().withProgramChanged (true));
    return true;
}

bool PresetManager::remove (const Entry& e)
{
    if (e.factory || ! e.file.existsAsFile())
        return false;
    const bool ok = e.file.moveToTrash() || e.file.deleteFile();
    if (ok && current == e.name && currentUser)
        currentUser = false;
    return ok;
}

bool PresetManager::currentIsUser() const
{
    return currentUser && presetFolder().getChildFile (current + extension).existsAsFile();
}

int PresetManager::currentIndex() const
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

void PresetManager::restoreName (const juce::String& name)
{
    current = name.isNotEmpty() ? name : juce::String ("Init Groove");
    bool isFactory = false;
    for (auto& p : factoryPresets())
        isFactory = isFactory || current == p.name;
    currentUser = ! isFactory && presetFolder().getChildFile (current + extension).existsAsFile();
}
} // namespace beadwork
