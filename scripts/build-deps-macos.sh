#!/usr/bin/env bash
# build-deps-macos.sh — Build all dependencies for macOS
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

# --- Defaults ---
ARCH=""
OUTPUT_DIR=""
QT_VERSION="6.5.9"
FFMPEG_VERSION="8.0"
JOBS="$(sysctl -n hw.ncpu)"

# --- Usage ---
usage() {
    cat <<EOF
Usage: $(basename "$0") [OPTIONS]

Options:
  --arch <arm64|x64>       Target architecture (required)
  --output <dir>           Output directory for compiled deps (required)
  --qt-version <ver>       Qt version (default: $QT_VERSION)
  --ffmpeg-version <ver>   FFmpeg version (default: $FFMPEG_VERSION)
  --jobs <n>               Parallel jobs (default: $JOBS)
  -h, --help               Show this help
EOF
    exit 0
}

# --- Parse arguments ---
while [[ $# -gt 0 ]]; do
    case "$1" in
        --arch)           ARCH="$2"; shift 2 ;;
        --output)         OUTPUT_DIR="$2"; shift 2 ;;
        --qt-version)     QT_VERSION="$2"; shift 2 ;;
        --ffmpeg-version) FFMPEG_VERSION="$2"; shift 2 ;;
        --jobs)           JOBS="$2"; shift 2 ;;
        -h|--help)        usage ;;
        *)                echo "Unknown argument: $1" >&2; usage; exit 1 ;;
    esac
done

# --- Validate ---
if [[ -z "$ARCH" ]] || [[ -z "$OUTPUT_DIR" ]]; then
    echo "Error: --arch and --output are required" >&2
    usage
    exit 1
fi

if [[ "$ARCH" != "arm64" && "$ARCH" != "x64" ]]; then
    echo "Error: --arch must be arm64 or x64" >&2
    exit 1
fi

command -v zstd >/dev/null 2>&1 || { echo "Error: zstd not found. Install with: brew install zstd" >&2; exit 1; }

mkdir -p "$OUTPUT_DIR"
DEPS_DIR="$OUTPUT_DIR/deps"
mkdir -p "$DEPS_DIR"

echo "=== Building dependencies for macOS $ARCH ==="
echo "Qt: $QT_VERSION | FFmpeg: $FFMPEG_VERSION | Jobs: $JOBS"
echo "Output: $OUTPUT_DIR"

# --- Build FFmpeg ---
build_ffmpeg() {
    echo ""
    echo "=== Building FFmpeg $FFMPEG_VERSION ==="
    local src_dir="$OUTPUT_DIR/ffmpeg-src"
    mkdir -p "$src_dir"

    # Download
    if [[ ! -f "$src_dir/Makefile" ]]; then
        echo "Downloading FFmpeg $FFMPEG_VERSION..."
        curl -L "https://ffmpeg.org/releases/ffmpeg-${FFMPEG_VERSION}.tar.xz" | \
            tar -xJ -C "$src_dir" --strip-components=1
    fi

    # Configure
    local host_arch
    if [[ "$ARCH" == "arm64" ]]; then
        host_arch="aarch64"
    else
        host_arch="x86_64"
    fi

    echo "Configuring FFmpeg..."
    cd "$src_dir"
    ./configure \
        --prefix="$DEPS_DIR/ffmpeg" \
        --enable-shared \
        --disable-static \
        --disable-doc \
        --disable-programs \
        --disable-autodetect \
        --arch="$host_arch" \
        --extra-cflags="-mmacosx-version-min=12.0" \
        --extra-ldflags="-mmacosx-version-min=12.0"

    # Build
    echo "Building FFmpeg with $JOBS jobs..."
    make -j"$JOBS"
    make install

    cd "$PROJECT_DIR"
    echo "FFmpeg installed to $DEPS_DIR/ffmpeg"
}

# --- Build Qt6 ---
build_qt6() {
    echo ""
    echo "=== Building Qt6 $QT_VERSION ==="
    local src_dir="$OUTPUT_DIR/qt6-src"
    local build_dir="$OUTPUT_DIR/qt6-build"
    mkdir -p "$src_dir" "$build_dir"

    # Download
    local qt_url="https://download.qt.io/official_releases/qt/6.5/${QT_VERSION}/src/single/qt-everywhere-opensource-src-${QT_VERSION}.zip"
    if [[ ! -d "$src_dir/qt-everywhere-opensource-src-${QT_VERSION}" ]]; then
        local qt_zip="$OUTPUT_DIR/qt6-src.zip"
        if [[ ! -f "$qt_zip" ]]; then
            echo "Downloading Qt6 $QT_VERSION..."
            curl -L -o "$qt_zip" "$qt_url"
        fi
        echo "Extracting Qt6 $QT_VERSION..."
        unzip -q -d "$src_dir" "$qt_zip"
        rm -f "$qt_zip"
    fi

    local qt_src="$src_dir/qt-everywhere-opensource-src-${QT_VERSION}"

    echo "Configuring Qt6..."
    cd "$build_dir"
    "$qt_src/configure" \
        -prefix "$DEPS_DIR/qt6" \
        -opensource -confirm-license \
        -nomake examples -nomake tests \
        -release \
        -no-rpath \
        -skip qtwebengine \
        -skip qt3d \
        -skip qtdeclarative \
        -skip qtquick \
        -skip qtquickcontrols2 \
        -skip qtquick3d \
        -skip qtactiveqt \
        -skip qtscxml \
        -skip qtserialbus \
        -skip qtserialport \
        -skip qtlocation \
        -skip qtsensors \
        -skip qtconnectivity \
        -skip qtwayland \
        -skip qtvirtualkeyboard \
        -skip qtnetworkauth \
        -skip qtremoteobjects \
        -skip qtcoap \
        -skip qtmqtt \
        -skip qtopcua \
        -skip qtwebview \
        -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0

    echo "Building Qt6 with $JOBS jobs..."
    cmake --build . -j"$JOBS"
    make install

    cd "$PROJECT_DIR"
    echo "Qt6 installed to $DEPS_DIR/qt6"
}

# --- Build QWindowKit ---
build_qwindowkit() {
    echo ""
    echo "=== Building QWindowKit ==="
    local src_dir="$OUTPUT_DIR/qwindowkit-src"

    # Clone if not present
    if [[ ! -d "$src_dir" ]]; then
        echo "Cloning QWindowKit..."
        git clone --depth 1 --branch v1.5.0 https://github.com/stdware/qwindowkit.git "$src_dir"
    fi

    local build_dir="$OUTPUT_DIR/qwindowkit-build"
    mkdir -p "$build_dir"

    echo "Configuring QWindowKit..."
    cd "$build_dir"
    cmake "$src_dir" \
        -DCMAKE_PREFIX_PATH="$DEPS_DIR/qt6" \
        -DCMAKE_INSTALL_PREFIX="$DEPS_DIR/qwindowkit" \
        -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0 \
        -DBUILD_SHARED_LIBS=ON

    echo "Building QWindowKit..."
    cmake --build . -j"$JOBS"
    make install

    cd "$PROJECT_DIR"
    echo "QWindowKit installed to $DEPS_DIR/qwindowkit"
}

# --- Main ---
build_ffmpeg
build_qt6
build_qwindowkit

echo ""
echo "=== Packaging dependencies ==="
cd "$OUTPUT_DIR"
tar -cf - deps/ | zstd -T0 -o "$OUTPUT_DIR/deps-macos-${ARCH}.tar.zst"
rm -rf deps

echo ""
echo "=== Done ==="
echo "Package: $OUTPUT_DIR/deps-macos-${ARCH}.tar.zst"
echo "Size: $(du -h "$OUTPUT_DIR/deps-macos-${ARCH}.tar.zst" | cut -f1)"
