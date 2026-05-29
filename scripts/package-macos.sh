#!/usr/bin/env bash
# package-macos.sh — Build Release and create .dmg for macOS (Apple Silicon)
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$PROJECT_DIR/cmake-build-release"
APP_NAME="subtitles-editor"
DMG_NAME="SubtitlesEditor-1.0.0-macOS-arm64"

QT_ROOT="${QT_ROOT:-$HOME/Tools/Qt/6.5.8}"
FFMPEG_ROOT="${FFMPEG_ROOT:-$HOME/Tools/ffmpeg/8.0}"
QWINDOWKIT_ROOT="${QWINDOWKIT_ROOT:-$HOME/Tools/Qt/QwindowKit/Qt6}"

APP_BUNDLE="$BUILD_DIR/$APP_NAME.app"
APP_BIN="$APP_BUNDLE/Contents/MacOS/$APP_NAME"
FW_DIR="$APP_BUNDLE/Contents/Frameworks"

# Track processed libs via temp file (works across subshells)
PROCESSED_FILE=$(mktemp)
trap 'rm -f "$PROCESSED_FILE"' EXIT

# --- Helper functions ---

is_system_lib() {
    [[ "$1" == /usr/lib/* ]] || [[ "$1" == /System/* ]]
}

is_framework() {
    [[ "$1" == *.framework/* ]]
}

is_processed() {
    grep -qxF "$1" "$PROCESSED_FILE" 2>/dev/null
}

mark_processed() {
    echo "$1" >> "$PROCESSED_FILE"
}

# Copy a dylib into Frameworks and fix its id
copy_to_frameworks() {
    local src="$1"
    local bname
    bname=$(basename "$src")

    [[ -f "$FW_DIR/$bname" ]] && return 0

    echo "  Bundling: $bname"
    cp "$src" "$FW_DIR/$bname"
    install_name_tool -id "@rpath/$bname" "$FW_DIR/$bname"
}

# Resolve a @rpath reference to an actual file path
resolve_rpath() {
    local ref="$1"
    local name="${ref#@rpath/}"

    # Search common library locations
    for search_dir in \
        "$FFMPEG_ROOT/lib" \
        "$QT_ROOT/lib" \
        "$QWINDOWKIT_ROOT/lib" \
        /opt/homebrew/lib \
        /opt/homebrew/opt/*/lib \
        "$HOME/Tools/flowcut/release/build/lib"; do
        local candidate="$search_dir/$name"
        if [[ -f "$candidate" ]]; then
            echo "$candidate"
            return 0
        fi
    done
    return 1
}

# Recursively discover and bundle all non-system dylib dependencies
bundle_deps() {
    local binary="$1"
    local bname
    bname=$(basename "$binary")

    is_processed "$bname" && return 0
    mark_processed "$bname"

    otool -L "$binary" 2>/dev/null | tail -n +2 | awk '{print $1}' | while read -r ref; do
        is_system_lib "$ref" && continue
        is_framework "$ref" && continue

        local key
        key=$(basename "$ref")
        is_processed "$key" && continue

        # Resolve the actual file path
        local real_path="$ref"
        if [[ "$ref" == @rpath/* ]]; then
            real_path=$(resolve_rpath "$ref") || continue
        fi

        [[ ! -f "$real_path" ]] && continue

        copy_to_frameworks "$real_path"
        mark_processed "$key"

        # Recurse into this library's dependencies
        bundle_deps "$FW_DIR/$key"
    done
}

# --- Main ---

echo "=== Building Release ==="
cmake -B "$BUILD_DIR" -S "$PROJECT_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DQt6_ROOT="$QT_ROOT" \
    -DFFMPEG_ROOT="$FFMPEG_ROOT" \
    -DQWindowKit_ROOT="$QWINDOWKIT_ROOT"

cmake --build "$BUILD_DIR" -j"$(sysctl -n hw.ncpu)"

if [[ ! -d "$APP_BUNDLE" ]]; then
    echo "ERROR: $APP_BUNDLE not found" >&2
    exit 1
fi

echo "=== Bundling Qt frameworks ==="
"$QT_ROOT/bin/macdeployqt" "$APP_BUNDLE" -always-overwrite

echo "=== Copying FFmpeg libraries ==="
mkdir -p "$FW_DIR"

for lib in libavcodec libavformat libavutil libswscale libswresample; do
    src=$(find "$FFMPEG_ROOT/lib" -name "${lib}.*.*.*.dylib" | head -1)
    [[ -z "$src" ]] && { echo "WARNING: $lib not found"; continue; }
    copy_to_frameworks "$src"
done

echo "=== Discovering and bundling dependencies ==="
bundle_deps "$APP_BIN"

# Also process each bundled dylib (in case executable didn't reference them directly)
for dylib in "$FW_DIR"/*.dylib; do
    [[ -L "$dylib" ]] && continue
    bundle_deps "$dylib"
done

echo "=== Fixing rpaths in main binary ==="
install_name_tool -add_rpath "@executable_path/../Frameworks" "$APP_BIN" 2>/dev/null || true

# Rewrite all references in main binary to use @rpath
for dylib in "$FW_DIR"/*.dylib; do
    [[ -L "$dylib" ]] && continue
    bname=$(basename "$dylib")

    # Find old install name (could be absolute path or @rpath)
    old_ref=$(otool -L "$APP_BIN" | grep "$bname" | awk '{print $1}' | head -1 || true)
    if [[ -n "$old_ref" && "$old_ref" != "@rpath/$bname" ]]; then
        install_name_tool -change "$old_ref" "@rpath/$bname" "$APP_BIN" 2>/dev/null || true
    fi
done

# Fix inter-dependencies among bundled dylibs
for dylib in "$FW_DIR"/*.dylib; do
    [[ -L "$dylib" ]] && continue
    otool -L "$dylib" 2>/dev/null | tail -n +2 | awk '{print $1}' | while read -r ref; do
        is_system_lib "$ref" && continue
        is_framework "$ref" && continue

        local_bname=$(basename "$ref")
        [[ -f "$FW_DIR/$local_bname" && "$ref" != "@rpath/$local_bname" ]] && \
            install_name_tool -change "$ref" "@rpath/$local_bname" "$dylib" 2>/dev/null || true
    done
done

echo "=== Verifying bundle ==="
# Check for any references that point outside the bundle (absolute paths not in Frameworks)
unresolved=$(otool -L "$APP_BIN" | tail -n +2 | awk '{print $1}' | grep -v /usr/lib | grep -v /System | grep -v "@rpath/" | grep -v "@executable_path/" | grep -v "$APP_NAME" || true)
if [[ -n "$unresolved" ]]; then
    echo "WARNING: Unresolved references:"
    echo "$unresolved"
else
    echo "All dependencies resolved OK"
fi

echo "=== Creating DMG ==="
STAGING_DIR=$(mktemp -d)
cp -R "$APP_BUNDLE" "$STAGING_DIR/"
ln -sf /Applications "$STAGING_DIR/Applications"

hdiutil create -volname "$DMG_NAME" \
    -srcfolder "$STAGING_DIR" \
    -ov -format UDZO \
    "$PROJECT_DIR/$DMG_NAME.dmg"

rm -rf "$STAGING_DIR"

echo ""
echo "=== Done ==="
echo "DMG: $PROJECT_DIR/$DMG_NAME.dmg"
echo "Size: $(du -h "$PROJECT_DIR/$DMG_NAME.dmg" | cut -f1)"
