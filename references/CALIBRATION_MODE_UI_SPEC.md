# FanTune — In-Plugin Calibration Mode UI Spec

## Purpose

Provide visual feedback panels that show the engineer exactly what the chop
engine is doing — AM envelope shape, BPF rate, duty cycle, dual-path mix ratio,
and comb structure — without needing external analyzers.

## Activation

- **Key command:** `Ctrl+Shift+C` (Windows), `Cmd+Shift+C` (Mac)
- **Button:** "CAL" button in plugin header, right of "A/B" button (only visible
  when developing; gated by `#ifdef FANTUNE_DEVELOPER_MODE`)
- **Exit:** Same key or close button in corner; returns to normal UI

---

## Metering Panels (developers only, no end-user build)

### 1. AM Envelope Scope

**Where:** Bottom-right quadrant, replacing the "fan guard" graphic.
**Size:** 320 × 180 px
**Type:** Real-time waveform + overlaid envelope trace.

| Element | Description |
|---------|-------------|
| Grid | 3 horizontal lines: 0%, 50%, 100% amplitude. Light gray, 1 px. |
| Waveform trace | 1 blade period windowed from scratch buffer. Blue (left), cyan (right). |
| Envelope overlay | Rectified + 40 Hz LPF envelope. Yellow, 2 px, bold. |
| Period markers | Vertical dashed red lines at blade-open peak and blade-close trough. |
| BPF readout | Top-left of scope: `BPF: 36.2 Hz` (from PLL, not display-only) |
| Duty readout | Top-right: `Duty: 0.42` (fraction where envelope > 50%) |
| Depth readout | Below duty: `Depth: 18.3 dB` |

**Update rate:** Every 64 samples for waveform, every analysis hop for envelope stats.

---

### 2. Dual-Path Mixer

**Where:** Below AM Scope, same width.
**Size:** 320 × 60 px
**Type:** Horizontal stacked bar.

```
┌──────────────────────────────────────────────┐
│ Direct  ████████░░░░░░░░░░░░░░░░ 24%        │
│ Blade   ████████████████████████░░ 76%       │
└──────────────────────────────────────────────┘
```

Blade path bar rendered in orange gradient. Direct path in gray.
Gap percentage printed at right (16% in above example — always sums to 100 + gap).

The **gap** = 1 - (direct + blade) at blade-trough: this is the energy lost to
obstruction/reflection/diffusion. Display as gray gap.

**Update rate:** Once per blade period.

---

### 3. Comb Filter Magnitude

**Where:** Right panel, 240 × 160 px.
**Type:** Log-frequency magnitude plot.

| Element | Description |
|---------|-------------|
| X-axis | 100 Hz – 10 kHz, log scale |
| Y-axis | −24 to 0 dB |
| Live line | Current comb response from HousingCombFilter transfer function. Green. |
| Reference dots | Gray dots at measured tap positions from loaded profile. |
| Dip markers | Inverted triangle at each notch center. |

**Update rate:** On profile load + every 1024 samples.

---

### 4. Blade Pass Modulation Heatmap

**Where:** Below comb, 240 × 120 px.
**Type:** 2D colormap: frequency (Y) × time (X), one blade period.

| Element | Description |
|---------|-------------|
| X-axis | 0 to 1 blade period |
| Y-axis | 100 Hz – 8 kHz, log scale |
| Color | Blue (open, 0 dB) → red (closed, −24 dB) via `viridis` palette |

Shows exactly which frequency bands get cut hardest during blade close.
Should visually confirm HF attenuates more than LF.

**Update rate:** Every 2 blade periods (moving window).

---

### 5. Sidebar: Profile Status

**Where:** Far left, 120 px wide.
**Type:** Vertical info list.

| Field | Source | Example |
|-------|--------|---------|
| Profile loaded | `FanTuneProfile.json` filename | `lasko20_med_behind.json` |
| Geometry | profile_meta.geometry | `behind` |
| Speed | profile_meta.speed_caption | `med` |
| BPF target | bpf.measured_hz | `36.2 Hz` |
| BPF actual | PLL output | `36.0 Hz` |
| Sync status | match ±2 Hz? | ✅ or ❌ |
| Blade count | profile_meta.blade_count | `3` |

---

## Interaction

| Action | Result |
|--------|--------|
| Click on AM scope | Toggle freeze/hold on current period |
| Click on Comb plot | Cycle through profile slots (load next profile) |
| Right-click on any meter | Copy live value to clipboard for logging |
| Drag on BPF readout | Override BPF target (calibration override) |
| Esc | Exit calibration mode |

---

## Implementation Notes

- All scope/meter rendering in `FanTuneUIComponents.h` with `juce::Graphics`.
- Meter data pushed from `FanTuneEngine` via atomic structs in `FanTuneCalibrationData.h`.
- Profile loading via `FanTuneProfileLoader` class (header + cpp, new files).
- No performance impact when calibrate mode is off (meters skip update).
- `#ifdef FANTUNE_DEVELOPER_MODE` wraps calibration UI; stripped from release builds.

## Files Affected

| File | Change |
|------|--------|
| Source/FanTuneCalibrationData.h | NEW — atomic meter structs |
| Source/FanTuneCalibrationScope.h | NEW — scope component |
| Source/FanTuneProfileLoader.h | NEW — profile JSON loader |
| Source/FanTuneProfileLoader.cpp | NEW — JSON parsing |
| Source/FanTuneDSP.h | Add calibration data to FanTuneEngine, push stats on process() |
| Source/PluginEditor.cpp | Add calibrate button, toggle scope visibility |
| CMakeLists.txt | Add new .cpp to target_sources |
