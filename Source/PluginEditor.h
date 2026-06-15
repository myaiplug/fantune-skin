#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "FanTuneLookAndFeel.h"
#include "FanTuneUIComponents.h"

class FanTuneAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                          private juce::Timer
{
public:
    explicit FanTuneAudioProcessorEditor (FanTuneAudioProcessor&);
    ~FanTuneAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void updateValueLabels();
    void updateFanVisuals();
    void refreshCompareButton();
    void refreshPresetBar();

    FanTuneAudioProcessor& processor;
    FanTune::FanTuneLookAndFeel lookAndFeel;

    FanTune::UI::FanGuardAssembly fanGuard;
    FanTune::UI::PresetBar presetBar;
    FanTune::UI::BypassLedButton bypassButton;
    FanTune::UI::LevelMeter inputMeter, outputMeter;
    FanTune::UI::CpuMeter cpuMeter;
    FanTune::UI::PitchDisplay pitchDisplay;
    FanTune::UI::KeySelector keySelector;
    FanTune::UI::ScaleSelector scaleSelector;
    FanTune::UI::MidiLearnButton midiLearnButton;

    FanTune::UI::HeaderActionButton saveButton { "Save" };
    FanTune::UI::HeaderActionButton abButton { "A/B" };

    FanTune::UI::KnobSection speedKnob    { "Speed", 62 };
    FanTune::UI::KnobSection pitchKnob    { "Pitch", 62 };
    FanTune::UI::KnobSection retuneKnob   { "Retune", 36 };
    FanTune::UI::KnobSection glideKnob    { "Glide", 36 };
    FanTune::UI::KnobSection humanizeKnob { "Humanize", 36 };
    FanTune::UI::KnobSection widthKnob    { "Width", 48 };
    FanTune::UI::KnobSection distanceKnob { "Distance", 48 };
    FanTune::UI::KnobSection toneKnob     { "Tone", 62 };
    FanTune::UI::KnobSection driveKnob    { "Drive", 62 };
    FanTune::UI::KnobSection spreadKnob   { "Spread", 48 };
    FanTune::UI::KnobSection inputKnob    { "Input", 36 };
    FanTune::UI::KnobSection mixKnob      { "Mix", 48 };
    FanTune::UI::KnobSection outputKnob   { "Output", 36 };
    FanTune::UI::KnobSection combFreqKnob { "Comb Fr", 36 };
    FanTune::UI::KnobSection combDepthKnob { "Comb Dep", 36 };
    FanTune::UI::KnobSection combFBKnob   { "Comb FB", 36 };
    FanTune::UI::KnobSection combDampKnob { "Comb Dmp", 36 };
    FanTune::UI::KnobSection vintRateKnob  { "Vint Rt", 36 };
    FanTune::UI::KnobSection vintDepthKnob { "Vint Dep", 36 };
    FanTune::UI::KnobSection vintFBKnob    { "Vint FB", 36 };
    FanTune::UI::KnobSection vintMixKnob   { "Vint Mix", 36 };

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::unique_ptr<SliderAttachment> speedAttach, pitchAttach, retuneAttach, glideAttach, humanizeAttach;
    std::unique_ptr<SliderAttachment> widthAttach, distanceAttach;
    std::unique_ptr<SliderAttachment> toneAttach, driveAttach, spreadAttach;
    std::unique_ptr<SliderAttachment> inputAttach, mixAttach, outputAttach;
    std::unique_ptr<SliderAttachment> combFreqAttach, combDepthAttach, combFBAttach, combDampAttach;
    std::unique_ptr<SliderAttachment> vintRateAttach, vintDepthAttach, vintFBAttach, vintMixAttach;
    std::unique_ptr<ComboAttachment> keyAttach, scaleAttach;
    std::unique_ptr<ButtonAttachment> bypassAttach, midiLearnAttach;

    float inputPeakLevel = 0.0f;
    float outputPeakLevel = 0.0f;
    float inputRmsLevel = 0.0f;
    float outputRmsLevel = 0.0f;
    float glowPhase = 0.0f;
    int lastPresetRevision = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FanTuneAudioProcessorEditor)
};