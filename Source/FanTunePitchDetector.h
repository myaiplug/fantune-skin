#pragma once

#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <vector>

namespace fantune::dsp
{

struct PitchDisplayState
{
    juce::String noteName { "---" };
    float frequencyHz = 0.0f;
    float centsOffset = 0.0f;
    float confidence = 0.0f;
    float vibratoBlend = 1.0f;
    bool tracking = false;
};

// YIN-based vocal pitch tracker with pre-allocated buffers, sibilance rejection,
// vibrato-aware smoothing, and decimated analysis for stable real-time use.
class VocalPitchDetector
{
public:
    static constexpr int kAnalysisHop = 384;

    void prepare (double sampleRate, int maxBlockSize)
    {
        juce::ignoreUnused (maxBlockSize);
        sr = sampleRate;
        analysisSize = juce::jmax (2048, (int) std::round (sampleRate * 0.060));
        analysisSize = juce::nextPowerOfTwo (analysisSize);

        ringBuffer.setSize (1, analysisSize * 2);
        ringBuffer.clear();

        const int maxTau = (int) std::ceil (sampleRate / 75.0);
        differenceBuffer.resize ((size_t) maxTau + 2, 0.0f);
        cmndBuffer.resize ((size_t) maxTau + 2, 1.0f);
        windowScratch.resize ((size_t) analysisSize, 0.0f);
        voiceScratch.resize ((size_t) analysisSize, 0.0f);

        const float voiceLpCutoff = 900.0f;
        const float voiceHpCutoff = 72.0f;
        const float sibHpCutoff = 3800.0f;
        voiceLpCoeff = 1.0f - std::exp (-2.0f * juce::MathConstants<float>::pi * voiceLpCutoff / (float) sampleRate);
        voiceHpCoeff = 1.0f - std::exp (-2.0f * juce::MathConstants<float>::pi * voiceHpCutoff / (float) sampleRate);
        sibHpCoeff = 1.0f - std::exp (-2.0f * juce::MathConstants<float>::pi * sibHpCutoff / (float) sampleRate);

        const float analysisRate = (float) sampleRate / (float) kAnalysisHop;
        vibratoLpCoeff = 1.0f - std::exp (-2.0f * juce::MathConstants<float>::pi * 5.5f / analysisRate);
        midiJumpRejectCents = 95.0f;

        reset();
    }

    void reset() noexcept
    {
        writePos = 0;
        samplesUntilAnalysis = 0;
        lastFrequency = 0.0f;
        smoothedFrequency = 0.0f;
        smoothedMidi = -1.0f;
        confidence = 0.0f;
        voiceLpState = voiceHpState = sibHpState = 0.0f;
        display = {};
    }

    void pushBlock (const float* samples, int numSamples) noexcept
    {
        auto* data = ringBuffer.getWritePointer (0);
        const int len = ringBuffer.getNumSamples();

        for (int i = 0; i < numSamples; ++i)
        {
            data[writePos] = samples[i];
            writePos = (writePos + 1) % len;
        }
    }

    bool processDecimated (int numSamples, float minHz, float maxHz) noexcept
    {
        samplesUntilAnalysis -= numSamples;
        if (samplesUntilAnalysis > 0)
            return false;

        samplesUntilAnalysis = kAnalysisHop;
        runAnalysis (minHz, maxHz);
        return true;
    }

    float getFrequency() const noexcept { return smoothedFrequency; }
    float getConfidence() const noexcept { return confidence; }
    float getVibratoBlend() const noexcept { return display.vibratoBlend; }
    float getSmoothedMidi() const noexcept { return smoothedMidi; }
    const PitchDisplayState& getDisplayState() const noexcept { return display; }

private:
    void runAnalysis (float minHz, float maxHz) noexcept
    {
        const int window = juce::jlimit (1024, analysisSize, (int) std::round (sr * 0.055));
        const auto* data = ringBuffer.getReadPointer (0);
        int pos = writePos;

        float fullRms = 0.0f;
        float sibRms = 0.0f;

        for (int i = window - 1; i >= 0; --i)
        {
            pos = (pos - 1 + ringBuffer.getNumSamples()) % ringBuffer.getNumSamples();
            const float x = data[pos];
            windowScratch[(size_t) i] = x;
            fullRms += x * x;

            sibHpState += sibHpCoeff * (x - sibHpState);
            const float hf = x - sibHpState;
            sibRms += hf * hf;
        }

        fullRms = std::sqrt (fullRms / (float) window);
        sibRms = std::sqrt (sibRms / (float) window);

        if (fullRms < 0.0025f)
        {
            decayTracking (0.75f);
            return;
        }

        const float sibilantRatio = sibRms / juce::jmax (fullRms, 1.0e-6f);
        if (sibilantRatio > 0.62f)
        {
            decayTracking (0.88f);
            updateDisplayFromSmoothed();
            return;
        }

        float voiceLp = 0.0f;
        float voiceHp = 0.0f;
        for (int i = 0; i < window; ++i)
        {
            const float x = windowScratch[(size_t) i];
            voiceLp += voiceLpCoeff * (x - voiceLp);
            voiceHp += voiceHpCoeff * (voiceLp - voiceHp);
            voiceScratch[(size_t) i] = voiceHp;
        }

        const float rawHz = detectYin (voiceScratch.data(), window, minHz, maxHz);
        if (rawHz <= 0.0f)
        {
            decayTracking (0.82f);
            return;
        }

        const float rawMidi = 69.0f + 12.0f * std::log2 (rawHz / 440.0f);

        if (smoothedMidi < 0.0f)
            smoothedMidi = rawMidi;

        const float midiDelta = std::abs (rawMidi - smoothedMidi);
        const float jumpReject = midiDelta > midiJumpRejectCents / 100.0f ? 0.35f : 1.0f;

        smoothedMidi += vibratoLpCoeff * (rawMidi - smoothedMidi) * jumpReject;
        smoothedFrequency = 440.0f * std::pow (2.0f, (smoothedMidi - 69.0f) / 12.0f);
        lastFrequency = rawHz;
        confidence = juce::jlimit (0.0f, 1.0f, confidence * 0.25f + 0.75f * (1.0f - sibilantRatio * 0.55f));

        const float vibratoActivity = juce::jlimit (0.0f, 1.0f, midiDelta / 1.2f);
        display.vibratoBlend = juce::jlimit (0.35f, 1.0f, 1.0f - vibratoActivity * 0.55f);
        updateDisplayFromSmoothed();
    }

    float detectYin (const float* windowData, int window, float minHz, float maxHz) noexcept
    {
        const int minTau = juce::jlimit (2, window - 1, (int) std::floor (sr / maxHz));
        const int maxTau = juce::jmin ((int) differenceBuffer.size() - 2,
                                       juce::jlimit (minTau + 1, window - 1, (int) std::ceil (sr / minHz)));

        for (int tau = 1; tau <= maxTau; ++tau)
        {
            float sum = 0.0f;
            for (int i = 0; i < window - tau; ++i)
            {
                const float delta = windowData[i] - windowData[i + tau];
                sum += delta * delta;
            }
            differenceBuffer[(size_t) tau] = sum;
        }

        float runningSum = 0.0f;
        for (int tau = 1; tau <= maxTau; ++tau)
        {
            runningSum += differenceBuffer[(size_t) tau];
            cmndBuffer[(size_t) tau] = differenceBuffer[(size_t) tau] * (float) tau / juce::jmax (runningSum, 1.0e-9f);
        }

        int bestTau = minTau;
        float bestValue = 1.0f;

        for (int tau = minTau; tau <= maxTau; ++tau)
        {
            if (cmndBuffer[(size_t) tau] < 0.1f)
            {
                while (tau + 1 <= maxTau && cmndBuffer[(size_t) (tau + 1)] < cmndBuffer[(size_t) tau])
                    ++tau;

                bestTau = tau;
                bestValue = cmndBuffer[(size_t) tau];
                break;
            }

            if (cmndBuffer[(size_t) tau] < bestValue)
            {
                bestValue = cmndBuffer[(size_t) tau];
                bestTau = tau;
            }
        }

        if (bestValue >= 0.22f || bestTau <= 0)
            return 0.0f;

        const float alpha = cmndBuffer[(size_t) (bestTau - 1)];
        const float beta  = cmndBuffer[(size_t) bestTau];
        const float gamma = cmndBuffer[(size_t) juce::jmin (bestTau + 1, maxTau)];
        const float denom = alpha - 2.0f * beta + gamma;

        float refinedTau = (float) bestTau;
        if (std::abs (denom) > 1.0e-9f)
            refinedTau += 0.5f * (alpha - gamma) / denom;

        const float yinConfidence = juce::jlimit (0.0f, 1.0f, 1.0f - bestValue);
        confidence = juce::jmax (confidence * 0.4f, yinConfidence);
        return (float) (sr / juce::jmax (refinedTau, 1.0f));
    }

    void decayTracking (float confDecay) noexcept
    {
        confidence *= confDecay;
        display.vibratoBlend = juce::jmax (0.35f, display.vibratoBlend * 0.96f);
        updateDisplayFromSmoothed();
    }

    void updateDisplayFromSmoothed() noexcept
    {
        static const char* kNames[] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

        if (smoothedMidi < 0.0f || confidence < 0.08f)
        {
            display.noteName = "---";
            display.frequencyHz = 0.0f;
            display.centsOffset = 0.0f;
            display.confidence = confidence;
            display.tracking = false;
            return;
        }

        const int nearest = (int) std::round (smoothedMidi);
        const int degree = ((nearest % 12) + 12) % 12;
        display.noteName = kNames[degree];
        display.frequencyHz = smoothedFrequency;
        display.centsOffset = (smoothedMidi - (float) nearest) * 100.0f;
        display.confidence = confidence;
        display.tracking = confidence > 0.18f;
    }

    juce::AudioBuffer<float> ringBuffer;
    std::vector<float> differenceBuffer;
    std::vector<float> cmndBuffer;
    std::vector<float> windowScratch;
    std::vector<float> voiceScratch;

    PitchDisplayState display;
    int analysisSize = 2048;
    int writePos = 0;
    int samplesUntilAnalysis = 0;
    double sr = 48000.0;
    float lastFrequency = 0.0f;
    float smoothedFrequency = 0.0f;
    float smoothedMidi = -1.0f;
    float confidence = 0.0f;
    float voiceLpState = 0.0f;
    float voiceHpState = 0.0f;
    float sibHpState = 0.0f;
    float voiceLpCoeff = 0.01f;
    float voiceHpCoeff = 0.01f;
    float sibHpCoeff = 0.01f;
    float vibratoLpCoeff = 0.1f;
    float midiJumpRejectCents = 95.0f;
};

} // namespace fantune::dsp