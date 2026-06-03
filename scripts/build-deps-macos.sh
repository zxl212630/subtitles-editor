#!/usr/bin/env bash
# build-deps-macos.sh — Build all dependencies for macOS
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

# --- Defaults ---
ARCH=""
OUTPUT_DIR=""
QT_VERSION="6.5.9"
FFMPEG_VERSION="8.1.1"
JOBS="$(sysctl -n hw.ncpu)"
TARGET="all"

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
  --target <target>        Build target: all, ffmpeg, qt6, qwindowkit (default: $TARGET)
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
        --target)         TARGET="$2"; shift 2 ;;
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

if [[ "$TARGET" != "all" && "$TARGET" != "ffmpeg" && "$TARGET" != "qt6" && "$TARGET" != "qwindowkit" ]]; then
    echo "Error: --target must be one of all, ffmpeg, qt6, qwindowkit" >&2
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
    local build_dir="$OUTPUT_DIR/ffmpeg-build"
    mkdir -p "$src_dir"

    # Download
    local ffmpeg_url="https://ffmpeg.org/releases/ffmpeg-${FFMPEG_VERSION}.tar.xz"
    local ffmpeg_tar="$OUTPUT_DIR/ffmpeg-${FFMPEG_VERSION}.tar.xz"
    if [[ ! -d "$src_dir/ffmpeg-${FFMPEG_VERSION}" ]]; then
        if [[ ! -f "$ffmpeg_tar" ]]; then
            echo "Downloading FFmpeg $FFMPEG_VERSION..."
            curl -L --retry 3 --retry-delay 5 -o "$ffmpeg_tar" "$ffmpeg_url"
        fi
        echo "Extracting FFmpeg $FFMPEG_VERSION..."
        tar -xJ -C "$src_dir" -f "$ffmpeg_tar"
        rm -f "$ffmpeg_tar"
    fi

    local ffmpeg_src="$src_dir/ffmpeg-${FFMPEG_VERSION}"
    if [[ ! -d "$ffmpeg_src" ]]; then
        echo "Error: FFmpeg source directory not found at $ffmpeg_src" >&2
        exit 1
    fi

    # Determine architecture
    local host_arch
    if [[ "$ARCH" == "arm64" ]]; then
        host_arch="aarch64"
    else
        host_arch="x86_64"
    fi

    echo "Configuring FFmpeg..."
    cd "$ffmpeg_src"
    ./configure \
        --prefix="$DEPS_DIR/ffmpeg" \
        --enable-shared \
        --disable-static \
        --disable-doc \
        --disable-programs \
        --enable-videotoolbox \
        --arch="$host_arch" \
        --extra-cflags="-mmacosx-version-min=12.0" \
        --extra-ldflags="-mmacosx-version-min=12.0"

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

    # Download (tar.xz is smaller than zip: ~900MB vs 1.4GB)
    local qt_url="https://download.qt.io/official_releases/qt/6.5/${QT_VERSION}/src/single/qt-everywhere-opensource-src-${QT_VERSION}.tar.xz"
    local qt_src="$src_dir/qt-everywhere-src-${QT_VERSION}"
    if [[ ! -d "$qt_src" ]]; then
        local qt_tar="$OUTPUT_DIR/qt6-src.tar.xz"
        if [[ ! -f "$qt_tar" ]]; then
            echo "Downloading Qt6 $QT_VERSION..."
            curl -L --retry 3 --retry-delay 5 -o "$qt_tar" "$qt_url"
        fi
        echo "Extracting Qt6 $QT_VERSION..."
        tar -xJ -C "$src_dir" -f "$qt_tar"
        rm -f "$qt_tar"
    fi

    # Verify source directory exists
    if [[ ! -d "$qt_src" ]]; then
        echo "Error: Qt source directory not found at $qt_src" >&2
        echo "Contents of $src_dir:" >&2
        ls -la "$src_dir" >&2
        exit 1
    fi

    echo "Configuring Qt6..."
    cd "$build_dir"
    "$qt_src/configure" \
        -prefix "$DEPS_DIR/qt6" \
        -opensource -confirm-license \
        -nomake examples -nomake tests \
        -release \
        -skip qt3d \
        -skip qt5compat \
        -skip qtactiveqt \
        -skip qtcharts \
        -skip qtcoap \
        -skip qtconnectivity \
        -skip qtdatavis3d \
        -skip qtdeclarative \
        -skip qtdoc \
        -skip qtgrpc \
        -skip qthttpserver \
        -skip qtimageformats \
        -skip qtlanguageserver \
        -skip qtlocation \
        -skip qtlottie \
        -skip qtmqtt \
        -skip qtnetworkauth \
        -skip qtopcua \
        -skip qtpositioning \
        -skip qtquick3d \
        -skip qtquick3dphysics \
        -skip qtquickeffectmaker \
        -skip qtquicktimeline \
        -skip qtremoteobjects \
        -skip qtscxml \
        -skip qtsensors \
        -skip qtserialbus \
        -skip qtserialport \
        -skip qtspeech \
        -skip qtvirtualkeyboard \
        -skip qtwayland \
        -skip qtwebchannel \
        -skip qtwebengine \
        -skip qtwebsockets \
        -skip qtwebview \
        -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0

    echo "Building Qt6 with $JOBS jobs..."
    cmake --build . -j"$JOBS"
    cmake --install .

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
        git clone --recursive https://github.com/stdware/qwindowkit.git "$src_dir"
        cd "$src_dir"
        git checkout 1.5.0
        git submodule update --init --recursive
        cd -
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
    cmake --install .

    cd "$PROJECT_DIR"
    echo "QWindowKit installed to $DEPS_DIR/qwindowkit"
}

# --- Main ---
cd "$OUTPUT_DIR"

if [[ "$TARGET" == "all" || "$TARGET" == "ffmpeg" ]]; then
    build_ffmpeg
    echo ""
    echo "=== Packaging FFmpeg ==="
    cd "$OUTPUT_DIR"
    tar -cf - deps/ffmpeg | zstd -T0 -o "$OUTPUT_DIR/ffmpeg-macos-${ARCH}.tar.zst"
    if [[ "$TARGET" == "ffmpeg" ]]; then
        rm -rf deps
    fi
fi

if [[ "$TARGET" == "all" || "$TARGET" == "qt6" ]]; then
    build_qt6
    echo ""
    echo "=== Packaging Qt6 ==="
    cd "$OUTPUT_DIR"
    tar -cf - deps/qt6 | zstd -T0 -o "$OUTPUT_DIR/qt6-macos-${ARCH}.tar.zst"
    if [[ "$TARGET" == "qt6" ]]; then
        rm -rf deps
    fi
fi

if [[ "$TARGET" == "all" || "$TARGET" == "qwindowkit" ]]; then
    if [[ "$TARGET" == "qwindowkit" ]]; then
        if [[ ! -d "$DEPS_DIR/qt6" ]]; then
            echo "Error: Qt6 directory not found at $DEPS_DIR/qt6." >&2
            echo "Please download and extract qt6-macos-${ARCH}.tar.zst first." >&2
            exit 1
        fi
    fi
    build_qwindowkit
    echo ""
    echo "=== Packaging QWindowKit ==="
    cd "$OUTPUT_DIR"
    tar -cf - deps/qwindowkit | zstd -T0 -o "$OUTPUT_DIR/qwindowkit-macos-${ARCH}.tar.zst"
    rm -rf deps
fi

echo ""
echo "=== Done ==="
for file in *.tar.zst; do
    if [[ -f "$file" ]]; then
        echo "Package: $OUTPUT_DIR/$file ($(du -h "$file" | cut -f1))"
    fi
done
