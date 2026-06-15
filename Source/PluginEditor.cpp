#include "PluginEditor.h"

namespace
{
    juce::String formatDb (float db)
    {
        const int rounded = (int) std::round (db);
        return (rounded > 0 ? "+" : "") + juce::String (rounded) + " dB";
    }

    float boxFanBpf (float speedNorm)
    {
        return (6.0f + speedNorm * 18.0f) * (float) FanTune::UI::kBoxFanBlades;
    }
}

FanTuneAudioProcessorEditor::FanTuneAudioProcessorEditor (FanTuneAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setLookAndFeel (&lookAndFeel);
    setSize (FanTune::UI::kChassisW, FanTune::UI::kChassisH);
    setOpaque (true);

    addAndMakeVisible (fanGuard);
    addAndMakeVisible (presetBar);
    bypassButton.setClickingTogglesState (true);
    addAndMakeVisible (bypassButton);
    addAndMakeVisible (inputMeter);
    addAndMakeVisible (outputMeter);
    addAndMakeVisible (cpuMeter);
    addAndMakeVisible (pitchDisplay);
    addAndMakeVisible (keySelector);
    addAndMakeVisible (scaleSelector);
    addAndMakeVisible (midiLearnButton);

    for (auto* btn : { &saveButton, &abButton })
        addAndMakeVisible (btn);

    saveButton.onClick = [this]
    {
        processor.savePresetToFileChooser();
    };

    abButton.onClick = [this]
    {
        if (juce::ModifierKeys::getCurrentModifiers().isShiftDown())
        {
            processor.saveCompareSlotA();
            refreshCompareButton();
            return;
        }

        processor.toggleCompareAB();
        refreshCompareButton();
        updateValueLabels();
        updateFanVisuals();
    };

    refreshPresetBar();
    presetBar.onPresetChange = [this] (int idx)
    {
        processor.setCurrentProgram (idx);
        updateValueLabels();
        updateFanVisuals();
    };
    presetBar.onLoadFromFile = [this]
    {
        processor.loadPresetFromFileChooser();
    };
    presetBar.onRefreshPresets = [this]
    {
        processor.refreshUserPresets();
        refreshPresetBar();
    };

    for (auto* section : { &speedKnob, &pitchKnob, &retuneKnob, &glideKnob, &humanizeKnob, &widthKnob, &distanceKnob,
                           &toneKnob, &driveKnob, &spreadKnob, &inputKnob, &mixKnob, &outputKnob,
                           &combFreqKnob, &combDepthKnob, &combFBKnob, &combDampKnob,
                           &vintRateKnob, &vintDepthKnob, &vintFBKnob, &vintMixKnob })
    {
        addAndMakeVisible (section);
        section->getSlider().onValueChange = [this] { updateValueLabels(); updateFanVisuals(); };
    }

    auto& apvts = processor.getAPVTS();

    speedAttach    = std::make_unique<SliderAttachment> (apvts, FanTuneAudioProcessor::speedParamId, speedKnob.getSlider());
    pitchAttach    = std::make_unique<SliderAttachment> (apvts, FanTuneAudioProcessor::pitchParamId, pitchKnob.getSlider());
    retuneAttach   = std::make_unique<SliderAttachment> (apvts, FanTuneAudioProcessor::retuneParamId, retuneKnob.getSlider());
    glideAttach    = std::make_unique<SliderAttachment> (apvts, FanTuneAudioProcessor::glideParamId, glideKnob.getSlider());
    humanizeAttach = std::make_unique<SliderAttachment> (apvts, FanTuneAudioProcessor::humanizeParamId, humanizeKnob.getSlider());
    widthAttach    = std::make_unique<SliderAttachment> (apvts, FanTuneAudioProcessor::widthParamId, widthKnob.getSlider());
    distanceAttach = std::make_unique<SliderAttachment> (apvts, FanTuneAudioProcessor::distanceParamId, distanceKnob.getSlider());
    toneAttach     = std::make_unique<SliderAttachment> (apvts, FanTuneAudioProcessor::toneParamId, toneKnob.getSlider());
    driveAttach    = std::make_unique<SliderAttachment> (apvts, FanTuneAudioProcessor::driveParamId, driveKnob.getSlider());
    spreadAttach   = std::make_unique<SliderAttachment> (apvts, FanTuneAudioProcessor::spreadParamId, spreadKnob.getSlider());
    inputAttach    = std::make_unique<SliderAttachment> (apvts, FanTuneAudioProcessor::inputParamId, inputKnob.getSlider());
    mixAttach      = std::make_unique<SliderAttachment> (apvts, FanTuneAudioProcessor::mixParamId, mixKnob.getSlider());
    outputAttach   = std::make_unique<SliderAttachment> (apvts, FanTuneAudioProcessor::outputParamId, outputKnob.getSlider());
    combFreqAttach  = std::make_unique<SliderAttachment> (apvts, FanTuneAudioProcessor::combFreqParamId, combFreqKnob.getSlider());
    combDepthAttach = std::make_unique<SliderAttachment> (apvts, FanTuneAudioProcessor::combDepthParamId, combDepthKnob.getSlider());
    combFBAttach    = std::make_unique<SliderAttachment> (apvts, FanTuneAudioProcessor::combFBParamId, combFBKnob.getSlider());
    combDampAttach  = std::make_unique<SliderAttachment> (apvts, FanTuneAudioProcessor::combDampParamId, combDampKnob.getSlider());
    vintRateAttach  = std::make_unique<SliderAttachment> (apvts, FanTuneAudioProcessor::vintageRateParamId, vintRateKnob.getSlider());
    vintDepthAttach = std::make_unique<SliderAttachment> (apvts, FanTuneAudioProcessor::vintageDepthParamId, vintDepthKnob.getSlider());
    vintFBAttach    = std::make_unique<SliderAttachment> (apvts, FanTuneAudioProcessor::vintageFBParamId, vintFBKnob.getSlider());
    vintMixAttach   = std::make_unique<SliderAttachment> (apvts, FanTuneAudioProcessor::vintageMixParamId, vintMixKnob.getSlider());
    keyAttach      = std::make_unique<ComboAttachment> (apvts, FanTuneAudioProcessor::keyParamId, keySelector.getComboBox());
    scaleAttach    = std::make_unique<ComboAttachment> (apvts, FanTuneAudioProcessor::scaleParamId, scaleSelector.getComboBox());
    bypassAttach   = std::make_unique<ButtonAttachment> (apvts, FanTuneAudioProcessor::bypassParamId, bypassButton);
    midiLearnAttach = std::make_unique<ButtonAttachment> (apvts, FanTuneAudioProcessor::midiLearnParamId, midiLearnButton);

    refreshCompareButton();
    startTimerHz (30);
    updateValueLabels();
    updateFanVisuals();
}

FanTuneAudioProcessorEditor::~FanTuneAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void FanTuneAudioProcessorEditor::refreshCompareButton()
{
    abButton.setButtonText (processor.isCompareSlotAActive() ? "A/B" : "B/A");
    abButton.setToggleState (! processor.isCompareSlotAActive(), juce::dontSendNotification);
    abButton.setTooltip ("Toggle A/B compare. Shift+click stores current settings to slot A.");
}

void FanTuneAudioProcessorEditor::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    FanTune::UI::paintChassisBackground (g, bounds);

    auto header = bounds.removeFromTop (52);
    FanTune::UI::paintHeader (g, header);

    FanTune::UI::paintTrackLines (g, getWidth());
    FanTune::UI::paintPanelDividers (g);

    auto footer = bounds.removeFromBottom (46);
    FanTune::UI::paintFooter (g, footer);

    FanTune::UI::paintLogoMark (g, { 22.0f, 11.0f, 30.0f, 30.0f });
    FanTune::UI::paintBrandTitle (g, { 58, 10, 200, 24 });

    g.setColour (FanTune::Colour::CyanPrimary.withAlpha (0.5f));
    g.setFont (FanTune::UI::labelFont (9.0f, false));
    g.drawText ("Spectral Fan Modulator", 58, 30, 220, 12, juce::Justification::centredLeft);

    g.setColour (FanTune::Colour::TextDim.withAlpha (0.35f));
    g.setFont (FanTune::UI::labelFont (8.0f, false));
    g.drawText ("CPU", getWidth() / 2 + 58, getHeight() - 36, 28, 16, juce::Justification::centredRight);

    const auto latencyText = "Latency " + juce::String (processor.getReportedLatencyMs(), 1) + " ms";
    g.setColour (FanTune::Colour::TextDim.withAlpha (0.30f));
    g.setFont (FanTune::UI::labelFont (8.0f, false));
    g.drawText (latencyText, footer.withTrimmedLeft (20).withWidth (140), juce::Justification::centredLeft);

    g.setColour (FanTune::Colour::CyanPrimary.withAlpha (0.35f));
    g.setFont (FanTune::UI::labelFont (8.5f));
    g.drawText ("VINTAGE", getWidth() - 128, 56, 110, 16, juce::Justification::centred);

    g.setColour (FanTune::Colour::TextDim.withAlpha (0.22f));
    g.drawText ("FanTune v1.0.0 — VST3 / CLAP", footer.withTrimmedLeft (20).withTrimmedRight (20),
                juce::Justification::centredRight);
}

void FanTuneAudioProcessorEditor::resized()
{
    presetBar.setBounds (getWidth() / 2 - 110, 13, 220, 26);
    bypassButton.setBounds (getWidth() - 52, 12, 28, 28);
    saveButton.setBounds (getWidth() - 130, 14, 44, 22);
    abButton.setBounds (getWidth() - 82, 14, 36, 22);

    fanGuard.setBounds (getWidth() / 2 - 150, 64, 300, 300);

    inputMeter.setBounds (getWidth() / 2 - 178, 368, 5, 52);
    outputMeter.setBounds (getWidth() / 2 + 173, 368, 5, 52);

    keySelector.setBounds (48, 68, 64, 34);
    midiLearnButton.setBounds (116, 78, 44, 18);
    scaleSelector.setBounds (164, 68, 96, 34);
    retuneKnob.setBounds (268, 68, 52, 66);
    glideKnob.setBounds (326, 68, 52, 66);
    humanizeKnob.setBounds (384, 68, 52, 66);

    speedKnob.setBounds (48, 86, 80, 92);
    pitchKnob.setBounds (48, 200, 80, 92);
    pitchDisplay.setBounds (48, 294, 80, 52);
    widthKnob.setBounds (60, 352, 64, 78);

    toneKnob.setBounds (getWidth() - 128, 86, 80, 92);
    driveKnob.setBounds (getWidth() - 128, 178, 80, 92);
    distanceKnob.setBounds (getWidth() - 112, 264, 64, 78);
    spreadKnob.setBounds (getWidth() - 112, 332, 64, 78);

    inputKnob.setBounds (130, getHeight() - 68, 52, 66);
    combFreqKnob.setBounds (194, getHeight() - 68, 42, 62);
    combDepthKnob.setBounds (240, getHeight() - 68, 42, 62);
    combFBKnob.setBounds (286, getHeight() - 68, 42, 62);
    combDampKnob.setBounds (332, getHeight() - 68, 42, 62);
    mixKnob.setBounds (getWidth() / 2 - 24, getHeight() - 72, 64, 74);
    outputKnob.setBounds (getWidth() - 182, getHeight() - 68, 52, 66);

    const int vx = getWidth() - 132;
    vintRateKnob.setBounds  (vx, 74,  56, 66);
    vintDepthKnob.setBounds (vx, 152, 56, 66);
    vintFBKnob.setBounds    (vx, 230, 56, 66);
    vintMixKnob.setBounds   (vx, 308, 56, 66);

    cpuMeter.setBounds (getWidth() / 2 + 90, getHeight() - 36, 60, 16);
}

void FanTuneAudioProcessorEditor::timerCallback()
{
    inputPeakLevel = juce::jmax (inputPeakLevel * 0.78f, processor.getInputPeak());
    outputPeakLevel = juce::jmax (outputPeakLevel * 0.78f, processor.getOutputPeak());
    inputRmsLevel = inputRmsLevel * 0.82f + processor.getInputRms() * 0.18f;
    outputRmsLevel = outputRmsLevel * 0.82f + processor.getOutputRms() * 0.18f;

    inputMeter.setLevels (inputPeakLevel, inputRmsLevel);
    outputMeter.setLevels (outputPeakLevel, outputRmsLevel);

    glowPhase += 0.05f;
    fanGuard.setGlowPhase (glowPhase);
    cpuMeter.setLoad (processor.getCpuLoad());
    midiLearnButton.setLearning (*processor.getAPVTS().getRawParameterValue (FanTuneAudioProcessor::midiLearnParamId) > 0.5f);
    pitchDisplay.setState (processor.getPitchNoteName(),
                           processor.getPitchCents(),
                           processor.getPitchConfidence(),
                           processor.getPitchFrequency(),
                           processor.isPitchTracking());

    const int revision = processor.getPresetListRevision();
    if (revision != lastPresetRevision)
    {
        lastPresetRevision = revision;
        refreshPresetBar();
        updateValueLabels();
    }

    updateFanVisuals();
}

void FanTuneAudioProcessorEditor::refreshPresetBar()
{
    presetBar.setNames (processor.getAllPresetNames());
    presetBar.setIndex (processor.getCurrentProgram());
}

void FanTuneAudioProcessorEditor::updateValueLabels()
{
    const float speedNorm = (float) speedKnob.getSlider().getValue();

    speedKnob.setValueText (juce::String (boxFanBpf (speedNorm), 1) + " Hz");
    pitchKnob.setValueText (juce::String ((int) std::round (pitchKnob.getSlider().getValue() * 100.0)) + " %");

    const float retuneNorm = (float) retuneKnob.getSlider().getValue();
    const float retuneMs = juce::jmap (retuneNorm, 0.0f, 1.0f, 90.0f, 2.0f);
    retuneKnob.setValueText (juce::String (retuneMs, retuneMs < 10.0f ? 1 : 0) + " ms");

    glideKnob.setValueText (juce::String ((int) std::round (glideKnob.getSlider().getValue() * 100.0)) + " %");
    humanizeKnob.setValueText (juce::String ((int) std::round (humanizeKnob.getSlider().getValue() * 100.0)) + " %");
    widthKnob.setValueText (juce::String ((int) std::round (widthKnob.getSlider().getValue() * 100.0)) + " %");

    const float dist = (float) distanceKnob.getSlider().getValue();
    distanceKnob.setValueText (dist < 0.35f ? "Far" : (dist > 0.72f ? "Grille" : "Near"));

    const float toneNorm = (float) toneKnob.getSlider().getValue();
    const float tiltDb = (toneNorm * 2.0f - 1.0f) * 9.0f;
    toneKnob.setValueText ((tiltDb >= 0.0f ? "+" : "") + juce::String (tiltDb, 1) + " dB");

    driveKnob.setValueText (juce::String ((int) std::round (driveKnob.getSlider().getValue() * 100.0)) + " %");
    spreadKnob.setValueText (juce::String ((int) std::round (spreadKnob.getSlider().getValue() * 100.0)) + " %");
    {
        const float cf = (float) combFreqKnob.getSlider().getValue();
        const float combHz = juce::jmap (cf, 0.0f, 1.0f, 80.0f, 2200.0f);
        combFreqKnob.setValueText (juce::String ((int) combHz) + " Hz");
    }
    combDepthKnob.setValueText (juce::String ((int) std::round (combDepthKnob.getSlider().getValue() * 100.0)) + " %");
    combFBKnob.setValueText (juce::String ((int) std::round (combFBKnob.getSlider().getValue() * 100.0)) + " %");
    combDampKnob.setValueText (juce::String ((int) std::round (combDampKnob.getSlider().getValue() * 100.0)) + " %");
    {
        const float vr = (float) vintRateKnob.getSlider().getValue();
        const float rateHz = juce::jmap (vr, 0.0f, 1.0f, 0.05f, 8.0f);
        vintRateKnob.setValueText (juce::String (rateHz, rateHz < 1.0f ? 2 : 1) + " Hz");
    }
    vintDepthKnob.setValueText (juce::String ((int) std::round (vintDepthKnob.getSlider().getValue() * 100.0)) + " %");
    vintFBKnob.setValueText (juce::String ((int) std::round (vintFBKnob.getSlider().getValue() * 100.0)) + " %");
    vintMixKnob.setValueText (juce::String ((int) std::round (vintMixKnob.getSlider().getValue() * 100.0)) + " %");
    inputKnob.setValueText (formatDb ((float) inputKnob.getSlider().getValue()));
    mixKnob.setValueText (juce::String ((int) std::round (mixKnob.getSlider().getValue() * 100.0)) + " %");
    outputKnob.setValueText (formatDb ((float) outputKnob.getSlider().getValue()));
}

void FanTuneAudioProcessorEditor::updateFanVisuals()
{
    const float bpf = processor.getFanHz();
    const float rotationHz = bpf / (float) FanTune::UI::kBoxFanBlades;
    fanGuard.setRotationHz (rotationHz);
}