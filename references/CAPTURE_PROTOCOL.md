# FanTune Reference Capture Protocol

## Purpose

Capture authentic 3-blade box fan vocal recordings to extract measurement profiles
for the FanTune DSP model. These WAV files are used by `tools/analyze_fan_profile.py`
to produce `FanTuneProfile.json` files that drive calibration.

## Equipment Requirements

| Item | Minimum | Preferred |
|------|---------|-----------|
| Fan | 3-blade box fan (Lasko, Holmes, Honeywell) | Lasko 20" 3-speed box fan |
| Mic | Cardioid condenser, flat response 40-16k | SM57 / AT2020 |
| Interface | 1 preamp, 48 kHz / 24-bit | Focusrite Scarlett 2i2 |
| Stand | Boom stand, pop filter | Isolated from fan vibration |
| Room | Quiet, no reverb > 0.3s RT60 | Treated corner, away from walls |

## WAV File Naming Convention

```
f_{fan_id}_g_{geometry}_s{speed}_p{phrase_idx}.wav
```

Example: `f_lasko20_g_grille_s2_p01.wav`

- `fan_id`: short slug e.g. `lasko20`, `holmes16`, `generic3blade`
- `geometry`: `grille` | `behind`
- `speed`: `1` (low) | `2` (med) | `3` (high)
- `phrase_idx`: zero-indexed phrase number

## The 9 Required Takes

### Position Geometry

| # | Geometry | Speed | Phrase | Start Mic Pos | Fan State |
|---|----------|-------|--------|---------------|-----------|
| 1 | GRILLE | Low | A | Lips touching front guard | Steady, fan on 30s pre |
| 2 | GRILLE | Med | A | Lips touching front guard | Steady |
| 3 | GRILLE | High | A | Lips touching front guard | Steady |
| 4 | BEHIND | Low | A | 6" from intake side, centered | Steady |
| 5 | BEHIND | Med | A | 6" from intake side, centered | Steady |
| 6 | BEHIND | High | A | 6" from intake side, centered | Steady |
| 7 | GRILLE | Med | B | Same as #2 | Repeat: different phrase |
| 8 | BEHIND | Med | B | Same as #5 | Repeat: different phrase |
| 9 | GRID SWEEP | Med | — | Sweep 0→24" from intake | Continuous, step each 4" |

### Phrase A (vowel + fricative)
```
"AHHH"  (open vowel, sustained 2s at comfortable pitch)
"EEEE"
"OHHH"
"ssss"  (unvoiced sibilant)
"SHAH"  (fricative + vowel)
"la la la la la"  (alternating consonant-vowel rapid)
```

### Phrase B (alternate)
```
"MMM-MMM" (nasal plosives)
"BA BA BA BA" (bilabial plosives through chop)
"FREEZE" (fricative + long vowel)
"A E I O U" each held 1s
"test 1 2 3" (connected speech)
```

### Grid Sweep (Take 9)
```
Position mic at exact distances: 0", 4", 8", 12", 16", 20", 24" from intake side
At each distance: sustain "AHHH" for 2 full seconds, then "PA" burst
3 full blade rotations minimum at each position
```

## Recording Chain Settings

| Parameter | Value |
|-----------|-------|
| Sample rate | 48 kHz |
| Bit depth | 24-bit |
| Format | WAV (PCM) |
| Preamp gain | Unity on vocal peaks (−12 dBFS target) |
| HPF on interface | OFF (capture motor hum) |
| Limiter | OFF |
| Plugin inserts | NONE — record raw | -- no EQ, compression, reverb |

## Metering During Capture

Monitor (do NOT record these averages, but log in notes):
- **Vocal level**: sustained AHH peaks at −12 dBFS ± 3
- **Fan motor noise floor**: measure RMS during silence before each take
- **Distance**: measure from mic diaphragm to nearest fan surface

## Reference Signal — DI Vocal

Also capture one dry DI vocal pass (no fan running) for each phrase at same
distance and gain. File prefix: `f_{fan_id}_g_di_s0_p{phrase_idx}.wav`.

This provides anechoic source material for A/B comparison and profile
differencing. Required: Phrase A and Phrase B, close mic (~3"), flat gain.

## Quality Checklist (before leaving setup)

- [ ] Fan runs minimum 30 seconds before first take (thermal settle)
- [ ] No clipping on any take (peak WAV level < −4 dBFS)
- [ ] Ambient noise floor > 40 dB below vocal peaks
- [ ] Mic shock mount used, no cable touching fan housing
- [ ] Distance recorded in capture log for every take
- [ ] Fan speed confirmed: low = 6 Hz rotation, med = 12 Hz, high = 24 Hz
  (measure with strobe app on phone if available)
- [ ] Both left and right channels identical (mono source, no stereo effects)
- [ ] Take 9 grid sweep: each distance step audible and marked in log

## Expected WAV Output (48 kHz / 24-bit / mono)

```
references/captures/
  f_lasko20_g_grille_s1_p01.wav
  f_lasko20_g_grille_s2_p01.wav
  f_lasko20_g_grille_s3_p01.wav
  f_lasko20_g_behind_s1_p01.wav
  f_lasko20_g_behind_s2_p01.wav
  f_lasko20_g_behind_s3_p01.wav
  f_lasko20_g_grille_s2_p02.wav
  f_lasko20_g_behind_s2_p02.wav
  f_lasko20_g_grid_sweep_s2_p01.wav
  f_lasko20_g_di_s0_p01.wav
  f_lasko20_g_di_s0_p02.wav
```
