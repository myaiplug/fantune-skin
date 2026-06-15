#pragma once

#include <array>
#include <juce_audio_processors/juce_audio_processors.h>
#include "FanTuneDSP.h"
#include "FanTunePresetManager.h"

class FanTuneAudioProcessor final : public juce::AudioProcessor
{
public:
    FanTuneAudioProcessor();
    ~FanTuneAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }

    float getInputPeak() const noexcept   { return engine.getInputPeak(); }
    float getOutputPeak() const noexcept  { return engine.getOutputPeak(); }
    float getInputRms() const noexcept    { return engine.getInputRms(); }
    float getOutputRms() const noexcept   { return engine.getOutputRms(); }
    float getFanHz() const noexcept       { return engine.getFanHz(); }
    float getCpuLoad() const noexcept     { return cpuLoad; }
    int getReportedLatencySamples() const noexcept { return getLatencySamples(); }
    float getReportedLatencyMs() const noexcept;

    void saveCompareSlotA();
    void toggleCompareAB();
    bool isCompareSlotAActive() const noexcept { return compareSlotIsA; }

    juce::String getPitchNoteName() const noexcept;
    float getPitchCents() const noexcept       { return pitchCents.load(); }
    float getPitchConfidence() const noexcept  { return pitchConfidence.load(); }
    float getPitchFrequency() const noexcept { return pitchFrequency.load(); }
    bool isPitchTracking() const noexcept      { return pitchTracking.load(); }

    juce::StringArray getAllPresetNames() const;
    int getFactoryPresetCount() const noexcept { return factoryPresetCount; }
    void refreshUserPresets();
    int getPresetListRevision() const noexcept { return presetListRevision.load(); }
    bool saveUserPreset (const juce::File& file, const juce::String& presetName);
    bool loadUserPreset (const juce::File& file);
    bool loadPresetFromFileChooser();
    bool savePresetToFileChooser();

    static constexpr const char* speedParamId    = "speed";
    static constexpr const char* pitchParamId    = "pitch";
    static constexpr const char* retuneParamId   = "retune";
    static constexpr const char* glideParamId    = "glide";
    static constexpr const char* keyParamId      = "key";
    static constexpr const char* scaleParamId    = "scale";
    static constexpr const char* widthParamId    = "width";
    static constexpr const char* distanceParamId = "distance";
    static constexpr const char* toneParamId     = "tone";
    static constexpr const char* driveParamId    = "drive";
    static constexpr const char* spreadParamId   = "spread";
    static constexpr const char* inputParamId    = "input";
    static constexpr const char* mixParamId      = "mix";
    static constexpr const char* outputParamId   = "output";
    static constexpr const char* bypassParamId   = "bypass";
    static constexpr const char* humanizeParamId = "humanize";
    static constexpr const char* midiLearnParamId = "midiLearn";
    static constexpr const char* combFreqParamId    = "combFreq";
    static constexpr const char* combDepthParamId   = "combDepth";
    static constexpr const char* combFBParamId      = "combFB";
    static constexpr const char* combDampParamId    = "combDamp";
    static constexpr const char* vintageRateParamId  = "vintageRate";
    static constexpr const char* vintageDepthParamId = "vintageDepth";
    static constexpr const char* vintageFBParamId    = "vintageFB";
    static constexpr const char* vintageMixParamId   = "vintageMix";

private:
    void syncEngineFromParameters();
    void processMidiKeyLearn (juce::MidiBuffer& midi);
    void applyFactoryPreset (int index);
    void updatePitchTelemetry();
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::AudioProcessorValueTreeState apvts;
    fantune::dsp::FanTuneEngine engine;
    juce::dsp::ProcessSpec processSpec;

    juce::ValueTree compareSlotA;
    juce::ValueTree compareSlotB;
    bool compareSlotIsA = true;

    std::atomic<float> cpuLoad { 0.12f };
    std::atomic<float> pitchCents { 0.0f };
    std::atomic<float> pitchConfidence { 0.0f };
    std::atomic<float> pitchFrequency { 0.0f };
    std::atomic<bool> pitchTracking { false };
    std::array<std::atomic<char>, 4> pitchNoteChars {};

    juce::StringArray userPresetNames;
    juce::Array<juce::File> userPresetFiles;
    int factoryPresetCount = 0;
    std::atomic<int> presetListRevision { 0 };
    int reportedLatencySamples = 0;
    int cachedKeyIndex = 0;
    int cachedScaleIndex = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FanTuneAudioProcessor)
};