#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cstring>

namespace
{
    const juce::StringArray kPresets {
        "Bedroom Box Fan",
        "T-Pain Breeze",
        "Subtle Draft",
        "Box Fan High",
        "Hurricane Box Fan"
    };

    struct PresetValues
    {
        float speed, pitch, retune, glide, width, distance, tone, drive, spread, mix;
        int key, scale;
    };

    const PresetValues kPresetData[] {
        { 0.62f, 0.86f, 0.72f, 0.28f, 0.68f, 0.62f, 0.56f, 0.28f, 0.52f, 1.0f, 0, 0 },
        { 0.52f, 0.98f, 0.92f, 0.12f, 0.62f, 0.72f, 0.60f, 0.38f, 0.58f, 1.0f, 5, 0 },
        { 0.38f, 0.52f, 0.45f, 0.55f, 0.40f, 0.35f, 0.54f, 0.14f, 0.30f, 0.75f, 0, 1 },
        { 0.78f, 0.90f, 0.82f, 0.22f, 0.78f, 0.80f, 0.52f, 0.36f, 0.58f, 1.0f, 2, 0 },
        { 0.88f, 0.94f, 0.88f, 0.18f, 0.82f, 0.92f, 0.50f, 0.42f, 0.68f, 1.0f, 7, 0 },
    };
}

FanTuneAudioProcessor::FanTuneAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    compareSlotA = apvts.copyState();
    compareSlotB = apvts.copyState();
    factoryPresetCount = kPresets.size();
    pitchNoteChars[0].store ('-');
    pitchNoteChars[1].store ('-');
    pitchNoteChars[2].store ('-');
    pitchNoteChars[3].store ('\0');
    refreshUserPresets();
}

FanTuneAudioProcessor::~FanTuneAudioProcessor() = default;

juce::AudioProcessorValueTreeState::ParameterLayout FanTuneAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    juce::NormalisableRange<float> speedRange (0.0f, 1.0f, 0.001f);
    speedRange.setSkewForCentre (0.55f);

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        speedParamId, "Speed", speedRange, 0.62f,
        juce::AudioParameterFloatAttributes().withLabel ("BPF")));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        pitchParamId, "Pitch",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.86f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    juce::NormalisableRange<float> retuneRange (0.0f, 1.0f, 0.001f);
    retuneRange.setSkewForCentre (0.72f);

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        retuneParamId, "Retune", retuneRange, 0.78f,
        juce::AudioParameterFloatAttributes().withLabel ("ms")));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        glideParamId, "Glide",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.32f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        keyParamId, "Key",
        juce::StringArray { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 0));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        scaleParamId, "Scale",
        juce::StringArray { "Chromatic", "Major", "Minor", "Pentatonic", "Pent Minor", "Dorian", "Blues" }, 0));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        humanizeParamId, "Humanize",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.18f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        midiLearnParamId, "MIDI Learn", false));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        widthParamId, "Width",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.68f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        distanceParamId, "Distance",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.58f,
        juce::AudioParameterFloatAttributes().withLabel ("prox")));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        toneParamId, "Tone",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.55f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        driveParamId, "Drive",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.28f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        spreadParamId, "Spread",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.52f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        inputParamId, "Input",
        juce::NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        mixParamId, "Mix",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 1.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        outputParamId, "Output",
        juce::NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        combFreqParamId, "Comb Freq",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.35f,
        juce::AudioParameterFloatAttributes().withLabel ("Hz")));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        combDepthParamId, "Comb Depth",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        combFBParamId, "Comb FB",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        combDampParamId, "Comb Damp",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vintageRateParamId, "Vint Rate",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.3f,
        juce::AudioParameterFloatAttributes().withLabel ("Hz")));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vintageDepthParamId, "Vint Depth",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vintageFBParamId, "Vint FB",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        vintageMixParamId, "Vint Mix",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        bypassParamId, "Bypass", false));

    return layout;
}

const juce::String FanTuneAudioProcessor::getName() const { return JucePlugin_Name; }
bool FanTuneAudioProcessor::acceptsMidi() const { return true; }
bool FanTuneAudioProcessor::producesMidi() const { return false; }
bool FanTuneAudioProcessor::isMidiEffect() const { return false; }
double FanTuneAudioProcessor::getTailLengthSeconds() const { return 0.05; }
int FanTuneAudioProcessor::getNumPrograms()
{
    return factoryPresetCount + userPresetNames.size();
}
bool FanTuneAudioProcessor::hasEditor() const { return true; }

int FanTuneAudioProcessor::getCurrentProgram()
{
    return apvts.state.getProperty ("preset", 0);
}

void FanTuneAudioProcessor::applyFactoryPreset (int index)
{
    const auto& p = kPresetData[(size_t) index];

    if (auto* param = apvts.getParameter (speedParamId))    param->setValueNotifyingHost (param->convertTo0to1 (p.speed));
    if (auto* param = apvts.getParameter (pitchParamId))    param->setValueNotifyingHost (param->convertTo0to1 (p.pitch));
    if (auto* param = apvts.getParameter (retuneParamId))   param->setValueNotifyingHost (param->convertTo0to1 (p.retune));
    if (auto* param = apvts.getParameter (glideParamId))    param->setValueNotifyingHost (param->convertTo0to1 (p.glide));
    if (auto* param = apvts.getParameter (widthParamId))    param->setValueNotifyingHost (param->convertTo0to1 (p.width));
    if (auto* param = apvts.getParameter (distanceParamId)) param->setValueNotifyingHost (param->convertTo0to1 (p.distance));
    if (auto* param = apvts.getParameter (toneParamId))     param->setValueNotifyingHost (param->convertTo0to1 (p.tone));
    if (auto* param = apvts.getParameter (driveParamId))    param->setValueNotifyingHost (param->convertTo0to1 (p.drive));
    if (auto* param = apvts.getParameter (spreadParamId))   param->setValueNotifyingHost (param->convertTo0to1 (p.spread));
    if (auto* param = apvts.getParameter (mixParamId))     param->setValueNotifyingHost (param->convertTo0to1 (p.mix));
    if (auto* param = apvts.getParameter (keyParamId))      param->setValueNotifyingHost (param->convertTo0to1 ((float) p.key));
    if (auto* param = apvts.getParameter (scaleParamId))    param->setValueNotifyingHost (param->convertTo0to1 ((float) p.scale));
}

void FanTuneAudioProcessor::setCurrentProgram (int index)
{
    const int total = getNumPrograms();
    if (total <= 0)
        return;

    index = juce::jlimit (0, total - 1, index);
    apvts.state.setProperty ("preset", index, nullptr);

    if (index < factoryPresetCount)
    {
        applyFactoryPreset (index);
        return;
    }

    const int userIndex = index - factoryPresetCount;
    if (juce::isPositiveAndBelow (userIndex, userPresetFiles.size()))
        loadUserPreset (userPresetFiles.getReference (userIndex));
}

const juce::String FanTuneAudioProcessor::getProgramName (int index)
{
    if (juce::isPositiveAndBelow (index, factoryPresetCount))
        return kPresets[index];

    const int userIndex = index - factoryPresetCount;
    if (juce::isPositiveAndBelow (userIndex, userPresetNames.size()))
        return userPresetNames[userIndex];

    return {};
}

void FanTuneAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

void FanTuneAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    processSpec.sampleRate = sampleRate;
    processSpec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    processSpec.numChannels = (juce::uint32) getTotalNumOutputChannels();
    engine.prepare (processSpec);
    syncEngineFromParameters();
    reportedLatencySamples = engine.getLatencySamples();
    setLatencySamples (reportedLatencySamples);
}

void FanTuneAudioProcessor::releaseResources() {}

bool FanTuneAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;

    return true;
}

void FanTuneAudioProcessor::syncEngineFromParameters()
{
    engine.setSpeed (*apvts.getRawParameterValue (speedParamId));
    engine.setPitchCorrection (*apvts.getRawParameterValue (pitchParamId));
    engine.setRetuneSpeed (*apvts.getRawParameterValue (retuneParamId));
    engine.setGlide (*apvts.getRawParameterValue (glideParamId));
    engine.setHumanize (*apvts.getRawParameterValue (humanizeParamId));

    if (auto* keyParam = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (keyParamId)))
    {
        cachedKeyIndex = keyParam->getIndex();
        engine.setKey (cachedKeyIndex);
    }

    if (auto* scaleParam = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (scaleParamId)))
    {
        cachedScaleIndex = scaleParam->getIndex();
        engine.setScaleMask (fantune::dsp::scaleMaskFromIndex (cachedScaleIndex));
    }

    engine.setWidth (*apvts.getRawParameterValue (widthParamId));
    engine.setDistance (*apvts.getRawParameterValue (distanceParamId));
    engine.setTone (*apvts.getRawParameterValue (toneParamId));
    engine.setDrive (*apvts.getRawParameterValue (driveParamId));
    engine.setSpread (*apvts.getRawParameterValue (spreadParamId));
    engine.setInputGainDb (*apvts.getRawParameterValue (inputParamId));
    engine.setOutputGainDb (*apvts.getRawParameterValue (outputParamId));
    engine.setMix (*apvts.getRawParameterValue (mixParamId));
    engine.setCombFreq (*apvts.getRawParameterValue (combFreqParamId));
    engine.setCombDepth (*apvts.getRawParameterValue (combDepthParamId));
    engine.setCombFeedback (*apvts.getRawParameterValue (combFBParamId));
    engine.setCombDamping (*apvts.getRawParameterValue (combDampParamId));
    engine.setVintageRate (*apvts.getRawParameterValue (vintageRateParamId));
    engine.setVintageDepth (*apvts.getRawParameterValue (vintageDepthParamId));
    engine.setVintageFB (*apvts.getRawParameterValue (vintageFBParamId));
    engine.setVintageMix (*apvts.getRawParameterValue (vintageMixParamId));
    engine.setBladeCount (3);
}

void FanTuneAudioProcessor::processMidiKeyLearn (juce::MidiBuffer& midi)
{
    if (*apvts.getRawParameterValue (midiLearnParamId) <= 0.5f)
        return;

    for (const auto metadata : midi)
    {
        const auto msg = metadata.getMessage();

        if (! msg.isNoteOn())
            continue;

        const int note = msg.getNoteNumber() % 12;

        if (auto* keyParam = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (keyParamId)))
            keyParam->setValueNotifyingHost (keyParam->convertTo0to1 ((float) note));

        if (auto* learnParam = apvts.getParameter (midiLearnParamId))
            learnParam->setValueNotifyingHost (0.0f);

        break;
    }
}

void FanTuneAudioProcessor::saveCompareSlotA()
{
    compareSlotA = apvts.copyState();
    compareSlotIsA = true;
}

void FanTuneAudioProcessor::toggleCompareAB()
{
    const auto current = apvts.copyState();

    if (compareSlotIsA)
    {
        compareSlotA = current;
        apvts.replaceState (compareSlotB);
        compareSlotIsA = false;
    }
    else
    {
        compareSlotB = current;
        apvts.replaceState (compareSlotA);
        compareSlotIsA = true;
    }
}

float FanTuneAudioProcessor::getReportedLatencyMs() const noexcept
{
    if (getSampleRate() <= 0.0)
        return 0.0f;

    return (float) getLatencySamples() * 1000.0f / (float) getSampleRate();
}

void FanTuneAudioProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    processMidiKeyLearn (midi);

    const float inGain = juce::Decibels::decibelsToGain (apvts.getRawParameterValue (inputParamId)->load());
    const float outGain = juce::Decibels::decibelsToGain (apvts.getRawParameterValue (outputParamId)->load());
    buffer.applyGain (inGain * outGain);
}

void FanTuneAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    processMidiKeyLearn (midi);
    syncEngineFromParameters();

    const bool bypassed = *apvts.getRawParameterValue (bypassParamId) > 0.5f;
    engine.setEffectActive (! bypassed);

    const auto startTicks = juce::Time::getHighResolutionTicks();
    engine.process (buffer);

    const auto endTicks = juce::Time::getHighResolutionTicks();
    const double blockSeconds = (double) buffer.getNumSamples() / getSampleRate();
    const double usedSeconds = juce::Time::highResolutionTicksToSeconds (endTicks - startTicks);
    const float blockCpu = blockSeconds > 0.0 ? (float) (usedSeconds / blockSeconds) : 0.0f;
    cpuLoad.store (cpuLoad.load() * 0.90f + blockCpu * 0.10f);
    updatePitchTelemetry();
}

void FanTuneAudioProcessor::updatePitchTelemetry()
{
    const auto& display = engine.getPitchDisplayState();
    pitchCents.store (display.centsOffset);
    pitchConfidence.store (display.confidence);
    pitchFrequency.store (display.frequencyHz);
    pitchTracking.store (display.tracking);

    const auto utf8 = display.noteName.toUTF8();
    const int len = juce::jmin (3, (int) std::strlen (utf8.getAddress()));
    for (int i = 0; i < 3; ++i)
        pitchNoteChars[(size_t) i].store (i < len ? utf8.getAddress()[i] : (i == 0 ? '-' : '\0'));
    pitchNoteChars[3].store ('\0');
}

juce::String FanTuneAudioProcessor::getPitchNoteName() const noexcept
{
    char buf[4] {};
    for (int i = 0; i < 3; ++i)
        buf[i] = pitchNoteChars[(size_t) i].load();

    return juce::String::fromUTF8 (buf);
}

juce::StringArray FanTuneAudioProcessor::getAllPresetNames() const
{
    juce::StringArray names;
    for (int i = 0; i < factoryPresetCount; ++i)
        names.add (kPresets[i]);

    names.addArray (userPresetNames);
    return names;
}

void FanTuneAudioProcessor::refreshUserPresets()
{
    userPresetNames.clear();
    userPresetFiles.clear();

    const auto dir = FanTune::PresetManager::getPresetDirectory();
    juce::Array<juce::File> files;

    for (const auto& f : dir.findChildFiles (juce::File::findFiles, false,
                                             "*" + juce::String (FanTune::PresetManager::kExtension)))
        files.add (f);

    struct FileSorter
    {
        static int compareElements (const juce::File& a, const juce::File& b)
        {
            return a.getFileNameWithoutExtension().compareIgnoreCase (b.getFileNameWithoutExtension());
        }
    };

    FileSorter sorter;
    files.sort (sorter);

    for (const auto& f : files)
    {
        userPresetNames.add (f.getFileNameWithoutExtension());
        userPresetFiles.add (f);
    }

    presetListRevision.fetch_add (1);
}

bool FanTuneAudioProcessor::saveUserPreset (const juce::File& file, const juce::String& presetName)
{
    const bool ok = FanTune::PresetManager::savePresetToFile (file, presetName, apvts.copyState());
    if (ok)
        refreshUserPresets();
    return ok;
}

bool FanTuneAudioProcessor::loadUserPreset (const juce::File& file)
{
    juce::ValueTree state;
    juce::String name;

    if (! FanTune::PresetManager::loadPresetFromFile (file, state, name))
        return false;

    apvts.replaceState (state);
    compareSlotA = apvts.copyState();
    compareSlotB = apvts.copyState();
    return true;
}

bool FanTuneAudioProcessor::loadPresetFromFileChooser()
{
    auto chooser = std::make_shared<juce::FileChooser> (
        "Load FanTune Preset",
        FanTune::PresetManager::getPresetDirectory(),
        "*" + juce::String (FanTune::PresetManager::kExtension));

    const auto flags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;

    chooser->launchAsync (flags, [this, chooser] (const juce::FileChooser& fc)
    {
        const auto file = fc.getResult();
        if (! file.existsAsFile())
            return;

        if (loadUserPreset (file))
        {
            refreshUserPresets();
            const int idx = userPresetFiles.indexOf (file);
            if (idx >= 0)
                apvts.state.setProperty ("preset", factoryPresetCount + idx, nullptr);
        }
    });

    return true;
}

bool FanTuneAudioProcessor::savePresetToFileChooser()
{
    const int current = getCurrentProgram();
    juce::String defaultName = "My FanTune Preset";

    if (current < factoryPresetCount)
        defaultName = kPresets[current] + " Copy";
    else if (juce::isPositiveAndBelow (current - factoryPresetCount, userPresetNames.size()))
        defaultName = userPresetNames[current - factoryPresetCount];

    auto chooser = std::make_shared<juce::FileChooser> (
        "Save FanTune Preset",
        FanTune::PresetManager::suggestSaveFile (defaultName),
        "*" + juce::String (FanTune::PresetManager::kExtension));

    const auto flags = juce::FileBrowserComponent::saveMode
                     | juce::FileBrowserComponent::canSelectFiles
                     | juce::FileBrowserComponent::warnAboutOverwriting;

    chooser->launchAsync (flags, [this, chooser] (const juce::FileChooser& fc)
    {
        auto file = fc.getResult();
        if (file == juce::File())
            return;

        const auto name = file.getFileNameWithoutExtension();
        if (! saveUserPreset (file, name))
            return;

        const int idx = userPresetFiles.indexOf (file.withFileExtension (FanTune::PresetManager::kExtension));
        if (idx >= 0)
            apvts.state.setProperty ("preset", factoryPresetCount + idx, nullptr);
    });

    return true;
}

void FanTuneAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void FanTuneAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
        compareSlotA = apvts.copyState();
        compareSlotB = apvts.copyState();
    }
}

juce::AudioProcessorEditor* FanTuneAudioProcessor::createEditor()
{
    return new FanTuneAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FanTuneAudioProcessor();
}