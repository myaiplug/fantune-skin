#pragma once

#include <juce_dsp/juce_dsp.h>
#include <cmath>

namespace fantune::dsp
{

// ── Position modes ────────────────────────────────────────────────────────────

enum class PositionMode : int
{
    Grille = 0,
    Near   = 1,
    Behind = 2
};

struct PositionWeights
{
    float grille = 0.0f;
    float near   = 0.0f;
    float behind = 0.0f;

    static PositionWeights fromDistance (float distance) noexcept
    {
        PositionWeights w;

        if (distance >= 0.66f)
        {
            w.grille = (distance - 0.66f) / 0.34f;
            w.near   = 1.0f - w.grille;
        }
        else if (distance >= 0.33f)
        {
            w.near   = (distance - 0.33f) / 0.33f;
            w.behind = 1.0f - w.near;
        }
        else
        {
            w.behind = 1.0f;
        }

        return w;
    }

    PositionMode dominantMode() const noexcept
    {
        if (grille >= near && grille >= behind) return PositionMode::Grille;
        if (near   >= grille && near   >= behind) return PositionMode::Near;
        return PositionMode::Behind;
    }
};

// ── MeasuredAMEnvelope ─────────────────────────────────────────────────────────
//
// Asymmetric amplitude modulation envelope derived from measured fan profiles.
// Uses a dual-Gaussian per-blade occlusion model:
//   - attack side (blade entering) has narrow sigma = sharp edge cut
//   - release side (blade leaving) has broad sigma = rounded exit
// Total transmission = 1 - max occlusion across all blades.
// This produces the "bits of voice removed" sound, NOT smooth tremolo.

class MeasuredAMEnvelope
{
public:
    struct ProfileCoeffs
    {
        float depthDb     = 18.0f;
        float dutyCycle   = 0.42f;
        float attackSigma = 0.22f;
        float releaseSigma = 0.52f;
    };

    void prepare (double sampleRate) noexcept
    {
        sr = sampleRate;
    }

    static ProfileCoeffs getCoeffs (const PositionWeights& w, float widthNorm, float speedNorm) noexcept
    {
        static constexpr ProfileCoeffs kGrille { 30.0f, 0.38f, 0.22f, 0.58f };
        static constexpr ProfileCoeffs kNear   { 22.0f, 0.42f, 0.28f, 0.50f };
        static constexpr ProfileCoeffs kBehind { 16.0f, 0.46f, 0.34f, 0.44f };

        ProfileCoeffs c;
        c.depthDb      = w.grille * kGrille.depthDb   + w.near * kNear.depthDb   + w.behind * kBehind.depthDb;
        c.dutyCycle    = w.grille * kGrille.dutyCycle  + w.near * kNear.dutyCycle + w.behind * kBehind.dutyCycle;
        c.attackSigma  = w.grille * kGrille.attackSigma + w.near * kNear.attackSigma + w.behind * kBehind.attackSigma;
        c.releaseSigma = w.grille * kGrille.releaseSigma + w.near * kNear.releaseSigma + w.behind * kBehind.releaseSigma;

        // Width maps primarily to AM depth
        c.depthDb *= (0.30f + widthNorm * 0.70f);

        // Speed modulates duty cycle slightly (slower = wider occlusion)
        c.dutyCycle *= (0.92f + speedNorm * 0.08f);

        // Clamp to physically valid ranges
        c.depthDb      = juce::jlimit (3.0f,  42.0f, c.depthDb);
        c.dutyCycle    = juce::jlimit (0.20f, 0.65f, c.dutyCycle);
        c.attackSigma  = juce::jlimit (0.08f, 0.50f, c.attackSigma);
        c.releaseSigma = juce::jlimit (0.12f, 0.65f, c.releaseSigma);

        return c;
    }

    // Returns transmission gain [0, 1] for the current rotation phase.
    // 0 = fully blocked, 1 = fully open.
    float process (float rotationPhase, const ProfileCoeffs& c) const noexcept
    {
        float obstruction = 0.0f;
        constexpr float bladeSpacing = juce::MathConstants<float>::twoPi / 3.0f;

        for (int b = 0; b < 3; ++b)
        {
            float bladeCenter = (float) b * bladeSpacing;
            float diff = rotationPhase - bladeCenter;
            diff = std::atan2 (std::sin (diff), std::cos (diff));
            float absDiff = std::abs (diff);

            // Asymmetric Gaussian: different sigma on attack vs release side
            float sigma = diff < 0.0f ? c.attackSigma : c.releaseSigma;
            float occlusion = std::exp (-0.5f * (absDiff * absDiff) / (sigma * sigma));

            obstruction = juce::jmax (obstruction, occlusion);
        }

        float depthLin = juce::Decibels::decibelsToGain (-c.depthDb);
        float transmission = 1.0f - obstruction * (1.0f - depthLin);

        return juce::jlimit (depthLin, 1.0f, transmission);
    }

private:
    double sr = 48000.0;
};

// ── DualPathFanMixer ───────────────────────────────────────────────────────────
//
// Models two acoustic paths:
//   Direct path:  sound reaching ear without fan interaction (bone conduction + body)
//   Blade path:   sound reflected/scattered by fan blades (strongly amplitude-modulated)
//
// At GRILLE: blade path dominates (kBlade ≈ 0.85, kDirect ≈ 0.15)
// At BEHIND: both paths contribute (kBlade ≈ 0.65, kDirect ≈ 0.35)

struct DualPathMixCoeffs
{
    float kDirect = 0.0f;
    float kBlade  = 0.0f;
    float gap     = 0.0f;  // 1 - kDirect - kBlade (scattering loss)

    static DualPathMixCoeffs fromPosition (const PositionWeights& w) noexcept
    {
        static constexpr float kDirectGrille = 0.12f;
        static constexpr float kBladeGrille  = 0.88f;
        static constexpr float kDirectNear   = 0.22f;
        static constexpr float kBladeNear    = 0.74f;
        static constexpr float kDirectBehind = 0.32f;
        static constexpr float kBladeBehind  = 0.62f;

        DualPathMixCoeffs m;
        m.kDirect = w.grille * kDirectGrille + w.near * kDirectNear + w.behind * kDirectBehind;
        m.kBlade  = w.grille * kBladeGrille  + w.near * kBladeNear  + w.behind * kBladeBehind;
        m.gap     = 1.0f - m.kDirect - m.kBlade;
        m.gap     = juce::jmax (0.0f, m.gap);
        return m;
    }

    float mix (float directSample, float bladeSample, float transmission) const noexcept
    {
        return directSample * kDirect + bladeSample * kBlade * transmission;
    }
};

// ── HarmonicChop ────────────────────────────────────────────────────────────────
//
// HF content (> 820 Hz) is attenuated MORE than LF content during blade close.
// This models the physical observation that higher frequencies scatter more
// and are blocked more effectively by the blade edge.
//
// The band-split at 820 Hz already exists in FanTuneEngine. This class adds
// frequency-dependent depth scaling.

class HarmonicChop
{
public:
    void prepare() noexcept {}

    // chopDepthOffset: additional dB of depth applied to high band (negative = more chop)
    static float getHfDepthOffset (const PositionWeights& w) noexcept
    {
        // Grille: HF chops ~8 dB harder than LF
        // Behind: HF chops ~3 dB harder
        return -(w.grille * 8.0f + w.near * 5.5f + w.behind * 3.0f);
    }

    // Apply frequency-dependent transmission
    static float processBand (float sample, float transmission, float hfDepthOffsetDb, bool isHighBand) noexcept
    {
        if (! isHighBand)
            return sample * transmission;

        // HF band: compute effective transmission with additional depth
        float effectiveTrans = transmission;
        if (hfDepthOffsetDb < 0.0f)
        {
            float offsetLin = juce::Decibels::decibelsToGain (hfDepthOffsetDb);
            float hfTrans = transmission * offsetLin;
            effectiveTrans = hfTrans;
        }

        return sample * effectiveTrans;
    }
};

} // namespace fantune::dsp
