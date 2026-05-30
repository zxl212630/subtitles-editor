#!/usr/bin/env bash
# package-macos.sh — Build Release and create .dmg for macOS (Apple Silicon)
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$PROJECT_DIR/cmake-build-release"
APP_NAME="subtitles-editor"
DMG_NAME="SubtitlesEditor-1.0.0-macOS-arm64-unsigned"

# --- Usage ---
usage() {
    cat <<EOF
Usage: $(basename "$0") [OPTIONS]

Options:
  --qt <path>            Qt6 安装根目录 (例: ~/Tools/Qt/6.5.7)
  --ffmpeg <path>        FFmpeg 安装根目录 (例: ~/Tools/ffmpeg/8.0)
  --qwindowkit <path>    QWindowKit 安装根目录 (例: ~/Tools/Qt/QwindowKit/Qt6)
  --output <name>        DMG 输出文件名 (默认: $DMG_NAME)
  -h, --help             显示此帮助信息

示例:
  # 完整参数
  $(basename "$0") --qt ~/Tools/Qt/6.5.7 --ffmpeg ~/Tools/ffmpeg/8.0 --qwindowkit ~/Tools/Qt/QwindowKit/Qt6

  # 仅 Qt 和 FFmpeg (QWindowKit 通过环境变量)
  $(basename "$0") --qt ~/Tools/Qt/6.5.7 --ffmpeg ~/Tools/ffmpeg/8.0

  # 通过环境变量
  QT_ROOT=~/Tools/Qt/6.5.7 FFMPEG_ROOT=~/Tools/ffmpeg/8.0 $(basename "$0")

环境变量 (命令行参数优先):
  QT_ROOT, FFMPEG_ROOT, QWINDOWKIT_ROOT
EOF
    exit 0
}

# --- Parse arguments ---
QT_ROOT="${QT_ROOT:-}"
FFMPEG_ROOT="${FFMPEG_ROOT:-}"
QWINDOWKIT_ROOT="${QWINDOWKIT_ROOT:-}"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --qt)        QT_ROOT="$2"; shift 2 ;;
        --ffmpeg)    FFMPEG_ROOT="$2"; shift 2 ;;
        --qwindowkit) QWINDOWKIT_ROOT="$2"; shift 2 ;;
        --output)    DMG_NAME="$2"; shift 2 ;;
        -h|--help)   usage ;;
        *)           echo "未知参数: $1"; usage ;;
    esac
done

# --- Validate required paths ---
missing=()
[[ -z "$QT_ROOT" ]]     && missing+=("--qt (Qt6 根目录)")
[[ -z "$FFMPEG_ROOT" ]] && missing+=("--ffmpeg (FFmpeg 根目录)")
[[ -z "$QWINDOWKIT_ROOT" ]] && missing+=("--qwindowkit (QWindowKit 根目录)")

if [[ ${#missing[@]} -gt 0 ]]; then
    echo "错误: 缺少必需参数:" >&2
    for m in "${missing[@]}"; do
        echo "  $m" >&2
    done
    echo "" >&2
    echo "使用 --help 查看完整用法" >&2
    exit 1
fi

for dir in "$QT_ROOT" "$FFMPEG_ROOT" "$QWINDOWKIT_ROOT"; do
    [[ -d "$dir" ]] || { echo "错误: 目录不存在: $dir" >&2; exit 1; }
done

APP_BUNDLE="$BUILD_DIR/$APP_NAME.app"
APP_BIN="$APP_BUNDLE/Contents/MacOS/$APP_NAME"
FW_DIR="$APP_BUNDLE/Contents/Frameworks"
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

# Copy a dylib into Frameworks, skip if already exists
copy_to_frameworks() {
    local src="$1"
    local bname
    bname=$(basename "$src")
    [[ -f "$FW_DIR/$bname" ]] && return 0
    echo "  Bundling: $bname"
    cp "$src" "$FW_DIR/$bname"
}

# Resolve @rpath reference to actual file
resolve_rpath() {
    local name="${1#@rpath/}"
    for dir in \
        "$FFMPEG_ROOT/lib" \
        "$QT_ROOT/lib" \
        "$QWINDOWKIT_ROOT/lib" \
        /opt/homebrew/lib \
        /opt/homebrew/opt/*/lib \
        "$HOME/Tools/flowcut/release/build/lib"; do
        [[ -f "$dir/$name" ]] && echo "$dir/$name" && return 0
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

    while IFS= read -r ref; do
        is_system_lib "$ref" && continue
        is_framework "$ref" && continue

        local key
        key=$(basename "$ref")
        is_processed "$key" && continue

        local real_path="$ref"
        if [[ "$ref" == @rpath/* ]]; then
            real_path=$(resolve_rpath "$ref") || continue
        fi
        [[ ! -f "$real_path" ]] && continue

        copy_to_frameworks "$real_path"
        mark_processed "$key"
        bundle_deps "$FW_DIR/$key"
    done < <(otool -L "$binary" 2>/dev/null | tail -n +2 | awk '{print $1}')
}

# --- Main ---

echo "=== Building Release ==="
cmake -B "$BUILD_DIR" -S "$PROJECT_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DQt6_ROOT="$QT_ROOT" \
    -DFFMPEG_ROOT="$FFMPEG_ROOT" \
    -DQWindowKit_ROOT="$QWINDOWKIT_ROOT"
cmake --build "$BUILD_DIR" -j"$(sysctl -n hw.ncpu)"

[[ ! -d "$APP_BUNDLE" ]] && { echo "ERROR: $APP_BUNDLE not found" >&2; exit 1; }

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
for dylib in "$FW_DIR"/*.dylib; do
    [[ -L "$dylib" ]] && continue
    bundle_deps "$dylib"
done

echo "=== Creating symlinks ==="
# For each file like libXXX.NN.dylib, replace with symlink to libXXX.NN.NNN.NNN.dylib
cd "$FW_DIR"
for f in *.dylib; do
    [[ -L "$f" ]] && continue
    # Match libXXX.NN.dylib (2-part version)
    if [[ "$f" =~ ^(.+)\.([0-9]{1,2})\.dylib$ ]]; then
        base="${BASH_REMATCH[1]}"
        # Find fully versioned match: libXXX.NN.NNN.NNN.dylib
        full=$(ls -1 "${base}".*.*.dylib 2>/dev/null | grep -E '\.[0-9]+\.[0-9]+\.[0-9]+\.dylib$' | head -1 || true)
        if [[ -n "$full" && "$full" != "$f" ]]; then
            echo "  Symlink: $f -> $(basename "$full")"
            rm "$f"
            ln -s "$(basename "$full")" "$f"
        fi
    fi
done
cd "$PROJECT_DIR"

echo "=== Fixing rpaths ==="
# Ensure @executable_path/../Frameworks is in rpath
install_name_tool -add_rpath "@executable_path/../Frameworks" "$APP_BIN" 2>/dev/null || true

# Fix IDs of all real dylibs
for dylib in "$FW_DIR"/*.dylib; do
    [[ -L "$dylib" ]] && continue
    install_name_tool -id "@rpath/$(basename "$dylib")" "$dylib"
done

# Rewrite all references in main binary to @rpath
for dylib in "$FW_DIR"/*.dylib; do
    bname=$(basename "$dylib")
    old_ref=$(otool -L "$APP_BIN" | grep "$bname" | awk '{print $1}' | head -1 || true)
    [[ -n "$old_ref" && "$old_ref" != "@rpath/$bname" ]] && \
        install_name_tool -change "$old_ref" "@rpath/$bname" "$APP_BIN"
done

# Fix inter-dependencies among bundled dylibs
for dylib in "$FW_DIR"/*.dylib; do
    [[ -L "$dylib" ]] && continue
    while IFS= read -r ref; do
        is_system_lib "$ref" && continue
        is_framework "$ref" && continue
        bname=$(basename "$ref")
        [[ -f "$FW_DIR/$bname" && "$ref" != "@rpath/$bname" ]] && \
            install_name_tool -change "$ref" "@rpath/$bname" "$dylib" 2>/dev/null || true
    done < <(otool -L "$dylib" 2>/dev/null | tail -n +2 | awk '{print $1}')
done

echo "=== Verifying bundle ==="
has_error=0
for dylib in "$APP_BIN" "$FW_DIR"/*.dylib; do
    [[ -L "$dylib" ]] && continue
    bad=$(otool -L "$dylib" 2>/dev/null | tail -n +2 | awk '{print $1}' | grep -E "^/(Users|opt|usr/local)" || true)
    if [[ -n "$bad" ]]; then
        echo "ERROR: $(basename "$dylib") has unresolved paths:"
        echo "$bad"
        has_error=1
    fi
done
[[ $has_error -eq 0 ]] && echo "All dependencies resolved OK"

echo "=== Signing bundle ==="
codesign --force --deep --sign - "$APP_BUNDLE"

echo "=== Creating DMG ==="
STAGING_DIR=$(mktemp -d)
cp -R "$APP_BUNDLE" "$STAGING_DIR/"
ln -sf /Applications "$STAGING_DIR/Applications"
mkdir -p "$PROJECT_DIR/dist/macos"
hdiutil create -volname "$DMG_NAME" \
    -srcfolder "$STAGING_DIR" \
    -ov -format UDZO \
    "$PROJECT_DIR/dist/macos/$DMG_NAME.dmg"
rm -rf "$STAGING_DIR"

echo ""
echo "=== Done ==="
echo "DMG: $PROJECT_DIR/dist/macos/$DMG_NAME.dmg"
echo "Size: $(du -h "$PROJECT_DIR/dist/macos/$DMG_NAME.dmg" | cut -f1)"
