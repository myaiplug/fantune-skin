#!/usr/bin/env python3
"""
analyze_fan_profile.py — Extract FanTune profile parameters from reference WAV captures.

Usage:
    python tools/analyze_fan_profile.py path/to/captures/ --output profiles/lasko20_med_behind.json

This script reads a 48 kHz / 24-bit WAV file of a vocalist singing into a 3-blade box fan
and extracts all parameters needed to populate a FanTuneProfile.json.

Output JSON conforms to schema defined in docs/FanTuneProfile-schema.json.
"""

import argparse
import json
import math
import os
import sys
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

import numpy as np
from scipy.io import wavfile
from scipy import signal
from scipy.ndimage import uniform_filter1d


# ── Configuration ──────────────────────────────────────────────────────────
BLADE_COUNT = 3
ROTATION_RANGES_HZ = {"low": (5.5, 7.5), "med": (10, 14), "high": (20, 26)}
BPF_RANGES_HZ = {
    speed: (lo * BLADE_COUNT, hi * BLADE_COUNT)
    for speed, (lo, hi) in ROTATION_RANGES_HZ.items()
}
SAMPLE_RATE = 48000


# ── Helper types ────────────────────────────────────────────────────────────

class EnvelopeStats:
    """AM envelope statistics extracted from a WAV take."""
    def __init__(self) -> None:
        self.bpf_hz: float = 0.0
        self.am_depth_db: float = 0.0
        self.am_duty_cycle: float = 0.0  # fraction of period where envelope > 50% of max
        self.rise_time_ms: float = 0.0    # 10% → 90% rise
        self.fall_time_ms: float = 0.0    # 90% → 10% fall
        self.asymmetry_ratio: float = 1.0  # rise / fall

class CombStats:
    """Housing comb filter parameters."""
    def __init__(self) -> None:
        self.tap_delays_ms: List[float] = []
        self.tap_gains_db: List[float] = []
        self.peak_spacing_hz: float = 0.0
        self.correlation_strength: float = 0.0

class MotorStats:
    """Motor hum and harmonic stack."""
    def __init__(self) -> None:
        self.hum_60_db: float = -60.0
        self.hum_120_db: float = -60.0
        self.hum_bpf_sideband_db: float = -60.0
        self.harmonic_count: int = 0

class SpectralChopStats:
    """Frequency-dependent chop profile."""
    def __init__(self) -> None:
        self.hf_atten_db: float = 0.0  # how much more HF attenuates than LF at blade close
        self.crossover_hz: float = 0.0
        self.low_band_am_depth_db: float = 0.0
        self.high_band_am_depth_db: float = 0.0


# ── Core analysis ───────────────────────────────────────────────────────────

def load_wav(path: str) -> Tuple[np.ndarray, int]:
    """Load a 48 kHz WAV file. Returns (samples, sample_rate)."""
    sr, data = wavfile.read(path)
    assert sr == SAMPLE_RATE, f"Expected {SAMPLE_RATE} Hz, got {sr} Hz"
    if data.ndim == 2:
        data = data.mean(axis=1)  # mono
    if data.dtype == np.int32:
        data = data / np.iinfo(np.int32).max
    elif data.dtype == np.int16:
        data = data / np.iinfo(np.int16).max
    elif data.dtype == np.int24:
        data = data / (2**23 - 1)
    return data.astype(np.float64), sr


def bandpass_filter(samples: np.ndarray, lo_hz: float, hi_hz: float, sr: int) -> np.ndarray:
    """Apply 4th-order Butterworth bandpass."""
    nyq = sr / 2.0
    lo, hi = lo_hz / nyq, hi_hz / nyq
    lo = max(lo, 1e-6)
    hi = min(hi, 0.9999)
    if lo >= hi:
        return np.zeros_like(samples)
    sos = signal.butter(4, [lo, hi], btype="band", output="sos")
    return signal.sosfiltfilt(sos, samples)


def extract_envelope(samples: np.ndarray, sr: int, cutoff_hz: float = 40.0) -> np.ndarray:
    """Full-wave rectify + lowpass to extract AM envelope."""
    rectified = np.abs(samples)
    nyq = sr / 2.0
    cutoff = cutoff_hz / nyq
    sos = signal.butter(2, cutoff, btype="low", output="sos")
    return signal.sosfiltfilt(sos, rectified)


def compute_bpf(samples: np.ndarray, sr: int, lo_hz: float, hi_hz: float) -> float:
    """Find dominant blade-pass frequency via Welch PSD peak in expected range."""
    freqs, psd = signal.welch(samples, sr, nperseg=min(8192, len(samples)))
    mask = (freqs >= lo_hz) & (freqs <= hi_hz)
    if not np.any(mask):
        return 0.0
    peak_idx = np.argmax(psd[mask])
    return freqs[mask][peak_idx]


def compute_am_depth_db(envelope: np.ndarray) -> float:
    """AM depth from envelope: 20*log10(max/min) within each cycle, then average."""
    if np.max(envelope) < 1e-8:
        return 0.0
    env_normalized = envelope / np.max(envelope)
    # Find local troughs and peaks
    diffs = np.diff(np.sign(np.diff(env_normalized)))
    peaks = np.where(diffs < -0.5)[0] + 1
    troughs = np.where(diffs > 0.5)[0] + 1

    if len(peaks) < 2 or len(troughs) < 2:
        return 12.0  # default fallback

    depths_db: List[float] = []
    for p, t in zip(peaks[:-1], troughs[:-1]):
        if p < len(envelope) and t < len(envelope) and envelope[p] > 0 and envelope[t] > 0:
            depth = 20.0 * math.log10(envelope[p] / max(envelope[t], 1e-12))
            depths_db.append(depth)

    if not depths_db:
        return 12.0
    return float(np.median(depths_db))


def compute_duty_cycle(envelope: np.ndarray) -> float:
    """Fraction of blade period where envelope > 50% of max."""
    if np.max(envelope) < 1e-8:
        return 0.5
    threshold = 0.5 * np.max(envelope)
    above = np.sum(envelope > threshold)
    return float(above) / float(len(envelope))


def compute_asymmetry(envelope: np.ndarray, sr: int, bpf_hz: float) -> Tuple[float, float, float]:
    """Rise time, fall time, and asymmetry ratio."""
    if bpf_hz <= 0:
        return 0.0, 0.0, 1.0
    period_samples = int(round(sr / bpf_hz))
    if period_samples < 1:
        return 0.0, 0.0, 1.0

    half_period = period_samples // 2
    if len(envelope) < period_samples * 3:
        return 0.0, 0.0, 1.0

    # Pick middle cycle
    mid = len(envelope) // 2
    start = mid - half_period
    end = mid + half_period
    if start < 0 or end > len(envelope):
        start = 0
        end = min(period_samples * 2, len(envelope))
    if end - start < 2:
        return 0.0, 0.0, 1.0

    cycle = envelope[start:end]
    lo, hi = np.min(cycle), np.max(cycle)
    if hi - lo < 1e-12:
        return 0.0, 0.0, 1.0

    ten_pct = lo + 0.1 * (hi - lo)
    ninety_pct = lo + 0.9 * (hi - lo)

    rising = np.where(cycle >= ninety_pct)[0]
    rise_end = rising[0] if len(rising) > 0 else half_period
    rising_start_candidates = np.where(cycle[:rise_end] <= ten_pct)[0]
    rise_start = rising_start_candidates[-1] if len(rising_start_candidates) > 0 else 0
    rise_ms = (rise_end - rise_start) * 1000.0 / sr

    falling = np.where(cycle[:half_period] <= ten_pct)[0]
    fall_end = falling[0] if len(falling) > 0 else half_period
    falling_start_candidates = np.where(cycle[:fall_end] >= ninety_pct)[0]
    fall_start = falling_start_candidates[-1] if len(falling_start_candidates) > 0 else 0
    fall_ms = (fall_end - fall_start) * 1000.0 / sr

    if fall_ms < 0.01:
        fall_ms = rise_ms  # guard
    asymmetry = rise_ms / fall_ms if fall_ms > 0 else 1.0
    return rise_ms, fall_ms, asymmetry


def compute_comb(samples: np.ndarray, sr: int, bpf_hz: float) -> CombStats:
    """Extract housing comb parameters from autocorrelation peaks."""
    stats = CombStats()

    # Bandpass at vocal midrange (300-3000 Hz) to get comb from housing
    bp = bandpass_filter(samples, 300, 3000, sr)
    if np.std(bp) < 1e-9:
        return stats

    # Autocorrelation
    corr = np.correlate(bp, bp, mode="full")
    corr = corr[len(corr) // 2:]  # take positive lags
    corr = corr / (corr[0] + 1e-12)

    # Find peaks in 0.1-5 ms range (2-50 samples at 48k)
    min_lag = int(sr * 0.0001)  # 0.1 ms
    max_lag = int(sr * 0.005)   # 5 ms
    if max_lag > len(corr):
        max_lag = len(corr) - 1
    if min_lag >= max_lag:
        return stats

    segment = corr[min_lag:max_lag + 1]
    diffs = np.diff(np.sign(np.diff(segment)))
    peak_indices = np.where(diffs < -0.5)[0] + min_lag + 1

    # Filter peaks above 0.15 correlation
    peak_vals = corr[peak_indices]
    significant = peak_indices[peak_vals > 0.15]

    if len(significant) == 0:
        return stats

    # Take up to 8 strongest taps
    tap_vals = corr[significant]
    order = np.argsort(-tap_vals)[:8]
    for idx in order:
        stats.tap_delays_ms.append(float(significant[idx] * 1000.0 / sr))
        stats.tap_gains_db.append(float(20.0 * math.log10(max(tap_vals[idx], 1e-12))))

    # Peak spacing (comb frequency)
    if len(significant) > 1:
        spacing = np.diff(significant.astype(float)) * 1000.0 / sr
        stats.peak_spacing_hz = float(1000.0 / np.mean(spacing))
    elif bpf_hz > 0:
        stats.peak_spacing_hz = bpf_hz

    stats.correlation_strength = float(np.max(tap_vals))
    return stats


def compute_motor(samples: np.ndarray, sr: int, bpf_hz: float) -> MotorStats:
    """Extract motor hum levels at 60 Hz, 120 Hz, and BPF sidebands."""
    stats = MotorStats()

    freqs, psd = signal.welch(samples, sr, nperseg=min(16384, len(samples)))
    psd_db = 10.0 * np.log10(psd + 1e-30)
    noise_floor = float(np.median(psd_db))

    for target_hz, attr in [(60, "hum_60_db"), (120, "hum_120_db")]:
        idx = np.argmin(np.abs(freqs - target_hz))
        if idx > 0 and idx < len(freqs) - 1:
            local_peak = float(np.max(psd_db[max(0, idx-2):min(len(psd_db), idx+3)]))
            setattr(stats, attr, local_peak - noise_floor)
        else:
            setattr(stats, attr, -60.0)

    # BPF sideband energy (BPF ± 60 Hz)
    if bpf_hz > 0:
        sideband_energy = 0.0
        for offset in [-60, 60]:
            target = bpf_hz + offset
            idx = np.argmin(np.abs(freqs - target))
            if 0 < idx < len(freqs) - 1:
                sideband_energy = max(
                    sideband_energy,
                    float(np.max(psd_db[max(0, idx-2):min(len(psd_db), idx+3)]))
                )
        stats.hum_bpf_sideband_db = sideband_energy - noise_floor

    # Count harmonics up to 500 Hz
    harm_count = 0
    for h in range(1, 9):
        target = 60 * h
        if target > 500:
            break
        idx = np.argmin(np.abs(freqs - target))
        if 0 < idx < len(freqs) - 1:
            peak = float(np.max(psd[max(0, idx-2):min(len(psd), idx+3)]))
            if 10.0 * math.log10(peak / (np.median(psd) + 1e-30)) > 6.0:
                harm_count += 1
    stats.harmonic_count = harm_count
    return stats


def compute_spectral_chop(samples: np.ndarray, sr: int, envelope: np.ndarray, bpf_hz: float) -> SpectralChopStats:
    """Compare AM depth in low band (< 820 Hz) vs high band."""
    stats = SpectralChopStats()

    low = bandpass_filter(samples, 72, 820, sr)
    high = bandpass_filter(samples, 820, 12000, sr)

    env_low = extract_envelope(low, sr)
    env_high = extract_envelope(high, sr)
    env_full = extract_envelope(samples, sr)

    stats.low_band_am_depth_db = compute_am_depth_db(env_low)
    stats.high_band_am_depth_db = compute_am_depth_db(env_high)

    # HF during blade close: compare high energy when envelope is low
    threshold_env = np.percentile(env_full, 30)
    high_during_close = high[env_full < threshold_env]
    high_during_open = high[env_full >= np.percentile(env_full, 70)]
    if len(high_during_close) > 0 and len(high_during_open) > 0:
        close_rms = np.sqrt(np.mean(high_during_close ** 2)) + 1e-12
        open_rms = np.sqrt(np.mean(high_during_open ** 2)) + 1e-12
        stats.hf_atten_db = float(20.0 * math.log10(close_rms / open_rms))

    # Vocal-vs-noise crossover: where the BPF fundamental meets vocal formants
    freqs, psd_vocal = signal.welch(samples[samples > np.percentile(np.abs(samples), 80)], sr, nperseg=2048)
    noise = samples[np.abs(samples) < np.percentile(np.abs(samples), 20)]
    if len(noise) > 256:
        _, psd_noise = signal.welch(noise, sr, nperseg=min(2048, len(noise)))
        psd_noise = np.interp(freqs, freqs, psd_noise)
        ratio = 10 * np.log10((psd_vocal + 1e-30) / (psd_noise + 1e-30))
        crossover_indices = np.where(np.diff(np.sign(ratio - 3)))[0]
        if len(crossover_indices) > 0:
            stats.crossover_hz = float(freqs[crossover_indices[0]])
        else:
            stats.crossover_hz = 3000.0

    return stats


def compute_envelope_rmse(ref_envelope: np.ndarray, model_envelope: np.ndarray) -> float:
    """RMSE between two envelopes (normalized)."""
    if len(ref_envelope) != len(model_envelope):
        min_len = min(len(ref_envelope), len(model_envelope))
        ref_envelope = ref_envelope[:min_len]
        model_envelope = model_envelope[:min_len]
    ref_norm = ref_envelope / (np.max(ref_envelope) + 1e-12)
    model_norm = model_envelope / (np.max(model_envelope) + 1e-12)
    return float(np.sqrt(np.mean((ref_norm - model_norm) ** 2)))


def compute_spectral_am_corr(ref_spec: np.ndarray, model_spec: np.ndarray) -> float:
    """Correlation coefficient between reference and model spectrogram AM profiles."""
    if ref_spec.shape != model_spec.shape:
        min_frames = min(ref_spec.shape[1], model_spec.shape[1])
        ref_spec = ref_spec[:, :min_frames]
        model_spec = model_spec[:, :min_frames]
    ref_flat = ref_spec.flatten()
    model_flat = model_spec.flatten()
    if np.std(ref_flat) < 1e-12 or np.std(model_flat) < 1e-12:
        return 0.0
    return float(np.corrcoef(ref_flat, model_flat)[0, 1])


def compute_comb_corr(ref_comb: np.ndarray, model_comb: np.ndarray) -> float:
    """Correlation between reference and model autocorrelation combs."""
    min_len = min(len(ref_comb), len(model_comb))
    ref_comb = ref_comb[:min_len]
    model_comb = model_comb[:min_len]
    if np.std(ref_comb) < 1e-12 or np.std(model_comb) < 1e-12:
        return 0.0
    return float(np.corrcoef(ref_comb, model_comb)[0, 1])


def analyze(wav_path: str, speed_hint: Optional[str] = None) -> Dict[str, Any]:
    """Full analysis pipeline returning a profile dict."""
    samples, sr = load_wav(wav_path)

    # Determine expected BPF range
    if speed_hint and speed_hint in BPF_RANGES_HZ:
        lo_bpf, hi_bpf = BPF_RANGES_HZ[speed_hint]
    else:
        lo_bpf, hi_bpf = 18, 72  # full range for 3-blade

    # 1. Extract full envelope
    env = extract_envelope(samples, sr)

    # 2. BPF detection
    bpf = compute_bpf(samples, sr, lo_bpf, hi_bpf)
    env = extract_envelope(samples, sr)  # re-extract

    env_stats = EnvelopeStats()
    env_stats.bpf_hz = bpf
    env_stats.am_depth_db = compute_am_depth_db(env)
    env_stats.am_duty_cycle = compute_duty_cycle(env)
    env_stats.rise_time_ms, env_stats.fall_time_ms, env_stats.asymmetry_ratio = compute_asymmetry(env, sr, bpf)

    # 3. Comb
    comb = compute_comb(samples, sr, bpf)

    # 4. Motor
    motor = compute_motor(samples, sr, bpf)

    # 5. Spectral chop
    chop = compute_spectral_chop(samples, sr, env, bpf)

    profile: Dict[str, Any] = {
        "profile_meta": {
            "source_file": os.path.basename(wav_path),
            "analysis_timestamp": str(np.datetime64("now")),
            "sample_rate": sr,
            "blade_count": BLADE_COUNT,
        },
        "bpf": {
            "measured_hz": env_stats.bpf_hz,
        },
        "am_envelope": {
            "depth_db": env_stats.am_depth_db,
            "duty_cycle": env_stats.am_duty_cycle,
            "rise_time_ms": round(env_stats.rise_time_ms, 3),
            "fall_time_ms": round(env_stats.fall_time_ms, 3),
            "asymmetry_ratio": round(env_stats.asymmetry_ratio, 3),
        },
        "comb_filter": {
            "tap_delays_ms": [round(t, 4) for t in comb.tap_delays_ms],
            "tap_gains_db": [round(g, 2) for g in comb.tap_gains_db],
            "peak_spacing_hz": round(comb.peak_spacing_hz, 1),
            "correlation_strength": round(comb.correlation_strength, 4),
        },
        "motor_hum": {
            "hum_60_db": round(motor.hum_60_db, 2),
            "hum_120_db": round(motor.hum_120_db, 2),
            "bpf_sideband_db": round(motor.hum_bpf_sideband_db, 2),
            "harmonic_count": motor.harmonic_count,
        },
        "spectral_chop": {
            "hf_attenuation_db": round(chop.hf_atten_db, 2),
            "crossover_hz": round(chop.crossover_hz, 0),
            "low_band_am_depth_db": round(chop.low_band_am_depth_db, 2),
            "high_band_am_depth_db": round(chop.high_band_am_depth_db, 2),
        },
    }

    return profile


def score_model_against_reference(
    ref_profile: Dict[str, Any],
    model_profile: Dict[str, Any],
    ref_wav: str,
    model_wav: str,
) -> Dict[str, float]:
    """Compare a modeled output WAV against a reference capture WAV."""
    ref_samples, sr = load_wav(ref_wav)
    model_samples, _ = load_wav(model_wav)

    ref_env = extract_envelope(ref_samples, sr)
    model_env = extract_envelope(model_samples, sr)
    env_rmse = compute_envelope_rmse(ref_env, model_env)

    # Spectrogram comparison
    nperseg = 1024
    _, _, ref_spec = signal.spectrogram(ref_samples, sr, nperseg=nperseg)
    _, _, model_spec = signal.spectrogram(model_samples, sr, nperseg=nperseg)
    spec_corr = compute_spectral_am_corr(ref_spec, model_spec)

    # Comb comparison
    ref_corr = np.correlate(ref_samples, ref_samples, mode="full")
    ref_corr = ref_corr[len(ref_corr) // 2:]
    model_corr = np.correlate(model_samples, model_samples, mode="full")
    model_corr = model_corr[len(model_corr) // 2:]
    comb_corr_val = compute_comb_corr(ref_corr[:500], model_corr[:500])

    # Per-metric scores
    depth_ref = ref_profile.get("am_envelope", {}).get("depth_db", 12)
    depth_model = model_profile.get("am_envelope", {}).get("depth_db", 12)
    depth_error = abs(depth_ref - depth_model)

    duty_ref = ref_profile.get("am_envelope", {}).get("duty_cycle", 0.5)
    duty_model = model_profile.get("am_envelope", {}).get("duty_cycle", 0.5)
    duty_error = abs(duty_ref - duty_model)

    scores: Dict[str, float] = {
        "envelope_rmse": round(env_rmse, 5),
        "spectral_am_correlation": round(spec_corr, 4),
        "comb_correlation": round(comb_corr_val, 4),
        "am_depth_error_db": round(depth_error, 2),
        "duty_cycle_error": round(duty_error, 4),
        "overall_score": round(
            0.30 * (1.0 - min(env_rmse * 4, 1.0))
            + 0.25 * spec_corr
            + 0.15 * comb_corr_val
            + 0.15 * max(0, 1.0 - depth_error / depth_ref)
            + 0.15 * max(0, 1.0 - duty_error * 3),
            4,
        ),
    }
    return scores


# ── CLI ────────────────────────────────────────────────────────────────────

def main() -> None:
    parser = argparse.ArgumentParser(
        description="Extract FanTune DSP profile from reference WAV captures."
    )
    parser.add_argument("input", help="Path to WAV file or directory of WAVs")
    parser.add_argument("--output", "-o", default=None, help="Output JSON path (single file mode)")
    parser.add_argument("--speed", choices=["low", "med", "high"], default=None, help="Fan speed hint")
    parser.add_argument("--score-ref-wav", default=None, help="Reference WAV for A/B scoring (optional)")
    parser.add_argument("--score-model-wav", default=None, help="Model output WAV for A/B scoring (optional)")
    parser.add_argument("--score-ref-profile", default=None, help="Reference profile JSON for scoring")
    args = parser.parse_args()

    input_path = Path(args.input)

    if input_path.is_file() and input_path.suffix.lower() == ".wav":
        print(f"Analyzing: {input_path}", file=sys.stderr)
        profile = analyze(str(input_path), speed_hint=args.speed)

        if args.score_ref_wav and args.score_model_wav:
            ref_profile = profile
            if args.score_ref_profile:
                with open(args.score_ref_profile) as f:
                    ref_profile = json.load(f)
            model_profile = analyze(str(args.score_model_wav), speed_hint=args.speed)
            scores = score_model_against_reference(ref_profile, model_profile, args.score_ref_wav, args.score_model_wav)
            profile["scoring"] = scores
            print(json.dumps(scores, indent=2), file=sys.stderr)

        if args.output:
            output_path = Path(args.output)
            output_path.parent.mkdir(parents=True, exist_ok=True)
            with open(output_path, "w") as f:
                json.dump(profile, f, indent=2)
            print(f"Wrote: {output_path}", file=sys.stderr)
        else:
            print(json.dumps(profile, indent=2))

    elif input_path.is_dir():
        wav_files = sorted(input_path.glob("*.wav"))
        if not wav_files:
            print("No WAV files found in directory.", file=sys.stderr)
            sys.exit(1)

        results: Dict[str, Any] = {}
        for wf in wav_files:
            speed = None
            for s in ["s1", "s2", "s3"]:
                if s in wf.stem:
                    speed = {"s1": "low", "s2": "med", "s3": "high"}[s]
                    break
            results[wf.name] = analyze(str(wf), speed_hint=speed)

        if args.output:
            output_path = Path(args.output)
            output_path.parent.mkdir(parents=True, exist_ok=True)
            with open(output_path, "w") as f:
                json.dump(results, f, indent=2)
            print(f"Wrote batch: {output_path}", file=sys.stderr)
        else:
            print(json.dumps(results, indent=2))

    else:
        print("Input must be a .wav file or directory containing .wav files.", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
