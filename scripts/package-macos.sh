#!/usr/bin/env bash
# package-macos.sh — Build Release and create .dmg for macOS (Apple Silicon)
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$PROJECT_DIR/cmake-build-release"
APP_NAME="subtitles-editor"
TARGET_ARCH="${TARGET_ARCH:-}"
VERSION="${VERSION:-1.0.0}"

# --- Usage ---
usage() {
    cat <<EOF
Usage: $(basename "$0") [OPTIONS]

Options:
  --qt <path>            Qt6 安装根目录 (例: ~/Tools/Qt/6.5.7)
  --ffmpeg <path>        FFmpeg 安装根目录 (例: ~/Tools/ffmpeg/8.0)
  --qwindowkit <path>    QWindowKit 安装根目录 (例: ~/Tools/Qt/QwindowKit/Qt6)
  --output <name>        DMG 输出文件名 (默认: $DMG_NAME)
  --deps-dir <dir>       Use pre-compiled dependencies from this directory
  --arch <arch>          Target architecture: arm64 or x64 (default: host arch)
  --version <ver>        App version for DMG name (default: 1.0.0)
  -h, --help             显示此帮助信息

示例:
  # 完整参数
  $(basename "$0") --qt ~/Tools/Qt/6.5.7 --ffmpeg ~/Tools/ffmpeg/8.0 --qwindowkit ~/Tools/Qt/QwindowKit/Qt6

  # 仅 Qt 和 FFmpeg (QWindowKit 通过环境变量)
  $(basename "$0") --qt ~/Tools/Qt/6.5.7 --ffmpeg ~/Tools/ffmpeg/8.0

  # 通过环境变量
  QT_ROOT=~/Tools/Qt/6.5.7 FFMPEG_ROOT=~/Tools/ffmpeg/8.0 $(basename "$0")

环境变量 (命令行参数优先):
  QT_ROOT, FFMPEG_ROOT, QWINDOWKIT_ROOT, VERSION
EOF
    exit 0
}

# --- Parse arguments ---
QT_ROOT="${QT_ROOT:-}"
FFMPEG_ROOT="${FFMPEG_ROOT:-}"
QWINDOWKIT_ROOT="${QWINDOWKIT_ROOT:-}"
DEPS_DIR=""
TARGET_ARCH=""

while [[ $# -gt 0 ]]; do
    case "$1" in
        --qt)           QT_ROOT="$2"; shift 2 ;;
        --ffmpeg)       FFMPEG_ROOT="$2"; shift 2 ;;
        --qwindowkit)   QWINDOWKIT_ROOT="$2"; shift 2 ;;
        --output)       DMG_NAME="$2"; shift 2 ;;
        --deps-dir)     DEPS_DIR="$2"; shift 2 ;;
        --arch)         TARGET_ARCH="$2"; shift 2 ;;
        --version)      VERSION="$2"; shift 2 ;;
        -h|--help)      usage ;;
        *)              echo "未知参数: $1"; usage ;;
    esac
done

# --- Set DMG name based on architecture ---
DMG_NAME="SubtitlesEditor-${VERSION}-macOS-${TARGET_ARCH:-$(uname -m)}-unsigned"

# --- Resolve dependencies from --deps-dir ---
if [[ -n "$DEPS_DIR" ]]; then
    [[ -d "$DEPS_DIR/deps/qt6" ]]     || { echo "Error: $DEPS_DIR/deps/qt6 not found" >&2; exit 1; }
    [[ -d "$DEPS_DIR/deps/qwindowkit" ]] || { echo "Error: $DEPS_DIR/deps/qwindowkit not found" >&2; exit 1; }
    [[ -d "$DEPS_DIR/deps/ffmpeg" ]]   || { echo "Error: $DEPS_DIR/deps/ffmpeg not found" >&2; exit 1; }
    QT_ROOT="$DEPS_DIR/deps/qt6"
    QWINDOWKIT_ROOT="$DEPS_DIR/deps/qwindowkit"
    FFMPEG_ROOT="$DEPS_DIR/deps/ffmpeg"
fi

# --- Validate required paths ---
missing=()
[[ -z "$QT_ROOT" ]]     && missing+=("--qt (Qt6 根目录)")
[[ -z "$QWINDOWKIT_ROOT" ]] && missing+=("--qwindowkit (QWindowKit 根目录)")
[[ -z "$FFMPEG_ROOT" ]] && missing+=("--ffmpeg (FFmpeg 根目录)")

if [[ ${#missing[@]} -gt 0 ]]; then
    echo "错误: 缺少必需参数:" >&2
    for m in "${missing[@]}"; do
        echo "  $m" >&2
    done
    echo "" >&2
    echo "使用 --help 查看完整用法" >&2
    exit 1
fi

for dir in "$QT_ROOT" "$QWINDOWKIT_ROOT"; do
    [[ -d "$dir" ]] || { echo "错误: 目录不存在: $dir" >&2; exit 1; }
done
# FFMPEG_ROOT 可以为空（使用系统 FFmpeg），非空时检查目录
[[ -n "$FFMPEG_ROOT" ]] && [[ ! -d "$FFMPEG_ROOT" ]] && { echo "错误: 目录不存在: $FFMPEG_ROOT" >&2; exit 1; }

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
    chmod 755 "$FW_DIR/$bname"
}

# Resolve @rpath reference to actual file
resolve_rpath() {
    local name="${1#@rpath/}"
    local dirs=()

    # 优先搜索指定的自定义依赖目录
    if [[ -n "$QT_ROOT" ]]; then
        dirs+=("$QT_ROOT/lib")
    fi
    if [[ -n "$QWINDOWKIT_ROOT" ]]; then
        dirs+=("$QWINDOWKIT_ROOT/lib")
    fi
    if [[ -n "$FFMPEG_ROOT" ]]; then
        dirs+=("$FFMPEG_ROOT/lib")
    fi

    # 最后才是系统级路径作为后备
    dirs+=(
        /opt/homebrew/lib
        /opt/homebrew/opt/ffmpeg/lib
        /opt/homebrew/opt/*/lib
        /usr/local/lib
        /usr/local/opt/ffmpeg/lib
        /usr/local/opt/*/lib
    )

    for dir in "${dirs[@]}"; do
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
        if [[ ! -f "$real_path" ]]; then
            # If the absolute path doesn't exist on this builder machine, try resolving it from local dependencies
            real_path=$(resolve_rpath "@rpath/$(basename "$ref")") || continue
        fi

        copy_to_frameworks "$real_path"
        mark_processed "$key"
        bundle_deps "$FW_DIR/$key"
    done < <(otool -L "$binary" 2>/dev/null | tail -n +2 | awk '{print $1}')
}

# --- Main ---

echo "=== Building Release ==="
cmake -B "$BUILD_DIR" -S "$PROJECT_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0 \
    -DQt6_ROOT="$QT_ROOT" \
    -DFFMPEG_ROOT="$FFMPEG_ROOT" \
    -DQWindowKit_ROOT="$QWINDOWKIT_ROOT"
cmake --build "$BUILD_DIR" -j"$(sysctl -n hw.ncpu)"

[[ ! -d "$APP_BUNDLE" ]] && { echo "ERROR: $APP_BUNDLE not found" >&2; exit 1; }

echo "=== Bundling Qt frameworks ==="
"$QT_ROOT/bin/macdeployqt" "$APP_BUNDLE" -always-overwrite

echo "=== Copying FFmpeg libraries ==="
mkdir -p "$FW_DIR"
FFMPEG_LIB_DIR="$FFMPEG_ROOT/lib"
FFMPEG_BIN_DIR="$FFMPEG_ROOT/bin"

# 复制所有 FFmpeg dylib（libav*, libsw*, libpostproc*）
for src in "$FFMPEG_LIB_DIR"/lib*.dylib; do
    [[ -L "$src" ]] && continue  # 跳过符号链接
    # 跳过 libavdevice，因为项目不需要它且它可能链接系统的 X11/XCB 库
    [[ "$(basename "$src")" == libavdevice* ]] && continue
    copy_to_frameworks "$src"
done

echo "=== Copying FFmpeg executables ==="
for exe in ffmpeg ffprobe; do
    src="$FFMPEG_BIN_DIR/$exe"
    if [[ -f "$src" ]]; then
        echo "  Bundling: $exe"
        cp "$src" "$APP_BUNDLE/Contents/MacOS/$exe"
        chmod +x "$APP_BUNDLE/Contents/MacOS/$exe"
    else
        echo "WARNING: $exe not found in $FFMPEG_BIN_DIR"
    fi
done

echo "=== Discovering and bundling dependencies ==="
bundle_deps "$APP_BIN"
for exe in ffmpeg ffprobe; do
    local_exe="$APP_BUNDLE/Contents/MacOS/$exe"
    if [[ -f "$local_exe" ]]; then
        bundle_deps "$local_exe"
    fi
done
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

echo "=== Fixing rpaths & references in executables ==="
# Remove absolute rpaths that point to build machine, add Frameworks rpath, rewrite references to @rpath
for bin in "$APP_BIN" "$APP_BUNDLE/Contents/MacOS/ffmpeg" "$APP_BUNDLE/Contents/MacOS/ffprobe"; do
    [[ ! -f "$bin" ]] && continue
    echo "  Processing executable: $(basename "$bin")"
    
    # 1. Remove absolute rpaths
    for rpath in $(otool -l "$bin" 2>/dev/null | grep -A2 LC_RPATH | grep path | awk '{print $2}'); do
        if [[ "$rpath" == /Users/* ]] || [[ "$rpath" == /opt/homebrew/* ]] || [[ "$rpath" == /usr/local/* ]]; then
            echo "    Removing absolute rpath: $rpath"
            install_name_tool -delete_rpath "$rpath" "$bin" 2>/dev/null || true
        fi
    done
    
    # 2. Ensure @executable_path/../Frameworks is in rpath
    install_name_tool -add_rpath "@executable_path/../Frameworks" "$bin" 2>/dev/null || true
    
    # 3. Rewrite all absolute references to @rpath
    for old_ref in $(otool -L "$bin" 2>/dev/null | tail -n +2 | awk '{print $1}'); do
        if [[ "$old_ref" == /Users/* ]] || [[ "$old_ref" == /opt/* ]]; then
            bname=$(basename "$old_ref")
            echo "    Rewriting reference: $old_ref -> @rpath/$bname"
            install_name_tool -change "$old_ref" "@rpath/$bname" "$bin" 2>/dev/null || true
        fi
    done
done

echo "=== Fixing references in bundled dylibs ==="
for dylib in "$FW_DIR"/*.dylib; do
    [[ -L "$dylib" ]] && continue
    # Fix ID of the dylib itself
    bname=$(basename "$dylib")
    install_name_tool -id "@rpath/$bname" "$dylib" 2>/dev/null || true

    # Fix absolute references inside the dylib
    for old_ref in $(otool -L "$dylib" 2>/dev/null | tail -n +2 | awk '{print $1}'); do
        if [[ "$old_ref" == /Users/* ]] || [[ "$old_ref" == /opt/* ]]; then
            ref_bname=$(basename "$old_ref")
            echo "  Rewriting dylib reference in $(basename "$dylib"): $old_ref -> @rpath/$ref_bname"
            install_name_tool -change "$old_ref" "@rpath/$ref_bname" "$dylib" 2>/dev/null || true
        fi
    done
done

echo "=== Verifying bundle ==="
has_error=0
for dylib in "$APP_BIN" "$APP_BUNDLE/Contents/MacOS/ffmpeg" "$APP_BUNDLE/Contents/MacOS/ffprobe" "$FW_DIR"/*.dylib; do
    [[ ! -f "$dylib" ]] && continue
    [[ -L "$dylib" ]] && continue
    
    # 1. Check for absolute paths
    bad=$(otool -L "$dylib" 2>/dev/null | tail -n +2 | awk '{print $1}' | grep -E "^/(Users|opt|usr/local)" || true)
    if [[ -n "$bad" ]]; then
        echo "ERROR: $(basename "$dylib") has unresolved absolute paths:"
        echo "$bad"
        has_error=1
    fi

    # 2. Check for missing @rpath references
    while IFS= read -r ref; do
        if [[ "$ref" == @rpath/* ]]; then
            local rfile="${ref#@rpath/}"
            if [[ ! -f "$FW_DIR/$rfile" ]]; then
                echo "ERROR: $(basename "$dylib") references $ref but $rfile is missing from Frameworks!"
                has_error=1
            fi
        fi
    done < <(otool -L "$dylib" 2>/dev/null | tail -n +2 | awk '{print $1}')
done

if [[ $has_error -ne 0 ]]; then
    echo "ERROR: Bundle verification failed!" >&2
    exit 1
fi
echo "All dependencies resolved OK"

echo "=== Signing bundle ==="
codesign --force --deep --sign - "$APP_BUNDLE"

echo "=== Creating DMG ==="
STAGING_DIR=$(mktemp -d)
cp -a "$APP_BUNDLE" "$STAGING_DIR/"
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
