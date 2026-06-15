# FanTune — Scoring Metrics & Regression

Five objective metrics validate how faithfully the DSP engine reproduces a
captured reference profile. The **overall_score** is a weighted composite; each
sub-score is 0 (fail) to 1 (perfect).

---

## 1. Envelope RMSE (`envelope_rmse`)

**Weight: 30%**

- Extract amplitude envelope of both reference and model via full-wave rectification + 40 Hz LP.
- Normalize each to [0, 1].
- RMSE = sqrt(mean((ref - model)^2)).
- Mapped to [0, 1] via `score = 1.0 - min(rmse * 4, 1.0)`.
- Penalty factor 4x amplifies small errors — an RMSE of 0.1 → score 0.6, 0.25 → 0.0.

**What this catches:** Wrong AM depth, wrong duty cycle, wrong asymmetry, wrong BPF rate,
or envelope smoothing/shape errors. If the blade-open window is too wide or chop too shallow,
this score drops.

---

## 2. Spectral AM Correlation (`spectral_am_correlation`)

**Weight: 25%**

- Compute narrowband spectrograms (1024-sample FFT, 50% overlap, 48 kHz).
- Flatten to vectors, compute Pearson correlation coefficient.
- Direct value = score (−1 to 1, bounded to 0–1).

**What this catches:** Frequency-dependent chop. If HF chops harder than LF (as it should),
this correlation stays high. If the model applies uniform-depth AM across all frequencies,
the spectral "fingerprint" diverges and correlation drops.

---

## 3. Comb Correlation (`comb_correlation`)

**Weight: 15%**

- Compute autocorrelation of each WAV (first 500 lags = ~10 ms at 48 kHz).
- Pearson correlation between the two autocorrelation sequences.

**What this catches:** Housing resonance and reflection comb structure. If the model
has no comb filtering, or the wrong tap delays, the short-lag autocorrelation diverges.

---

## 4. AM Depth Error (`am_depth_error_db`)

**Weight: 15%**

- `score = max(0, 1.0 - |depth_ref - depth_model| / depth_ref)`.
- A 50% error in depth cuts the score to 0.5; doubling error → 0.0.

**What this catches:** The most audible perceptual parameter — how much volume dips
between blade passes. Under-chop (depth < 6 dB) sounds like tremolo, not fan. Over-chop
(depth > 35 dB) sounds like a mute pedal. This metric ensures the model lands in the
right region.

---

## 5. Duty Cycle Error (`duty_cycle_error`)

**Weight: 15%**

- `score = max(0, 1.0 - |duty_ref - duty_model| * 3)`.
- Penalty factor 3×: a duty cycle error of 0.1 → score 0.7, error of 0.33 → 0.0.

**What this catches:** How long the blade is "closed" vs "open." At GRILLE, duty cycle
should be ~0.35–0.45 (closed ≥ 40% of period). At BEHIND, wider ~0.45–0.60. A tremolo
(sine-wave) modulator always has duty = 0.5, which scores poorly against asymmetric
fan envelopes.

---

## 6. Overall Score (composite)

Automatically computed by `analyze_fan_profile.py --score-*`:

```
overall = 0.30 * envelope_score
        + 0.25 * spectral_am_correlation
        + 0.15 * comb_correlation
        + 0.15 * depth_score
        + 0.15 * duty_score
```

| Range | Interpretation | Action |
|-------|---------------|--------|
| 0.90–1.00 | Perceptually indistinguishable | Ship it |
| 0.80–0.89 | Minor differences under A/B | Tweak if easy |
| 0.65–0.79 | Audibly different but "feels like a fan" | Adjust chop + comb |
| 0.40–0.64 | Wrong envelope shape or depth | Rethink AM profile |
| < 0.40 | Not a fan sound at all | Restart from physics |

---

## Regression Procedure

```bash
# Single file analysis
python tools/analyze_fan_profile.py captures/f_lasko20_g_behind_s2_p01.wav --output profiles/lasko20_med_behind.json

# Offline regression: compare model output to reference
python tools/analyze_fan_profile.py ref.wav \
    --score-ref-wav dry_vocal_di.wav \
    --score-model-wav model_output.wav \
    --score-ref-profile profiles/lasko20_med_behind.json
```

Run regression on all 9 takes + DI source. Track overall_score across iterations.
Target: overall_score ≥ 0.85 for BEHIND geometry, ≥ 0.80 for GRILLE.
