#!/usr/bin/env bash
# package-macos.sh — Build Release and create .dmg for macOS (Apple Silicon)
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$PROJECT_DIR/cmake-build-release"
APP_NAME="subtitles-editor"
DMG_NAME="SubtitlesEditor-1.0.0-macOS-arm64"

# SDK paths (adjust if needed)
QT_ROOT="${QT_ROOT:-$HOME/Tools/Qt/6.5.8}"
FFMPEG_ROOT="${FFMPEG_ROOT:-$HOME/Tools/ffmpeg/8.0}"
QWINDOWKIT_ROOT="${QWINDOWKIT_ROOT:-$HOME/Tools/Qt/QwindowKit/Qt6}"

echo "=== Building Release ==="
cmake -B "$BUILD_DIR" -S "$PROJECT_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DQt6_ROOT="$QT_ROOT" \
    -DFFMPEG_ROOT="$FFMPEG_ROOT" \
    -DQWindowKit_ROOT="$QWINDOWKIT_ROOT"

cmake --build "$BUILD_DIR" -j"$(sysctl -n hw.ncpu)"

APP_BUNDLE="$BUILD_DIR/$APP_NAME.app"
if [[ ! -d "$APP_BUNDLE" ]]; then
    echo "ERROR: $APP_BUNDLE not found" >&2
    exit 1
fi

echo "=== Bundling Qt frameworks ==="
"$QT_ROOT/bin/macdeployqt" "$APP_BUNDLE" -always-overwrite

echo "=== Copying FFmpeg libraries ==="
FW_DIR="$APP_BUNDLE/Contents/Frameworks"
mkdir -p "$FW_DIR"

FFMPEG_LIBS=(libavcodec libavformat libavutil libswscale libswresample)
for lib in "${FFMPEG_LIBS[@]}"; do
    # Copy versioned dylib (e.g. libavcodec.62.11.100.dylib)
    src=$(find "$FFMPEG_ROOT/lib" -name "${lib}.*.*.*.dylib" | head -1)
    if [[ -z "$src" ]]; then
        echo "WARNING: $lib not found, skipping"
        continue
    fi
    basename=$(basename "$src")
    cp "$src" "$FW_DIR/$basename"

    # Create symlinks (libavcodec.dylib -> libavcodec.62.dylib -> libavcodec.62.11.100.dylib)
    major=$(echo "$basename" | sed -E "s/${lib}\.([0-9]+)\..*/\1/")
    cd "$FW_DIR"
    ln -sf "$basename" "${lib}.${major}.dylib"
    ln -sf "${lib}.${major}.dylib" "${lib}.dylib"
    cd "$PROJECT_DIR"
done

echo "=== Fixing FFmpeg rpaths ==="
APP_BIN="$APP_BUNDLE/Contents/MacOS/$APP_NAME"
for lib in "${FFMPEG_LIBS[@]}"; do
    src=$(find "$FFMPEG_ROOT/lib" -name "${lib}.*.*.*.dylib" | head -1)
    [[ -z "$src" ]] && continue
    old_id=$(otool -D "$src" | tail -1)
    basename=$(basename "$src")
    install_name_tool -change "$old_id" "@rpath/$basename" "$APP_BIN" 2>/dev/null || true
done

# Ensure @loader_path/../Frameworks is in rpath
install_name_tool -add_rpath "@executable_path/../Frameworks" "$APP_BIN" 2>/dev/null || true

# Fix FFmpeg library IDs and inter-dependencies
for dylib in "$FW_DIR"/lib*.dylib; do
    [[ -L "$dylib" ]] && continue
    bname=$(basename "$dylib")
    install_name_tool -id "@rpath/$bname" "$dylib"

    # Fix references to other FFmpeg libs
    for lib in "${FFMPEG_LIBS[@]}"; do
        ref=$(otool -L "$dylib" | grep "$lib" | awk '{print $1}' | head -1 || true)
        [[ -n "$ref" && "$ref" != "@rpath/"* ]] && \
            install_name_tool -change "$ref" "@rpath/$(basename "$ref")" "$dylib" 2>/dev/null || true
    done
done

echo "=== Verifying bundle ==="
if otool -L "$APP_BIN" | grep -qE "(ffmpeg|avcodec|avformat|avutil|swscale|swresample)"; then
    echo "FFmpeg linked OK"
else
    echo "WARNING: FFmpeg not linked"
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
