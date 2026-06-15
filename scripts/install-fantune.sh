#!/usr/bin/env bash
# =============================================================================
#  install-fantune.sh — macOS/Linux FanTune installer
#
#  Installs VST3 / AU / CLAP / Standalone to the correct system locations
#  for the current platform.
#
#  Usage:
#    sudo ./install-fantune.sh                    # default build path
#    ./install-fantune.sh --prefix ~/.local        # user-local install (Linux)
# =============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-"$PROJECT_DIR/build/FanTune_artefacts/Release"}"
PRESET_DIR="${PRESET_DIR:-"$PROJECT_DIR/presets"}"
PREFIX="${PREFIX:-}"  # set to ~/.local for user install

usage() {
    echo "Usage: $0 [--prefix <path>] [--build-dir <path>] [--preset-dir <path>]"
    exit 1
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --prefix)    PREFIX="$2";     shift 2 ;;
        --build-dir) BUILD_DIR="$2";  shift 2 ;;
        --preset-dir) PRESET_DIR="$2"; shift 2 ;;
        *) usage ;;
    esac
done

echo "===== FanTune Installer [$(uname)] ====="
echo "  Build:  $BUILD_DIR"
echo "  Prefix: ${PREFIX:-"(system default)"}"

get_vst3_dir() {
    if [[ -n "$PREFIX" ]]; then
        echo "$PREFIX/lib/vst3"
    elif [[ "$(uname)" == "Darwin" ]]; then
        echo "/Library/Audio/Plug-Ins/VST3"
    else
        echo "/usr/lib/vst3"
    fi
}

get_clap_dir() {
    if [[ -n "$PREFIX" ]]; then
        echo "$PREFIX/lib/clap"
    elif [[ "$(uname)" == "Darwin" ]]; then
        echo "/Library/Audio/Plug-Ins/CLAP"
    else
        echo "/usr/lib/clap"
    fi
}

get_au_dir() {
    if [[ "$(uname)" == "Darwin" ]]; then
        echo "/Library/Audio/Plug-Ins/Components"
    fi
}

get_app_dir() {
    if [[ -n "$PREFIX" ]]; then
        echo "$PREFIX/bin"
    elif [[ "$(uname)" == "Darwin" ]]; then
        echo "/Applications"
    else
        echo "/usr/local/bin"
    fi
}

get_preset_dir() {
    if [[ -n "$PREFIX" ]]; then
        echo "$PREFIX/share/fantune/presets"
    elif [[ "$(uname)" == "Darwin" ]]; then
        echo "$HOME/Documents/FanTune/Presets"
    else
        echo "${XDG_DATA_HOME:-$HOME/.local/share}/FanTune/Presets"
    fi
}

# ── VST3 ──
VST3_DIR="$(get_vst3_dir)"
VST3_SRC="$BUILD_DIR/VST3/FanTune.vst3"
if [[ -d "$VST3_SRC" ]]; then
    mkdir -p "$VST3_DIR"
    rm -rf "$VST3_DIR/FanTune.vst3"
    cp -R "$VST3_SRC" "$VST3_DIR/"
    echo "  VST3 → $VST3_DIR/FanTune.vst3"
fi

# ── CLAP ──
CLAP_DIR="$(get_clap_dir)"
CLAP_SRC="$BUILD_DIR/CLAP/FanTune.clap"
if [[ -f "$CLAP_SRC" ]]; then
    mkdir -p "$CLAP_DIR"
    cp "$CLAP_SRC" "$CLAP_DIR/FanTune.clap"
    echo "  CLAP → $CLAP_DIR/FanTune.clap"
fi

# ── AU (macOS only) ──
AU_DIR="$(get_au_dir)"
AU_SRC="$BUILD_DIR/AU/FanTune.component"
if [[ -n "$AU_DIR" && -d "$AU_SRC" ]]; then
    mkdir -p "$AU_DIR"
    rm -rf "$AU_DIR/FanTune.component"
    cp -R "$AU_SRC" "$AU_DIR/"
    echo "  AU   → $AU_DIR/FanTune.component"
fi

# ── Standalone ──
APP_DIR="$(get_app_dir)"
if [[ "$(uname)" == "Darwin" ]]; then
    APP_SRC="$BUILD_DIR/Standalone/FanTune.app"
    if [[ -d "$APP_SRC" ]]; then
        rm -rf "$APP_DIR/FanTune.app"
        cp -R "$APP_SRC" "$APP_DIR/"
        echo "  APP  → $APP_DIR/FanTune.app"
    fi
else
    EXE_SRC="$BUILD_DIR/Standalone/fantune"
    if [[ -f "$EXE_SRC" ]]; then
        mkdir -p "$APP_DIR"
        install -m 755 "$EXE_SRC" "$APP_DIR/fantune"
        echo "  APP  → $APP_DIR/fantune"
    fi
fi

# ── Factory presets ──
PRESET_DST="$(get_preset_dir)"
if [[ -d "$PRESET_DIR" ]]; then
    mkdir -p "$PRESET_DST"
    count=0
    for f in "$PRESET_DIR"/*.fantune; do
        [[ -f "$f" ]] || continue
        cp "$f" "$PRESET_DST/"
        count=$((count + 1))
    done
    echo "  Presets ($count) → $PRESET_DST"
fi

echo ""
echo "FanTune installed. Restart your DAW to scan the new plugin."
[[ -z "$PREFIX" && "$(uname)" == "Darwin" ]] && echo "Note: AU registration may need: sudo killall -9 AudioComponentRegistrar"
