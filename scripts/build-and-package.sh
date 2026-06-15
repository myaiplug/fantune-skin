#!/usr/bin/env bash
# =============================================================================
#  FanTune Build + Package — macOS/Linux
#  Builds Release config, generates presets, creates DMG/DEB/RPM/TGZ.
# =============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$PROJECT_DIR/build"
PRESET_DIR="$PROJECT_DIR/presets"

echo "===== FanTune Build + Package [$(uname)] ====="

# ── 1. Generate factory presets ──
echo ""
echo "[1/4] Generating factory presets..."
cd "$PROJECT_DIR"
python3 tools/generate_factory_presets.py --output-dir "$PRESET_DIR" --verbose

# ── 2. Configure CMake ──
echo ""
echo "[2/4] Configuring CMake..."
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

if [[ "$(uname)" == "Darwin" ]]; then
    cmake "$PROJECT_DIR" -G "Xcode" -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64"
else
    cmake "$PROJECT_DIR" -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/usr
fi

# ── 3. Build all targets ──
echo ""
echo "[3/4] Building all targets..."
cmake --build . --config Release

# ── 4. Create package ──
echo ""
echo "[4/4] Creating package..."
if [[ "$(uname)" == "Darwin" ]]; then
    # macOS: DMG
    cpack -G DragNDrop -C Release
    echo "Created DMG package."
elif [[ -f /etc/debian_version ]]; then
    # Debian/Ubuntu: DEB + TGZ
    cpack -G DEB -C Release || true
    cpack -G TGZ -C Release || true
    echo "Created DEB + TGZ packages."
elif [[ -f /etc/redhat-release ]] || command -v rpm &>/dev/null; then
    # Fedora/RHEL: RPM + TGZ
    cpack -G RPM -C Release || true
    cpack -G TGZ -C Release || true
    echo "Created RPM + TGZ packages."
else
    cpack -G TGZ -C Release || true
    echo "Created TGZ package."
fi

echo ""
echo "===== BUILD + PACKAGE COMPLETE ====="
echo "Packages in: $BUILD_DIR"

case "$(uname)" in
    Darwin) echo "Install DMG by opening it and dragging FanTune.app to Applications." ;;
    Linux)  echo "Install DEB: sudo dpkg -i build/FanTune-*.deb" ;;
esac
