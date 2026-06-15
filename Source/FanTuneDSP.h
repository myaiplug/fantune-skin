#pragma once

#include <juce_dsp/juce_dsp.h>
#include "FanTunePitchDetector.h"
#include "FanTuneChopEngine.h"
#include <atomic>
#include <cmath>
#include <vector>

namespace fantune::dsp
{

inline float midiFromFrequency (float hz) noexcept
{
    if (hz <= 0.0f)
        return 0.0f;

    return 69.0f + 12.0f * std::log2 (hz / 440.0f);
}

inline bool isNoteInScale (int note, int root, int scaleMask) noexcept
{
    const int degree = ((note - root) % 12 + 12) % 12;
    return (scaleMask & (1 << degree)) != 0;
}

inline float snapMidiToScale (float midi, int root, int scaleMask) noexcept
{
    const int rounded = (int) std::round (midi);
    if (isNoteInScale (rounded, root, scaleMask))
        return (float) rounded;

    for (int offset = 1; offset <= 6; ++offset)
    {
        if (isNoteInScale (rounded + offset, root, scaleMask))
            return (float) (rounded + offset);

        if (isNoteInScale (rounded - offset, root, scaleMask))
            return (float) (rounded - offset);
    }

    return (float) rounded;
}

class PitchShifter
{
public:
    void prepare (double sampleRate, int maxBlockSize, float grainMs = 23.0f)
    {
        sr = sampleRate;
        const int delayLen = (int) std::round (sampleRate * 0.12) + maxBlockSize + 32;
        delayBuffer.setSize (1, delayLen);
        delayBuffer.clear();
        writeIndex = 0;
        setGrainMs (grainMs);
        readPosA = (float) (delayLen / 4);
        readPosB = readPosA + grainSize;
        crossfade = 0.0f;
        currentRatio = 1.0f;
        ratioSmoothed = 1.0f;
    }

    void setGrainMs (float grainMs) noexcept
    {
        grainSize = juce::jmax (320.0f, (float) std::round (sr * grainMs * 0.001f));
    }

    void setRatio (float ratio) noexcept
    {
        currentRatio = juce::jlimit (0.5f, 2.0f, ratio);
    }

    void setRatioSmoothing (float coeff) noexcept
    {
        ratioSmoothCoeff = juce::jlimit (0.0001f, 1.0f, coeff);
    }

    float processSample (float input) noexcept
    {
        ratioSmoothed += ratioSmoothCoeff * (currentRatio - ratioSmoothed);

        auto* data = delayBuffer.getWritePointer (0);
        const int len = delayBuffer.getNumSamples();

        data[writeIndex] = input;
        writeIndex = (writeIndex + 1) % len;

        const float outA = readInterpolated (readPosA);
        const float outB = readInterpolated (readPosB);
        const float fadeA = 0.5f * (1.0f - std::cos (crossfade * juce::MathConstants<float>::pi));
        float output = outA * fadeA + outB * (1.0f - fadeA);

        readPosA += ratioSmoothed;
        readPosB += ratioSmoothed;
        crossfade += currentRatio / grainSize;

        if (crossfade >= 1.0f)
        {
            crossfade -= 1.0f;

            if (fadeA > 0.5f)
                readPosB = readPosA + grainSize;
            else
                readPosA = readPosB + grainSize;
        }

        while (readPosA >= (float) len) readPosA -= (float) len;
        while (readPosA < 0.0f)         readPosA += (float) len;
        while (readPosB >= (float) len) readPosB -= (float) len;
        while (readPosB < 0.0f)         readPosB += (float) len;

        return output;
    }

private:
    float readInterpolated (float readPos) const noexcept
    {
        const auto* data = delayBuffer.getReadPointer (0);
        const int len = delayBuffer.getNumSamples();
        float wrapped = std::fmod (readPos, (float) len);

        if (wrapped < 0.0f)
            wrapped += (float) len;

        const int i0 = (int) wrapped;
        const float frac = wrapped - (float) i0;
        const int i1 = (i0 + 1) % len;
        const int im1 = (i0 - 1 + len) % len;
        const int i2 = (i0 + 2) % len;

        const float ym1 = data[im1];
        const float y0  = data[i0];
        const float y1  = data[i1];
        const float y2  = data[i2];

        const float c0 = y0;
        const float c1 = 0.5f * (y1 - ym1);
        const float c2 = ym1 - 2.5f * y0 + 2.0f * y1 - 0.5f * y2;
        const float c3 = 0.5f * (y2 - ym1) + 1.5f * (y0 - y1);
        return ((c3 * frac + c2) * frac + c1) * frac + c0;
    }

    juce::AudioBuffer<float> delayBuffer;
    int writeIndex = 0;
    float readPosA = 0.0f;
    float readPosB = 0.0f;
    float crossfade = 0.0f;
    float currentRatio = 1.0f;
    float ratioSmoothed = 1.0f;
    float ratioSmoothCoeff = 0.08f;
    float grainSize = 512.0f;
    double sr = 48000.0;
};

// Dual grain shifter: pitch shift then compensating shift preserves vocal formants.
class FormantPreservingPitchShifter
{
public:
    void prepare (double sampleRate, int maxBlockSize)
    {
        pitchStage.prepare (sampleRate, maxBlockSize, 26.0f);
        formantStage.prepare (sampleRate, maxBlockSize, 20.0f);
        pitchStage.setRatioSmoothing (0.045f);
        formantStage.setRatioSmoothing (0.065f);
    }

    void setPitchRatio (float ratio) noexcept
    {
        pitchRatio = juce::jlimit (0.5f, 2.0f, ratio);
        pitchStage.setRatio (pitchRatio);
    }

    void setFormantPreserve (float amount) noexcept
    {
        formantPreserve = juce::jlimit (0.0f, 1.0f, amount);
    }

    float processSample (float input) noexcept
    {
        const float pitched = pitchStage.processSample (input);

        if (formantPreserve < 0.001f)
            return pitched;

        const float correctionRatio = std::pow (pitchRatio, -formantPreserve);
        formantStage.setRatio (correctionRatio);
        return formantStage.processSample (pitched);
    }

private:
    PitchShifter pitchStage;
    PitchShifter formantStage;
    float pitchRatio = 1.0f;
    float formantPreserve = 0.85f;
};

class PinkNoiseGenerator
{
public:
    float processSample() noexcept
    {
        const float white = rng.nextFloat() * 2.0f - 1.0f;

        b0 = 0.99886f * b0 + white * 0.0555179f;
        b1 = 0.99332f * b1 + white * 0.0750759f;
        b2 = 0.96900f * b2 + white * 0.1538520f;
        b3 = 0.86650f * b3 + white * 0.3104856f;
        b4 = 0.55000f * b4 + white * 0.5329522f;
        b5 = -0.7616f * b5 - white * 0.0168980f;
        return (b0 + b1 + b2 + b3 + b4 + b5 + b6 + white * 0.5362f) * 0.11f;
    }

    void setSeed (int seed) noexcept { rng.setSeed (seed); }

private:
    juce::Random rng;
    float b0 {}, b1 {}, b2 {}, b3 {}, b4 {}, b5 {}, b6 {};
};

constexpr int kScaleChromatic       = 0xFFF;
constexpr int kScaleMajor           = 0xAB5;
constexpr int kScaleMinor           = 0x5AD;
constexpr int kScalePentatonicMajor = 0x295; // 0,2,4,7,9
constexpr int kScalePentatonicMinor = 0x4A9; // 0,3,5,7,10
constexpr int kScaleDorian          = 0x6AD; // 0,2,3,5,7,9,10
constexpr int kScaleBlues           = 0x4F1; // 0,3,5,6,7,10

inline int scaleMaskFromIndex (int index) noexcept
{
    switch (index)
    {
        case 1:  return kScaleMajor;
        case 2:  return kScaleMinor;
        case 3:  return kScalePentatonicMajor;
        case 4:  return kScalePentatonicMinor;
        case 5:  return kScaleDorian;
        case 6:  return kScaleBlues;
        default: return kScaleChromatic;
    }
}

class DryDelayLine
{
public:
    void prepare (double sampleRate, int maxBlockSize, float delayMs = 26.0f)
    {
        delaySamples = juce::jmax (1, (int) std::round (sampleRate * delayMs * 0.001f));
        const int len = delaySamples + maxBlockSize + 8;
        buffer.setSize (1, len);
        buffer.clear();
        writePos = 0;
    }

    void reset() noexcept
    {
        buffer.clear();
        writePos = 0;
    }

    int getDelaySamples() const noexcept { return delaySamples; }

    float process (float input) noexcept
    {
        auto* data = buffer.getWritePointer (0);
        const int len = buffer.getNumSamples();
        const int readPos = (writePos - delaySamples + len) % len;
        const float delayed = data[readPos];
        data[writePos] = input;
        writePos = (writePos + 1) % len;
        return delayed;
    }

private:
    juce::AudioBuffer<float> buffer;
    int writePos = 0;
    int delaySamples = 1;
};

class RoomTail
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        const float delaysMs[] { 11.0f, 19.0f, 31.0f, 47.0f };
        const float gains[]    { 0.42f, 0.28f, 0.18f, 0.11f };

        for (int i = 0; i < numTaps; ++i)
        {
            const int len = juce::jmax (8, (int) std::round (sampleRate * delaysMs[i] * 0.001f));
            buffers[i].setSize (1, len);
            buffers[i].clear();
            writePos[i] = 0;
            tapGain[i] = gains[i];
        }

        lpState = 0.0f;
        lpCoeff = 1.0f - std::exp (-2.0f * juce::MathConstants<float>::pi * 4200.0f / (float) sampleRate);
    }

    void reset() noexcept
    {
        for (int i = 0; i < numTaps; ++i)
        {
            buffers[i].clear();
            writePos[i] = 0;
        }
        lpState = 0.0f;
    }

    float process (float input, float amount) noexcept
    {
        if (amount < 0.0001f)
            return input;

        float wet = 0.0f;

        for (int i = 0; i < numTaps; ++i)
        {
            auto* data = buffers[i].getWritePointer (0);
            const int len = buffers[i].getNumSamples();
            const int readPos = (writePos[i] - len / 2 + len) % len;
            const float tap = data[readPos];
            data[writePos[i]] = input + tap * 0.12f;
            writePos[i] = (writePos[i] + 1) % len;
            wet += tap * tapGain[i];
        }

        lpState += lpCoeff * (wet - lpState);
        return input + amount * lpState * 0.55f;
    }

private:
    static constexpr int numTaps = 4;
    juce::AudioBuffer<float> buffers[numTaps];
    int writePos[numTaps] {};
    float tapGain[numTaps] {};
    float lpState = 0.0f;
    float lpCoeff = 0.01f;
    double sr = 48000.0;
};

// ── VintageTune ─────────────────────────────────────────────────────────
//
// LFO-driven delay line that produces chorus/flange modulation.
// Simulates the warm, unstable pitch character of vintage analog gear.
//   - Sine LFO modulates delay time
//   - Interpolated read for smooth modulation
//   - Feedback path for flange resonance
//   - Wet/dry mix at output

class VintageTune
{
public:
    void prepare (double sampleRate, int maxBlockSize)
    {
        sr = sampleRate;
        const int maxDelay = (int) std::round (sampleRate * 0.025) + maxBlockSize + 8;
        buffer.setSize (1, maxDelay);
        buffer.clear();
        writePos = 0;
        lfoPhase = 0.0f;
        currentRate = 0.5f;
        currentDepth = 0.3f;
        feedbackState = 0.0f;

        for (int i = 0; i < lutSize; ++i)
        {
            const float phase = (float) i * juce::MathConstants<float>::twoPi / (float) lutSize;
            sinLut[i] = std::sin (phase);
        }
    }

    void reset() noexcept
    {
        buffer.clear();
        writePos = 0;
        lfoPhase = 0.0f;
        feedbackState = 0.0f;
    }

    void setRate (float norm) noexcept
    {
        currentRate = juce::jmap (norm, 0.0f, 1.0f, 0.05f, 8.0f);
    }

    void setDepth (float norm) noexcept
    {
        currentDepth = juce::jlimit (0.0f, 1.0f, norm);
    }

    float processSample (float input, float feedback, float wetMix) noexcept
    {
        if (wetMix < 0.001f)
            return input;

        const float lfo = sinLut[(int) (lfoPhase * (float) lutSize) % lutSize];
        lfoPhase += currentRate / (float) sr;
        if (lfoPhase >= 1.0f)
            lfoPhase -= 1.0f;

        const float modDepth = currentDepth * 0.008f * (float) sr;
        const float baseDelay = 0.002f * (float) sr;
        const float delaySamples = juce::jmax (2.0f, baseDelay + modDepth * (1.0f + lfo) * 0.5f);
        const int delayInt = (int) delaySamples;
        const float frac = delaySamples - (float) delayInt;

        auto* data = buffer.getWritePointer (0);
        const int len = buffer.getNumSamples();

        const int r0 = (writePos - delayInt + len) % len;
        const int r1 = (r0 - 1 + len) % len;
        const float delayed = data[r0] + frac * (data[r1] - data[r0]);

        data[writePos] = input + feedback * delayed;
        writePos = (writePos + 1) % len;

        return input + wetMix * (delayed - input);
    }

private:
    static constexpr int lutSize = 1024;
    float sinLut[lutSize] {};
    juce::AudioBuffer<float> buffer;
    int writePos = 0;
    float lfoPhase = 0.0f;
    float currentRate = 0.5f;
    float currentDepth = 0.3f;
    float feedbackState = 0.0f;
    double sr = 48000.0;
};

// ── HousingComb ──────────────────────────────────────────────────────────
//
// Resonant comb filter that models the acoustic cavity of the fan's plastic
// housing. A feedback comb with one-pole lowpass damping in the feedback
// path produces spectral notches/peaks at regular intervals.
//
//    y[n] = x[n] + feedback * damped(y[n - D])
//
// where D = sampleRate / combFreqHz, and damping is a one-pole LPF on the
// feedback signal. The wet/dry mix is applied at the output.

class HousingComb
{
public:
    void prepare (double sampleRate, int maxBlockSize)
    {
        sr = sampleRate;
        const int maxDelay = (int) std::round (sampleRate / 30.0f) + maxBlockSize + 8;
        buffer.setSize (1, maxDelay);
        buffer.clear();
        writePos = 0;
        lpState = 0.0f;
        currentFreq = 400.0f;
        currentDamping = 0.3f;
    }

    void reset() noexcept
    {
        buffer.clear();
        writePos = 0;
        lpState = 0.0f;
    }

    void setFreq (float norm) noexcept
    {
        currentFreq = juce::jmap (norm, 0.0f, 1.0f, 80.0f, 2200.0f);
    }

    void setDamping (float norm) noexcept
    {
        currentDamping = juce::jlimit (0.0f, 0.99f, norm);
    }

    float getFreqHz() const noexcept { return currentFreq; }

    float processSample (float input, float depth, float feedback) noexcept
    {
        if (depth < 0.001f && feedback < 0.001f)
            return input;

        auto* data = buffer.getWritePointer (0);
        const int len = buffer.getNumSamples();
        const int delaySamples = juce::jmax (4, (int) std::round (sr / currentFreq));
        const int readPos = (writePos - delaySamples + len) % len;

        float delayed = data[readPos];

        const float dampCoeff = 1.0f - std::exp (-2.0f * juce::MathConstants<float>::pi
                                                  * (2000.0f + (1.0f - currentDamping) * 14000.0f) / (float) sr);
        lpState += dampCoeff * (delayed - lpState);
        const float damped = lpState;

        data[writePos] = input + feedback * damped;
        writePos = (writePos + 1) % len;

        const float wet = delayed;
        return input + depth * (wet - input);
    }

private:
    juce::AudioBuffer<float> buffer;
    int writePos = 0;
    float lpState = 0.0f;
    float currentFreq = 400.0f;
    float currentDamping = 0.3f;
    double sr = 48000.0;
};

class MotorHumGenerator
{
public:
    void prepare (double sampleRate) noexcept
    {
        sr = sampleRate;
        phase60 = phase120 = 0.0f;
        inc60 = juce::MathConstants<float>::twoPi * 60.0f / (float) sampleRate;
        inc120 = juce::MathConstants<float>::twoPi * 120.0f / (float) sampleRate;
    }

    float process (float speedNorm, float distance, float depth) noexcept
    {
        const float level = (0.08f + speedNorm * 0.22f) * (0.25f + distance * 0.75f) * depth;
        const float hum = 0.65f * std::sin (phase60) + 0.35f * std::sin (phase120);
        phase60 += inc60;
        phase120 += inc120;
        return hum * level;
    }

private:
    double sr = 48000.0;
    float phase60 = 0.0f;
    float phase120 = 0.0f;
    float inc60 = 0.0f;
    float inc120 = 0.0f;
};

// Short reflection path — blades reflect voice back (Physics Forums / CK-12).
class BladeReflection
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        const int len = juce::jmax (8, (int) std::round (sampleRate * 0.0032));
        buffer.setSize (1, len);
        buffer.clear();
        writePos = 0;
        hpState = 0.0f;
        const float hpCutoff = 280.0f;
        hpCoeff = 1.0f - std::exp (-2.0f * juce::MathConstants<float>::pi * hpCutoff / (float) sampleRate);
    }

    void reset() noexcept
    {
        buffer.clear();
        writePos = 0;
        hpState = 0.0f;
    }

    float process (float input, float mix) noexcept
    {
        if (mix < 0.0001f)
            return input;

        auto* data = buffer.getWritePointer (0);
        const int len = buffer.getNumSamples();
        const int readPos = (writePos - len / 3 + len) % len;
        float reflected = data[readPos];

        hpState += hpCoeff * (reflected - hpState);
        reflected = hpState;

        data[writePos] = input + reflected * 0.07f;
        writePos = (writePos + 1) % len;

        return input + mix * reflected * 0.65f;
    }

private:
    juce::AudioBuffer<float> buffer;
    int writePos = 0;
    float hpState = 0.0f;
    float hpCoeff = 0.01f;
    double sr = 48000.0;
};

struct FanSampleState
{
    float rotationPhase = 0.0f;
    float wobblePhase = 0.0f;
    float motorPhase = 0.0f;

    // Proximity: 0 = across the room, 1 = lips on the grille.
    static float proximity (float distance) noexcept
    {
        return 0.22f + distance * 0.78f;
    }

    // Asymmetric box-fan blades: hard slap on obstruction, softer reopening.
    float bladeTransmission (float depth, float distance, int bladeCount, float channelPhaseOffset) const noexcept
    {
        const float prox = proximity (distance);
        const float effectiveDepth = depth * prox;
        const float rotPhase = rotationPhase + channelPhaseOffset;
        float maxObstruction = 0.0f;

        for (int b = 0; b < bladeCount; ++b)
        {
            const float bladePhase = rotPhase
                                   + juce::MathConstants<float>::twoPi * (float) b / (float) bladeCount;
            const float raised = 0.5f * (1.0f + std::cos (bladePhase));
            const float bladeSlope = std::sin (bladePhase);
            const float sharpObstruct = 1.3f + effectiveDepth * 4.2f;
            const float sharpOpen = 2.0f + effectiveDepth * 5.5f;
            const float sharpness = bladeSlope < 0.0f ? sharpObstruct : sharpOpen;
            maxObstruction = juce::jmax (maxObstruction, std::pow (raised, sharpness));
        }

        const float bodyFloor = 0.16f + (1.0f - effectiveDepth) * 0.20f + (1.0f - prox) * 0.10f;
        float transmission = 1.0f - effectiveDepth * 0.68f * maxObstruction;
        transmission = bodyFloor + (1.0f - bodyFloor) * transmission;

        const float motorHum = 1.0f + 0.010f * std::sin (motorPhase) * prox;
        const float wobble = 1.0f + 0.006f * std::sin (wobblePhase);
        transmission *= motorHum * wobble;

        return juce::jlimit (bodyFloor, 1.0f, transmission);
    }

    float dopplerRatio (float depth, float distance, int bladeCount, float channelPhaseOffset) const noexcept
    {
        const float prox = proximity (distance);
        const float rotPhase = rotationPhase + channelPhaseOffset;
        float bladeVector = 0.0f;

        for (int b = 0; b < bladeCount; ++b)
        {
            const float bladePhase = rotPhase
                                   + juce::MathConstants<float>::twoPi * (float) b / (float) bladeCount;
            bladeVector += std::sin (bladePhase);
        }

        bladeVector /= (float) bladeCount;
        const float maxCents = (1.8f + depth * 5.0f) * prox;
        return std::pow (2.0f, (bladeVector * maxCents) / 1200.0f);
    }

    float reflectionAmount (float depth, float distance, float transmission) const noexcept
    {
        return depth * proximity (distance) * 0.32f * (1.0f - transmission);
    }

    float roomTailAmount (float depth, float distance, float transmission, float openBurst) const noexcept
    {
        return depth * proximity (distance) * 0.38f * openBurst * transmission;
    }

    float airCutoffHz (float depth, float distance, float transmission) const noexcept
    {
        const float prox = proximity (distance);
        const float base = 13800.0f - depth * 4200.0f - (1.0f - prox) * 4800.0f;
        return juce::jlimit (480.0f, 18000.0f, base - depth * 1800.0f * (1.0f - transmission));
    }

    float turbulenceNoise (float depth, float distance, float transmission) const noexcept
    {
        return depth * proximity (distance) * (0.018f + 0.10f * (1.0f - transmission));
    }
};

constexpr int kBoxFanBlades = 3;

inline float rotationHzFromSpeed (float speedNorm) noexcept
{
    return 6.0f + speedNorm * 18.0f;
}

inline float bladePassHzFromSpeed (float speedNorm) noexcept
{
    return rotationHzFromSpeed (speedNorm) * (float) kBoxFanBlades;
}

class FanTuneEngine
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        numChannels = (int) spec.numChannels;
        pitchDetector.prepare (sampleRate, (int) spec.maximumBlockSize);

        maxBlockSize = (int) spec.maximumBlockSize;

        for (int ch = 0; ch < 2; ++ch)
        {
            flutterShifter[ch].prepare (sampleRate, maxBlockSize, 14.0f);
            formantTune[ch].prepare (sampleRate, maxBlockSize);
            flutterShifter[ch].setRatioSmoothing (0.14f);
            toneFilter[ch].prepare (spec);
            toneFilter[ch].reset();
            reflection[ch].prepare (sampleRate);
            roomTail[ch].prepare (sampleRate);
            dryDelay[ch].prepare (sampleRate, maxBlockSize, 26.0f);
            housingComb[ch].prepare (sampleRate, maxBlockSize);
            vintageTune[ch].prepare (sampleRate, maxBlockSize);
            polishFilter[ch].prepare (spec);
            polishFilter[ch].reset();
        }

        motorHum.prepare (sampleRate);
        amEnvelope.prepare (sampleRate);
        monoScratch.resize ((size_t) maxBlockSize);
        dryScratch[0].resize ((size_t) maxBlockSize);
        dryScratch[1].resize ((size_t) maxBlockSize);

        const float hpCutoff = 72.0f;
        inputHpCoeff = 1.0f - std::exp (-2.0f * juce::MathConstants<float>::pi * hpCutoff / (float) sampleRate);
        chopLpCoeff = 1.0f - std::exp (-2.0f * juce::MathConstants<float>::pi * 820.0f / (float) sampleRate);
        pitchSmoothCoeff = 1.0f - std::exp (-1.0f / (0.035f * (float) sampleRate));
        transAttackCoeff = 1.0f - std::exp (-1.0f / (0.0022f * (float) sampleRate));
        transReleaseCoeff = 1.0f - std::exp (-1.0f / (0.011f * (float) sampleRate));
        dcBlockCoeff = 1.0f - std::exp (-2.0f * juce::MathConstants<float>::pi * 8.0f / (float) sampleRate);
        compCoeff = 1.0f - std::exp (-1.0f / (0.008f * (float) sampleRate));

        pinkNoise.setSeed (1337);
        humanizeRng.setSeed (4242);
        initSmoothers (spec.sampleRate);
        updateLatencySamples();
        reset();
    }

    void reset()
    {
        detectedHz = 0.0f;
        smoothedPitchHz = 0.0f;
        glideMidi = -1.0f;
        smoothedTuneRatio[0] = smoothedTuneRatio[1] = 1.0f;
        smoothedFlutterRatio[0] = smoothedFlutterRatio[1] = 1.0f;
        smoothedTransmission[0] = smoothedTransmission[1] = 1.0f;
        prevTransmission[0] = prevTransmission[1] = 1.0f;
        airLpState[0] = airLpState[1] = 0.0f;
        chopLpState[0] = chopLpState[1] = 0.0f;
        noiseLpState[0] = noiseLpState[1] = 0.0f;
        inputHpState[0] = inputHpState[1] = 0.0f;
        dcBlockState[0] = dcBlockState[1] = 0.0f;
        compEnv[0] = compEnv[1] = 0.0f;
        fanState.rotationPhase = 0.0f;
        fanState.wobblePhase = 0.0f;
        fanState.motorPhase = 0.0f;

        for (int ch = 0; ch < 2; ++ch)
        {
            reflection[ch].reset();
            roomTail[ch].reset();
            dryDelay[ch].reset();
            housingComb[ch].reset();
            vintageTune[ch].reset();
            polishFilter[ch].reset();
        }

        inputPeak.store (0.0f);
        outputPeak.store (0.0f);
        inputRms.store (0.0f);
        outputRms.store (0.0f);
        humanizeCurrent = humanizeTarget = 0.0f;
        lastSnappedMidi = -100.0f;
    }

    void setSpeed (float norm) noexcept           { speedNorm.setTargetValue (norm); }
    void setPitchCorrection (float norm) noexcept { pitchCorrection.setTargetValue (norm); }
    void setRetuneSpeed (float norm) noexcept     { retuneSpeed.setTargetValue (norm); }
    void setGlide (float norm) noexcept           { glideAmount.setTargetValue (norm); }
    void setKey (int semitone) noexcept           { keyRoot = juce::jlimit (0, 11, semitone); }
    void setScaleMask (int mask) noexcept         { scaleMask = mask == 0 ? kScaleChromatic : mask; }
    void setWidth (float norm) noexcept           { fanDepth.setTargetValue (norm); }
    void setDistance (float norm) noexcept        { distanceNorm.setTargetValue (norm); }
    void setBladeCount (int /*blades*/) noexcept { bladeCount = kBoxFanBlades; }
    void setTone (float norm) noexcept
    {
        toneNorm.setTargetValue (norm);
        updateToneFilters();
    }
    void setDrive (float norm) noexcept          { drive.setTargetValue (norm); }
    void setSpread (float norm) noexcept          { spread.setTargetValue (norm); }
    void setInputGainDb (float db) noexcept      { inputGain.setTargetValue (juce::Decibels::decibelsToGain (db)); }
    void setOutputGainDb (float db) noexcept     { outputGain.setTargetValue (juce::Decibels::decibelsToGain (db)); }
    void setMix (float norm) noexcept            { mix.setTargetValue (juce::jlimit (0.0f, 1.0f, norm)); }
    void setHumanize (float norm) noexcept       { humanizeAmount.setTargetValue (juce::jlimit (0.0f, 1.0f, norm)); }
    void setEffectActive (bool active) noexcept  { effectBlend.setTargetValue (active ? 1.0f : 0.0f); }
    void setCombFreq (float norm) noexcept       { combFreq.setTargetValue (juce::jlimit (0.0f, 1.0f, norm)); }
    void setCombDepth (float norm) noexcept      { combDepth.setTargetValue (juce::jlimit (0.0f, 1.0f, norm)); }
    void setCombFeedback (float norm) noexcept   { combFeedback.setTargetValue (juce::jlimit (0.0f, 1.0f, norm)); }
    void setCombDamping (float norm) noexcept    { combDamping.setTargetValue (juce::jlimit (0.0f, 1.0f, norm)); }
    void setVintageRate (float norm) noexcept    { vintageRate.setTargetValue (juce::jlimit (0.0f, 1.0f, norm)); }
    void setVintageDepth (float norm) noexcept   { vintageDepth.setTargetValue (juce::jlimit (0.0f, 1.0f, norm)); }
    void setVintageFB (float norm) noexcept      { vintageFB.setTargetValue (juce::jlimit (0.0f, 1.0f, norm)); }
    void setVintageMix (float norm) noexcept     { vintageMix.setTargetValue (juce::jlimit (0.0f, 1.0f, norm)); }

    float getInputPeak() const noexcept   { return inputPeak.load(); }
    float getOutputPeak() const noexcept  { return outputPeak.load(); }
    float getInputRms() const noexcept    { return inputRms.load(); }
    float getOutputRms() const noexcept   { return outputRms.load(); }
    int getBladeCount() const noexcept    { return bladeCount; }
    int getLatencySamples() const noexcept { return latencySamples; }
    float getFanHz() const noexcept
    {
        return bladePassHzFromSpeed (speedNorm.getCurrentValue());
    }

    const PitchDisplayState& getPitchDisplayState() const noexcept
    {
        return pitchDetector.getDisplayState();
    }

    void process (juce::AudioBuffer<float>& buffer)
    {
        const int numSamples = buffer.getNumSamples();
        const int channels = juce::jmin (buffer.getNumChannels(), 2);

        if (channels <= 0 || numSamples <= 0)
            return;

        updateToneFilters();

        if (monoScratch.size() < (size_t) numSamples)
            monoScratch.resize ((size_t) numSamples);

        float* mono = monoScratch.data();
        float blockInputPeak = 0.0f;
        double blockInputSq = 0.0;

        for (int i = 0; i < numSamples; ++i)
        {
            float s = 0.0f;

            for (int ch = 0; ch < channels; ++ch)
                s += buffer.getSample (ch, i);

            mono[i] = s / (float) channels;
            blockInputPeak = juce::jmax (blockInputPeak, std::abs (mono[i]));
            blockInputSq += (double) mono[i] * (double) mono[i];
        }

        pitchDetector.pushBlock (mono, numSamples);
        pitchDetector.processDecimated (numSamples, 75.0f, 1100.0f);
        const float pitchHz = pitchDetector.getFrequency();
        const float pitchConf = pitchDetector.getConfidence();
        const float vibratoBlend = pitchDetector.getVibratoBlend();

        if (pitchHz > 0.0f && pitchConf > 0.10f)
        {
            detectedHz = pitchHz;

            if (smoothedPitchHz <= 0.0f)
                smoothedPitchHz = pitchHz;
            else
                smoothedPitchHz += pitchSmoothCoeff * (pitchHz - smoothedPitchHz);
        }
        else
        {
            smoothedPitchHz *= 0.9992f;
        }

        const float motorInc = juce::MathConstants<float>::twoPi * 120.0f / (float) sampleRate;
        const float wobbleInc = juce::MathConstants<float>::twoPi * 0.28f / (float) sampleRate;

        for (int ch = 0; ch < channels; ++ch)
        {
            if (dryScratch[ch].size() < (size_t) numSamples)
                dryScratch[ch].resize ((size_t) numSamples);
        }

        float blockOutputPeak = 0.0f;
        double blockOutputSq = 0.0;

        for (int i = 0; i < numSamples; ++i)
        {
            const float speed = speedNorm.getNextValue();
            const float depth = fanDepth.getNextValue();
            const float dist = distanceNorm.getNextValue();
            const float correction = pitchCorrection.getNextValue();
            const float retune = retuneSpeed.getNextValue();
            const float glide = glideAmount.getNextValue();
            const float driveAmt = drive.getNextValue();
            const float mixAmt = mix.getNextValue();
            const float inGain = inputGain.getNextValue();
            const float outGain = outputGain.getNextValue();
            const float spreadAmt = spread.getNextValue();
            const float humanize = humanizeAmount.getNextValue();
            toneNorm.getNextValue();
            const float blend = effectBlend.getNextValue();
            const PositionWeights posWeights = PositionWeights::fromDistance (dist);
            const auto amCoeffs = MeasuredAMEnvelope::getCoeffs (posWeights, depth, speed);
            const auto mixCoeffs = DualPathMixCoeffs::fromPosition (posWeights);
            const float hfDepthOffset = HarmonicChop::getHfDepthOffset (posWeights);
            const float rotationHz = rotationHzFromSpeed (speed);
            const float phaseInc = juce::MathConstants<float>::twoPi * rotationHz / (float) sampleRate;

            const float retuneMs = juce::jmap (retune, 0.0f, 1.0f, 90.0f, 2.0f);
            const float retuneCoeff = 1.0f - std::exp (-1.0f / (retuneMs * 0.001f * (float) sampleRate));
            const float flutterCoeff = juce::jmin (0.35f, retuneCoeff * 2.2f);

            float targetTuneRatio = 1.0f;
            float formantPreserve = 0.0f;
            const float pitchForTune = smoothedPitchHz > 0.0f ? smoothedPitchHz : detectedHz;

            if (pitchForTune > 0.0f && correction > 0.001f)
            {
                const float midi = midiFromFrequency (pitchForTune);
                const float snapped = snapMidiToScale (midi, keyRoot, scaleMask);

                if (glideMidi < 0.0f)
                    glideMidi = snapped;

                if (glide < 0.01f)
                {
                    glideMidi = snapped;
                }
                else
                {
                    const float glideMs = juce::jmap (glide, 0.0f, 1.0f, 12.0f, 220.0f);
                    const float glideCoeff = 1.0f - std::exp (-1.0f / (glideMs * 0.001f * (float) sampleRate));
                    glideMidi += glideCoeff * (snapped - glideMidi);
                }

                const float confBlend = juce::jlimit (0.25f, 1.0f, (0.35f + pitchConf * 0.65f) * vibratoBlend);

                if (std::abs (snapped - lastSnappedMidi) > 0.45f)
                {
                    humanizeTarget = (humanizeRng.nextFloat() * 2.0f - 1.0f) * humanize * 28.0f;
                    lastSnappedMidi = snapped;
                }

                humanizeCurrent += humanizeSmoothCoeff * (humanizeTarget - humanizeCurrent);
                const float correctionCents = (glideMidi - midi) * 100.0f * correction * confBlend + humanizeCurrent;
                targetTuneRatio = std::pow (2.0f, correctionCents / 1200.0f);
                formantPreserve = juce::jlimit (0.55f, 0.95f, 0.55f + correction * 0.40f);
            }
            else
            {
                glideMidi = -1.0f;
            }

            for (int ch = 0; ch < channels; ++ch)
            {
                auto* channelData = buffer.getWritePointer (ch);
                const float channelPhase = (ch == 0 ? -1.0f : 1.0f)
                                         * spreadAmt * juce::MathConstants<float>::pi * 0.32f;
                const float detuneCents = (ch == 0 ? -1.0f : 1.0f) * spreadAmt * 4.5f;
                const float detuneRatio = std::pow (2.0f, detuneCents / 1200.0f);

                float drySample = channelData[i] * inGain;
                inputHpState[ch] += inputHpCoeff * (drySample - inputHpState[ch]);
                drySample = drySample - inputHpState[ch];

                const float absIn = std::abs (drySample);
                compEnv[ch] += compCoeff * (absIn - compEnv[ch]);
                const float compGain = compEnv[ch] > 0.55f
                    ? 1.0f / (1.0f + (compEnv[ch] - 0.55f) * 2.8f)
                    : 1.0f;
                drySample *= compGain;

                dryScratch[ch][(size_t) i] = drySample;
                const float rotPhaseForCh = fanState.rotationPhase + channelPhase;
                const float rawTransmission = amEnvelope.process (rotPhaseForCh, amCoeffs);
                const float transCoeff = rawTransmission > smoothedTransmission[ch] ? transAttackCoeff
                                                                                     : transReleaseCoeff;
                smoothedTransmission[ch] += transCoeff * (rawTransmission - smoothedTransmission[ch]);
                const float transmission = smoothedTransmission[ch];
                const float openBurst = juce::jmax (0.0f, transmission - prevTransmission[ch]);
                prevTransmission[ch] = transmission;

                const float tuneGate = juce::jlimit (0.15f, 1.0f, (transmission - 0.12f) / 0.88f);
                const float effectiveRetune = retuneCoeff * (0.35f + tuneGate * 0.65f);
                smoothedTuneRatio[ch] += effectiveRetune * (targetTuneRatio * detuneRatio - smoothedTuneRatio[ch]);

                formantTune[ch].setPitchRatio (smoothedTuneRatio[ch]);
                formantTune[ch].setFormantPreserve (formantPreserve);

                const float dopplerTarget = fanState.dopplerRatio (depth, dist, bladeCount, channelPhase);

                smoothedFlutterRatio[ch] += flutterCoeff * (dopplerTarget - smoothedFlutterRatio[ch]);
                flutterShifter[ch].setRatio (smoothedFlutterRatio[ch]);

                float x = drySample;
                x = flutterShifter[ch].processSample (x);
                x = formantTune[ch].processSample (x);

                chopLpState[ch] += chopLpCoeff * (x - chopLpState[ch]);
                const float lowBand = chopLpState[ch];
                const float highBand = x - lowBand;
                const float lowProcessed = HarmonicChop::processBand (lowBand, transmission, hfDepthOffset, false);
                const float highProcessed = HarmonicChop::processBand (highBand, transmission, hfDepthOffset, true);
                float chopped = lowProcessed + highProcessed;
                const float bodyBleed = (1.0f - posWeights.grille) * 0.028f * (1.0f - transmission);
                x = drySample * mixCoeffs.kDirect + chopped * mixCoeffs.kBlade + drySample * bodyBleed;

                const float reflectMix = fanState.reflectionAmount (depth, dist, transmission);
                x = reflection[ch].process (x, reflectMix);

                const float roomAmt = fanState.roomTailAmount (depth, dist, transmission, openBurst);
                x = roomTail[ch].process (x, roomAmt);

                {
                    const float cFreq = combFreq.getNextValue();
                    const float cDepth = combDepth.getNextValue();
                    const float cFB = combFeedback.getNextValue();
                    const float cDamp = combDamping.getNextValue();
                    housingComb[ch].setFreq (cFreq);
                    housingComb[ch].setDamping (cDamp);
                    x = housingComb[ch].processSample (x, cDepth, cFB);
                }

                const float cutoff = fanState.airCutoffHz (depth, dist, transmission);
                const float lpCoeff = 1.0f - std::exp (-2.0f * juce::MathConstants<float>::pi * cutoff / (float) sampleRate);
                airLpState[ch] += lpCoeff * (x - airLpState[ch]);
                x = airLpState[ch];

                float* tonePtr = &x;
                juce::dsp::AudioBlock<float> toneBlock (&tonePtr, 1, 1);
                juce::dsp::ProcessContextReplacing<float> toneCtx (toneBlock);
                toneFilter[ch].process (toneCtx);

                {
                    const float vRate = vintageRate.getNextValue();
                    const float vDepth = vintageDepth.getNextValue();
                    const float vFB = vintageFB.getNextValue();
                    const float vMix = vintageMix.getNextValue();
                    vintageTune[ch].setRate (vRate);
                    vintageTune[ch].setDepth (vDepth);
                    x = vintageTune[ch].processSample (x, vFB, vMix);
                }

                const float turbNoise = fanState.turbulenceNoise (depth, dist, transmission) * (0.22f + driveAmt * 0.40f);
                if (turbNoise > 0.001f)
                {
                    const float noiseCutoff = 1000.0f + (1.0f - dist) * 2200.0f;
                    const float noiseCoeff = 1.0f - std::exp (-2.0f * juce::MathConstants<float>::pi * noiseCutoff / (float) sampleRate);
                    const float air = pinkNoise.processSample() * turbNoise;
                    noiseLpState[ch] += noiseCoeff * (air - noiseLpState[ch]);
                    x += noiseLpState[ch] + motorHum.process (speed, dist, depth);
                }

                if (driveAmt > 0.001f)
                {
                    const float driveGain = 1.0f + driveAmt * 2.8f;
                    const float driven = x * driveGain;
                    x = driven / (1.0f + std::abs (driven) * (0.50f + driveAmt * 0.30f));
                }

                float* polishPtr = &x;
                juce::dsp::AudioBlock<float> polishBlock (&polishPtr, 1, 1);
                juce::dsp::ProcessContextReplacing<float> polishCtx (polishBlock);
                polishFilter[ch].process (polishCtx);

                dcBlockState[ch] += dcBlockCoeff * (x - dcBlockState[ch]);
                x -= dcBlockState[ch];

                const float delayedDry = dryDelay[ch].process (drySample);
                const float wet = x;
                const float mixed = (delayedDry * (1.0f - mixAmt) + wet * mixAmt) * outGain;
                const float bypassDry = drySample * outGain;
                channelData[i] = bypassDry + (mixed - bypassDry) * blend;
                blockOutputPeak = juce::jmax (blockOutputPeak, std::abs (channelData[i]));
                blockOutputSq += (double) channelData[i] * (double) channelData[i];
            }

            if (channels == 2)
            {
                auto* left = buffer.getWritePointer (0);
                auto* right = buffer.getWritePointer (1);
                const float mid = (left[i] + right[i]) * 0.5f;
                float side = (left[i] - right[i]) * 0.5f;
                side *= 1.0f + spreadAmt * 1.15f;
                left[i] = mid + side;
                right[i] = mid - side;

                const float peak = juce::jmax (std::abs (left[i]), std::abs (right[i]));
                if (peak > 0.92f)
                {
                    const float soft = 0.92f + (peak - 0.92f) / (1.0f + (peak - 0.92f) * 6.0f);
                    const float gain = soft / peak;
                    left[i] *= gain;
                    right[i] *= gain;
                }
            }

            fanState.rotationPhase += phaseInc;
            fanState.wobblePhase += wobbleInc;
            fanState.motorPhase += motorInc;

            if (fanState.rotationPhase > juce::MathConstants<float>::twoPi)
                fanState.rotationPhase -= juce::MathConstants<float>::twoPi;

            if (fanState.wobblePhase > juce::MathConstants<float>::twoPi)
                fanState.wobblePhase -= juce::MathConstants<float>::twoPi;

            if (fanState.motorPhase > juce::MathConstants<float>::twoPi)
                fanState.motorPhase -= juce::MathConstants<float>::twoPi;
        }

        inputPeak.store (blockInputPeak * inputGain.getCurrentValue());
        outputPeak.store (blockOutputPeak);
        inputRms.store ((float) std::sqrt (blockInputSq / (double) juce::jmax (1, numSamples)));
        outputRms.store ((float) std::sqrt (blockOutputSq / (double) juce::jmax (1, numSamples)));
    }

private:
    void initSmoothers (double sr)
    {
        const double ramp = 0.02;
        speedNorm.reset (sr, ramp);
        pitchCorrection.reset (sr, ramp);
        retuneSpeed.reset (sr, ramp);
        glideAmount.reset (sr, ramp);
        fanDepth.reset (sr, ramp);
        toneNorm.reset (sr, ramp);
        drive.reset (sr, ramp);
        spread.reset (sr, ramp);
        distanceNorm.reset (sr, ramp);
        inputGain.reset (sr, ramp);
        outputGain.reset (sr, ramp);
        mix.reset (sr, ramp);
        effectBlend.reset (sr, 0.012);
        humanizeAmount.reset (sr, ramp);
        combFreq.reset (sr, ramp);
        combDepth.reset (sr, ramp);
        combFeedback.reset (sr, ramp);
        combDamping.reset (sr, ramp);
        vintageRate.reset (sr, ramp);
        vintageDepth.reset (sr, ramp);
        vintageFB.reset (sr, ramp);
        vintageMix.reset (sr, ramp);

        speedNorm.setCurrentAndTargetValue (0.62f);
        pitchCorrection.setCurrentAndTargetValue (0.86f);
        retuneSpeed.setCurrentAndTargetValue (0.78f);
        glideAmount.setCurrentAndTargetValue (0.32f);
        fanDepth.setCurrentAndTargetValue (0.68f);
        distanceNorm.setCurrentAndTargetValue (0.58f);
        toneNorm.setCurrentAndTargetValue (0.56f);
        drive.setCurrentAndTargetValue (0.28f);
        spread.setCurrentAndTargetValue (0.52f);
        inputGain.setCurrentAndTargetValue (1.0f);
        outputGain.setCurrentAndTargetValue (1.0f);
        mix.setCurrentAndTargetValue (1.0f);
        effectBlend.setCurrentAndTargetValue (1.0f);
        humanizeAmount.setCurrentAndTargetValue (0.18f);
        combFreq.setCurrentAndTargetValue (0.35f);
        combDepth.setCurrentAndTargetValue (0.0f);
        combFeedback.setCurrentAndTargetValue (0.0f);
        combDamping.setCurrentAndTargetValue (0.5f);
        vintageRate.setCurrentAndTargetValue (0.3f);
        vintageDepth.setCurrentAndTargetValue (0.0f);
        vintageFB.setCurrentAndTargetValue (0.0f);
        vintageMix.setCurrentAndTargetValue (0.0f);

        humanizeSmoothCoeff = 1.0f - std::exp (-1.0f / (0.14f * (float) sr));
        updateLatencySamples();
    }

    void updateLatencySamples() noexcept
    {
        latencySamples = dryDelay[0].getDelaySamples();
    }

    void updateToneFilters()
    {
        const float norm = toneNorm.getTargetValue();
        const float tilt = norm * 2.0f - 1.0f;
        const float shelfFreq = 2800.0f;
        const float shelfGainDb = tilt * 9.0f;
        const float q = 0.55f;

        auto toneCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighShelf (
            sampleRate, shelfFreq, q, juce::Decibels::decibelsToGain (shelfGainDb));

        auto polishCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass (
            sampleRate, 13500.0f, 0.62f);

        for (int ch = 0; ch < 2; ++ch)
        {
            *toneFilter[ch].coefficients = *toneCoeffs;
            *polishFilter[ch].coefficients = *polishCoeffs;
        }
    }

    double sampleRate = 48000.0;
    int numChannels = 2;
    int maxBlockSize = 512;
    int latencySamples = 0;

    VocalPitchDetector pitchDetector;
    PitchShifter flutterShifter[2];
    FormantPreservingPitchShifter formantTune[2];
    juce::dsp::IIR::Filter<float> toneFilter[2];
    juce::dsp::IIR::Filter<float> polishFilter[2];
    MeasuredAMEnvelope amEnvelope;
    PinkNoiseGenerator pinkNoise;
    MotorHumGenerator motorHum;
    BladeReflection reflection[2];
    RoomTail roomTail[2];
    DryDelayLine dryDelay[2];
    HousingComb housingComb[2];
    VintageTune vintageTune[2];
    FanSampleState fanState;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> speedNorm;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> pitchCorrection;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> retuneSpeed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> glideAmount;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> fanDepth;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> distanceNorm;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> toneNorm;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> drive;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> spread;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> inputGain;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> outputGain;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mix;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> humanizeAmount;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> effectBlend;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> combFreq;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> combDepth;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> combFeedback;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> combDamping;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> vintageRate;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> vintageDepth;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> vintageFB;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> vintageMix;

    std::vector<float> monoScratch;
    std::vector<float> dryScratch[2];

    float detectedHz = 0.0f;
    float smoothedPitchHz = 0.0f;
    float glideMidi = -1.0f;
    float smoothedTuneRatio[2] { 1.0f, 1.0f };
    float smoothedFlutterRatio[2] { 1.0f, 1.0f };
    float smoothedTransmission[2] { 1.0f, 1.0f };
    float prevTransmission[2] { 1.0f, 1.0f };
    float airLpState[2] {};
    float chopLpState[2] {};
    float noiseLpState[2] {};
    float inputHpState[2] {};
    float dcBlockState[2] {};
    float compEnv[2] {};
    float inputHpCoeff = 0.01f;
    float chopLpCoeff = 0.01f;
    float pitchSmoothCoeff = 0.01f;
    float transAttackCoeff = 0.1f;
    float transReleaseCoeff = 0.02f;
    float dcBlockCoeff = 0.01f;
    float compCoeff = 0.01f;
    float humanizeSmoothCoeff = 0.01f;
    float humanizeCurrent = 0.0f;
    float humanizeTarget = 0.0f;
    float lastSnappedMidi = -100.0f;
    juce::Random humanizeRng;

    std::atomic<float> inputPeak { 0.0f };
    std::atomic<float> outputPeak { 0.0f };
    std::atomic<float> inputRms { 0.0f };
    std::atomic<float> outputRms { 0.0f };

    int keyRoot = 0;
    int scaleMask = kScaleChromatic;
    int bladeCount = 3;
};

} // namespace fantune::dsp