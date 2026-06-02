# macOS CI/CD Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Set up GitHub Actions CI/CD for macOS (arm64 + x64) with pre-compiled dependencies stored on GitHub Release.

**Architecture:** Two-workflow design: `build-deps` (manual trigger) compiles Qt6/FFmpeg/QWindowKit and uploads to GitHub Release; `release` (tag push) downloads deps, builds project, and publishes DMGs. A helper script `build-deps-macos.sh` encapsulates the dependency compilation.

**Tech Stack:** GitHub Actions, CMake, Qt 6.5.9, FFmpeg 8.0, QWindowKit 1.5.0, zstd

---

## File Structure

| File | Action | Purpose |
|------|--------|---------|
| `scripts/build-deps-macos.sh` | Create | Compile all dependencies for a given architecture |
| `scripts/package-macos.sh` | Modify | Add `--deps-dir` and `--arch` flags for CI |
| `.github/workflows/build-deps.yml` | Create | Workflow to compile and upload dependencies |
| `.github/workflows/release.yml` | Modify | Matrix build for arm64+x64, download deps, publish |

---

### Task 1: Create `scripts/build-deps-macos.sh`

**Files:**
- Create: `scripts/build-deps-macos.sh`

- [ ] **Step 1: Create the script skeleton with argument parsing**

```bash
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
        *)                echo "Unknown argument: $1" >&2; usage ;;
    esac
done

# --- Validate ---
if [[ -z "$ARCH" ]] || [[ -z "$OUTPUT_DIR" ]]; then
    echo "Error: --arch and --output are required" >&2
    usage
fi

if [[ "$ARCH" != "arm64" && "$ARCH" != "x64" ]]; then
    echo "Error: --arch must be arm64 or x64" >&2
    exit 1
fi

mkdir -p "$OUTPUT_DIR"
DEPS_DIR="$OUTPUT_DIR/deps"
mkdir -p "$DEPS_DIR"

echo "=== Building dependencies for macOS $ARCH ==="
echo "Qt: $QT_VERSION | FFmpeg: $FFMPEG_VERSION | Jobs: $JOBS"
echo "Output: $OUTPUT_DIR"
```

- [ ] **Step 2: Add FFmpeg compilation function**

Append to the script:

```bash
# --- Build FFmpeg ---
build_ffmpeg() {
    echo ""
    echo "=== Building FFmpeg $FFMPEG_VERSION ==="
    local src_dir="$OUTPUT_DIR/ffmpeg-src"
    local build_dir="$OUTPUT_DIR/ffmpeg-build"
    mkdir -p "$src_dir" "$build_dir"

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
```

- [ ] **Step 3: Add Qt6 compilation function**

Append to the script:

```bash
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
        echo "Downloading Qt6 $QT_VERSION..."
        curl -L "$qt_url" | unzip -q -d "$src_dir"
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
```

- [ ] **Step 4: Add QWindowKit compilation function**

Append to the script:

```bash
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
```

- [ ] **Step 5: Add main execution and packaging**

Append to the script:

```bash
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
```

- [ ] **Step 6: Make executable and verify syntax**

Run:
```bash
chmod +x scripts/build-deps-macos.sh
bash -n scripts/build-deps-macos.sh
```
Expected: No output (syntax OK)

- [ ] **Step 7: Commit**

```bash
git add scripts/build-deps-macos.sh
git commit -m "ci: add build-deps-macos.sh for compiling Qt/FFmpeg/QWindowKit"
```

---

### Task 2: Modify `scripts/package-macos.sh` for CI

**Files:**
- Modify: `scripts/package-macos.sh`

- [ ] **Step 1: Add new CLI options for `--deps-dir` and `--arch`**

In the argument parsing section (around line 40-53), add new options:

```bash
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
        -h|--help)      usage ;;
        *)              echo "Unknown argument: $1" >&2; usage ;;
    esac
done
```

- [ ] **Step 2: Add deps-dir resolution logic after argument parsing**

After the path validation block (around line 73), add:

```bash
# --- Resolve dependencies from --deps-dir ---
if [[ -n "$DEPS_DIR" ]]; then
    [[ -d "$DEPS_DIR/deps/qt6" ]]     || { echo "Error: $DEPS_DIR/deps/qt6 not found" >&2; exit 1; }
    [[ -d "$DEPS_DIR/deps/ffmpeg" ]]   || { echo "Error: $DEPS_DIR/deps/ffmpeg not found" >&2; exit 1; }
    [[ -d "$DEPS_DIR/deps/qwindowkit" ]] || { echo "Error: $DEPS_DIR/deps/qwindowkit not found" >&2; exit 1; }
    QT_ROOT="$DEPS_DIR/deps/qt6"
    FFMPEG_ROOT="$DEPS_DIR/deps/ffmpeg"
    QWINDOWKIT_ROOT="$DEPS_DIR/deps/qwindowkit"
fi
```

- [ ] **Step 3: Add architecture-specific DMG naming**

Where `DMG_NAME` is set (line 9), update to incorporate architecture:

```bash
DMG_NAME="SubtitlesEditor-1.0.0-macOS-${TARGET_ARCH:-$(uname -m)}-unsigned"
```

- [ ] **Step 4: Update the `usage()` function to document new options**

Add to the usage text:

```
  --deps-dir <dir>     Use pre-compiled dependencies from this directory
  --arch <arch>        Target architecture: arm64 or x64 (default: host arch)
```

- [ ] **Step 5: Verify script syntax**

Run:
```bash
bash -n scripts/package-macos.sh
```
Expected: No output (syntax OK)

- [ ] **Step 6: Commit**

```bash
git add scripts/package-macos.sh
git commit -m "ci: add --deps-dir and --arch flags to package-macos.sh"
```

---

### Task 3: Create `.github/workflows/build-deps.yml`

**Files:**
- Create: `.github/workflows/build-deps.yml`

- [ ] **Step 1: Write the workflow file**

```yaml
name: Build Dependencies

on:
  workflow_dispatch:
    inputs:
      qt_version:
        description: 'Qt version to compile'
        required: false
        default: '6.5.9'
      ffmpeg_version:
        description: 'FFmpeg version to compile'
        required: false
        default: '8.0'
      deps_tag:
        description: 'Tag for the dependency release (e.g. deps-v1)'
        required: true
        default: 'deps-v1'

permissions:
  contents: write

jobs:
  build-deps:
    runs-on: ${{ matrix.runner }}
    strategy:
      fail-fast: false
      matrix:
        include:
          - arch: arm64
            runner: macos-14
          - arch: x64
            runner: macos-13

    steps:
      - uses: actions/checkout@v4

      - name: Install build tools
        run: |
          brew install autoconf automake libtool nasm yasm pkg-config cmake zstd

      - name: Build dependencies
        run: |
          ./scripts/build-deps-macos.sh \
            --arch ${{ matrix.arch }} \
            --output ${{ runner.temp }}/deps-output \
            --qt-version ${{ inputs.qt_version }} \
            --ffmpeg-version ${{ inputs.ffmpeg_version }} \
            --jobs $(sysctl -n hw.ncpu)

      - name: Upload dependency package
        env:
          GH_TOKEN: ${{ secrets.GITHUB_TOKEN }}
        run: |
          PACKAGE="${{ runner.temp }}/deps-output/deps-macos-${{ matrix.arch }}.tar.zst"
          if [[ ! -f "$PACKAGE" ]]; then
            echo "Error: Package not found at $PACKAGE" >&2
            exit 1
          fi
          echo "Uploading $(du -h "$PACKAGE" | cut -f1) package..."
          gh release upload "${{ inputs.deps_tag }}" "$PACKAGE" \
            --clobber \
            --repo "${{ github.repository }}"
```

- [ ] **Step 2: Create the GitHub Release for dependencies (one-time setup)**

This is a manual step the user runs once:
```bash
gh release create deps-v1 --title "Dependencies v1" --notes "Pre-compiled dependencies for macOS arm64 and x64"
```

- [ ] **Step 3: Verify YAML syntax**

Run:
```bash
python3 -c "import yaml; yaml.safe_load(open('.github/workflows/build-deps.yml'))"
```
Expected: No output (valid YAML)

- [ ] **Step 4: Commit**

```bash
git add .github/workflows/build-deps.yml
git commit -m "ci: add build-deps workflow for compiling Qt/FFmpeg/QWindowKit"
```

---

### Task 4: Modify `.github/workflows/release.yml`

**Files:**
- Modify: `.github/workflows/release.yml`

- [ ] **Step 1: Replace the entire workflow with matrix-based release**

```yaml
name: Release

on:
  push:
    tags:
      - 'v*'

permissions:
  contents: write

jobs:
  build:
    runs-on: ${{ matrix.runner }}
    strategy:
      fail-fast: false
      matrix:
        include:
          - arch: arm64
            runner: macos-14
          - arch: x64
            runner: macos-13

    steps:
      - uses: actions/checkout@v4

      - name: Install tools
        run: |
          brew install zstd

      - name: Find latest deps tag
        id: find-deps
        env:
          GH_TOKEN: ${{ secrets.GITHUB_TOKEN }}
        run: |
          # Find the latest deps-v* tag
          DEPS_TAG=$(gh release list --limit 100 --json tagName -q \
            '[.[] | select(.tagName | startswith("deps-v"))] | sort_by(.tagName) | last | .tagName')
          if [[ -z "$DEPS_TAG" ]]; then
            echo "Error: No deps release found. Run build-deps workflow first." >&2
            exit 1
         
          echo "tag=$DEPS_TAG" >> "$GITHUB_OUTPUT"
          echo "Using dependencies from: $DEPS_TAG"

      - name: Download dependencies
        env:
          GH_TOKEN: ${{ secrets.GITHUB_TOKEN }}
        run: |
          gh release download "${{ steps.find-deps.outputs.tag }}" \
            -p "deps-macos-${{ matrix.arch }}.tar.zst" \
            --repo "${{ github.repository }}"
          zstd -d "deps-macos-${{ matrix.arch }}.tar.zst"
          tar -xf "deps-macos-${{ matrix.arch }}.tar"

      - name: Build and package
        run: |
          ./scripts/package-macos.sh \
            --deps-dir "$PWD" \
            --arch "${{ matrix.arch }}"

      - name: Upload DMG artifact
        uses: actions/upload-artifact@v4
        with:
          name: dmg-${{ matrix.arch }}
          path: dist/macos/*.dmg

  release:
    needs: build
    runs-on: ubuntu-latest
    steps:
      - name: Download all DMGs
        uses: actions/download-artifact@v4
        with:
          path: artifacts
          pattern: dmg-*

      - name: Create Release
        uses: softprops/action-gh-release@v2
        with:
          files: artifacts/**/*.dmg
          generate_release_notes: true
          draft: false
          prerelease: false
        env:
          GITHUB_TOKEN: ${{ secrets.GITHUB_TOKEN }}
```

- [ ] **Step 2: Verify YAML syntax**

Run:
```bash
python3 -c "import yaml; yaml.safe_load(open('.github/workflows/release.yml'))"
```
Expected: No output (valid YAML)

- [ ] **Step 3: Commit**

```bash
git add .github/workflows/release.yml
git commit -m "ci: update release workflow with matrix build for arm64+x64"
```

---

### Task 5: End-to-End Verification

- [ ] **Step 1: Run shellcheck on all scripts**

Run:
```bash
shellcheck scripts/build-deps-macos.sh scripts/package-macos.sh
```
Expected: No errors (warnings acceptable for complex bash)

- [ ] **Step 2: Validate all YAML files**

Run:
```bash
python3 -c "
import yaml, sys
for f in ['.github/workflows/build-deps.yml', '.github/workflows/release.yml']:
    try:
        yaml.safe_load(open(f))
        print(f'{f}: OK')
    except Exception as e:
        print(f'{f}: ERROR - {e}')
        sys.exit(1)
"
```

- [ ] **Step 3: Test package-macos.sh locally with --help**

Run:
```bash
./scripts/package-macos.sh --help
```
Expected: Help text showing new `--deps-dir` and `--arch` options

- [ ] **Step 4: Test build-deps-macos.sh locally with --help**

Run:
```bash
./scripts/build-deps-macos.sh --help
```
Expected: Help text showing all options

- [ ] **Step 5: Final commit if any fixes were made**

```bash
git add -A
git commit -m "ci: fix linting issues in CI scripts"
```
