#include "FanTunePresetManager.h"

namespace FanTune
{

juce::File PresetManager::getPresetDirectory()
{
    const auto dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                         .getChildFile ("FanTune")
                         .getChildFile ("Presets");
    dir.createDirectory();
    return dir;
}

juce::StringArray PresetManager::listUserPresets()
{
    juce::StringArray names;
    const auto dir = getPresetDirectory();

    for (const auto& f : dir.findChildFiles (juce::File::findFiles, false, "*" + juce::String (kExtension)))
        names.add (f.getFileNameWithoutExtension());

    names.sort (true);
    return names;
}

bool PresetManager::savePresetToFile (const juce::File& file,
                                      const juce::String& presetName,
                                      const juce::ValueTree& apvtsState)
{
    if (presetName.isEmpty() || ! apvtsState.isValid())
        return false;

    auto xml = std::make_unique<juce::XmlElement> ("FanTunePreset");
    xml->setAttribute ("name", presetName);
    xml->setAttribute ("version", "1.0.0");
    xml->setAttribute ("plugin", "FanTune");
    xml->addChildElement (apvtsState.createXml().release());

    const auto target = file.withFileExtension (kExtension);
    return xml->writeTo (target);
}

bool PresetManager::loadPresetFromFile (const juce::File& file,
                                        juce::ValueTree& outState,
                                        juce::String& outName)
{
    if (! file.existsAsFile())
        return false;

    const auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr || xml->getTagName() != "FanTunePreset")
        return false;

    outName = xml->getStringAttribute ("name", file.getFileNameWithoutExtension());

    if (auto* paramsXml = xml->getChildByName ("PARAMETERS"))
    {
        outState = juce::ValueTree::fromXml (*paramsXml);
        return outState.isValid();
    }

    return false;
}

juce::File PresetManager::suggestSaveFile (const juce::String& presetName)
{
    auto safe = presetName.trim();
    if (safe.isEmpty())
        safe = "My FanTune Preset";

    return getPresetDirectory().getChildFile (safe).withFileExtension (kExtension);
}

} // namespace FanTune