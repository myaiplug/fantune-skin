#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace FanTune
{

class PresetManager
{
public:
    static constexpr const char* kExtension = ".fantune";

    static juce::File getPresetDirectory();
    static juce::StringArray listUserPresets();
    static bool savePresetToFile (const juce::File& file,
                                    const juce::String& presetName,
                                    const juce::ValueTree& apvtsState);
    static bool loadPresetFromFile (const juce::File& file,
                                      juce::ValueTree& outState,
                                      juce::String& outName);
    static juce::File suggestSaveFile (const juce::String& presetName);
};

} // namespace FanTune