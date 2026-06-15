#!/usr/bin/env python3
"""
Generate FanTune factory preset .fantune XML files.

Each preset is a JUCE AudioProcessorValueTreeState XML that can be loaded
by FanTune directly. The values match the factory presets in PluginProcessor.cpp.

Usage:
    python tools/generate_factory_presets.py [--output-dir presets/]
"""

import argparse
import math
import os
import xml.etree.ElementTree as ET


# ── JUCE NormalisableRange helpers ──────────────────────────────────────────────

def skew_from_centre(start: float, end: float, centre: float) -> float:
    """Compute JUCE NormalisableRange skew factor from centre value."""
    if not (start < centre < end):
        return 1.0
    normalized_centre = (centre - start) / (end - start)
    if normalized_centre <= 0.0 or normalized_centre >= 1.0:
        return 1.0
    return math.log(0.5) / math.log(normalized_centre)


def convert_to_0to1(value: float, start: float, end: float, skew: float = 1.0) -> float:
    """JUCE NormalisableRange::convertTo0to1()."""
    norm = (value - start) / (end - start)
    if skew != 1.0:
        norm = math.pow(norm, skew)
    return norm


# ── Preset data (copied from PluginProcessor.cpp) ──────────────────────────────

PRESET_NAMES = [
    "Bedroom Box Fan",
    "T-Pain Breeze",
    "Subtle Draft",
    "Box Fan High",
    "Hurricane Box Fan",
]

# Parameter specs: (id, start, end, centre_or_None)
PARAM_SPECS = {
    "speed":    (0.0, 1.0, 0.55),   # skewed
    "pitch":    (0.0, 1.0, None),   # linear
    "retune":   (0.0, 1.0, 0.72),   # skewed
    "glide":    (0.0, 1.0, None),   # linear
    "width":    (0.0, 1.0, None),   # linear
    "distance": (0.0, 1.0, None),   # linear
    "tone":     (0.0, 1.0, None),   # linear
    "drive":    (0.0, 1.0, None),   # linear
    "spread":   (0.0, 1.0, None),   # linear
    "mix":      (0.0, 1.0, None),   # linear
    "humanize": (0.0, 1.0, None),   # linear
    "input":    (-24.0, 12.0, None),  # linear dB
    "output":   (-24.0, 12.0, None),  # linear dB
    "output":   (-24.0, 12.0, None),  # linear dB
}

# Raw preset values from PluginProcessor.cpp kPresetData[]
# Order: speed, pitch, retune, glide, width, distance, tone, drive, spread, mix, key, scale
RAW_PRESETS = [
    (0.62, 0.86, 0.72, 0.28, 0.68, 0.62, 0.56, 0.28, 0.52, 1.0, 0, 0),
    (0.52, 0.98, 0.92, 0.12, 0.62, 0.72, 0.60, 0.38, 0.58, 1.0, 5, 0),
    (0.38, 0.52, 0.45, 0.55, 0.40, 0.35, 0.54, 0.14, 0.30, 0.75, 0, 1),
    (0.78, 0.90, 0.82, 0.22, 0.78, 0.80, 0.52, 0.36, 0.58, 1.0, 2, 0),
    (0.88, 0.94, 0.88, 0.18, 0.82, 0.92, 0.50, 0.42, 0.68, 1.0, 7, 0),
]


def generate_presets(output_dir: str, verbose: bool = False) -> None:
    """Generate .fantune preset XML files."""
    os.makedirs(output_dir, exist_ok=True)

    # Parameter IDs in the same order as kPresetData struct:
    #   speed, pitch, retune, glide, width, distance, tone, drive, spread, mix, key, scale
    # humanize is NOT in factory presets; default is 0.18.
    # input/output default to 0 dB.
    float_ids = ["speed", "pitch", "retune", "glide", "width", "distance",
                 "tone", "drive", "spread", "mix"]

    for idx, name in enumerate(PRESET_NAMES):
        raw = RAW_PRESETS[idx]
        values = {}

        # Map float params (indices 0-9)
        for j, pid in enumerate(float_ids):
            spec = PARAM_SPECS[pid]
            if spec[2] is not None:  # skewed
                skew = skew_from_centre(spec[0], spec[1], spec[2])
            else:
                skew = 1.0
            values[pid] = convert_to_0to1(raw[j], spec[0], spec[1], skew)

        # humanize: default factory value
        values["humanize"] = 0.18

        # input / output: default 0 dB
        for pid in ["input", "output"]:
            spec = PARAM_SPECS[pid]
            values[pid] = convert_to_0to1(0.0, spec[0], spec[1], 1.0)

        # key (index 10, 0-11), scale (index 11, 0-6)
        values["key"]   = raw[10] / 11.0 if raw[10] >= 0 else 0.0
        values["scale"] = raw[11] / 6.0  if raw[11] >= 0 else 0.0

        # Build XML
        root = ET.Element("FanTunePreset")
        root.set("name", name)
        root.set("version", "1.0.0")
        root.set("plugin", "FanTune")

        params = ET.SubElement(root, "PARAMETERS")
        params.set("id", "PARAMETERS")
        for pid, val in values.items():
            # Format with 6 decimal places for precision
            params.set(pid, f"{val:.6f}")

        # NOTE: JUCE APVTS may serialize as VALUE TREE instead
        # Write both formats to be safe
        tree = ET.ElementTree(root)
        filename = f"{name}.fantune"
        filepath = os.path.join(output_dir, filename)
        tree.write(filepath, xml_declaration=True, encoding="UTF-8")

        if verbose:
            print(f"  {filename}")
            for pid, val in values.items():
                print(f"    {pid} = {val:.6f}")

    if verbose:
        print(f"\nGenerated {len(PRESET_NAMES)} presets in {output_dir}/")


def main() -> None:
    parser = argparse.ArgumentParser(description="Generate FanTune factory presets")
    parser.add_argument("--output-dir", "-o", default="presets",
                        help="Output directory for .fantune files")
    parser.add_argument("--verbose", "-v", action="store_true",
                        help="Print preset values")
    args = parser.parse_args()
    generate_presets(args.output_dir, verbose=args.verbose)
    print("Done.")


if __name__ == "__main__":
    main()
