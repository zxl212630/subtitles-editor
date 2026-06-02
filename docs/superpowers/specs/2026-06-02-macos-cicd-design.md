# macOS CI/CD Design

## Overview

Replace local packaging with GitHub Actions CI/CD for macOS, supporting both Apple Silicon (arm64) and Intel (x64) architectures. Dependencies (Qt6, FFmpeg, QWindowKit) are compiled once and stored as GitHub Release assets, then reused by the release workflow.

## Scope

- **Platforms**: macOS only (arm64 + x64)
- **Trigger**: Tag push (`v*`) for releases, `workflow_dispatch` for dependency builds
- **Dependencies**: Pre-compiled and stored on GitHub Release

## Architecture

### Two-Workflow Design

```
Workflow 1: build-deps (manual trigger)
├── Matrix: macos-14 (arm64), macos-13 (x64)
├── Compile Qt6 (minimal), FFmpeg, QWindowKit
├── Package as .tar.zst
└── Upload to GitHub Release (tag: deps-v{N})

Workflow 2: release (tag push v*)
├── Matrix: macos-14 (arm64), macos-13 (x64)
├── Download pre-compiled dependencies
├── Build project
├── Package as .dmg
└── Create GitHub Release with both DMGs
```

### File Structure

```
.github/workflows/
├── build-deps.yml          # Dependency compilation (new)
└── release.yml             # Release build (modify existing)

scripts/
├── package-macos.sh        # Existing - modify for CI env
├── build-deps-macos.sh     # New - compile dependencies
└── ...
```

## Workflow 1: build-deps

### Trigger

```yaml
on:
  workflow_dispatch:
    inputs:
      qt_version:
        description: 'Qt version to compile'
        required: false
        default: '6.5.3'
      rebuild:
        description: 'Force rebuild even if cache exists'
        required: false
        default: 'false'
        type: boolean
```

### Matrix Strategy

```yaml
strategy:
  fail-fast: false
  matrix:
    include:
      - arch: arm64
        runner: macos-14
        ffmpeg_arch: arm64
      - arch: x64
        runner: macos-13
        ffmpeg_arch: x86_64
```

### Steps

1. **Checkout code**
2. **Install build tools**: `brew install autoconf automake libtool nasm yasm pkg-config cmake`
3. **Compile FFmpeg** (~10-15 min)
   - Download FFmpeg source
   - Configure with minimal features (no docs, no examples)
   - Build and install to `deps/ffmpeg`
4. **Compile QWindowKit** (~2-3 min)
   - Clone from source
   - Build with CMake against system Qt or downloaded Qt
   - Install to `deps/qwindowkit`
5. **Compile Qt6** (~30-40 min)
   - Download source from `download.qt.io/official_releases/qt/6.5/6.5.9/src/single/qt-everywhere-opensource-src-6.5.9.zip`
   - Configure with minimal modules (see below)
   - Build and install to `deps/qt6`
6. **Package dependencies**
   ```bash
   tar -cf - deps/ | zstd -o deps-macos-{arch}.tar.zst
   ```
7. **Upload to GitHub Release**
   - Create/update release with tag `deps-v{N}`
   - Upload `deps-macos-{arch}.tar.zst`

### Dependency Package Contents

```
deps/
├── qt6/
│   ├── bin/
│   ├── lib/
│   ├── include/
│   └── plugins/
├── ffmpeg/
│   ├── lib/
│   └── include/
└── qwindowkit/
    ├── lib/
    └── include/
```

Estimated size per architecture: ~150-300MB compressed.

## Workflow 2: release

### Trigger

```yaml
on:
  push:
    tags:
      - 'v*'
```

### Matrix Strategy

Same as build-deps: arm64 (macos-14) + x64 (macos-13).

### Steps

1. **Checkout code**
2. **Download dependency package**
   ```bash
   # Download from deps-v{N} release
   gh release download deps-v{N} -p "deps-macos-{arch}.tar.zst"
   zstd -d deps-macos-{arch}.tar.zst | tar -xf -
   ```
3. **Build project**
   ```bash
   cmake -B cmake-build-release -S . \
     -DCMAKE_BUILD_TYPE=Release \
     -DQt6_ROOT=$PWD/deps/qt6 \
     -DFFMPEG_ROOT=$PWD/deps/ffmpeg \
     -DQWindowKit_ROOT=$PWD/deps/qwindowkit
   cmake --build cmake-build-release -j$(sysctl -n hw.ncpu)
   ```
4. **Bundle dependencies** (reuse existing package-macos.sh logic)
   - macdeployqt for Qt frameworks
   - Copy FFmpeg dylibs
   - Fix rpaths
   - Ad-hoc codesign
5. **Create DMG**
6. **Upload artifact** (for matrix aggregation)
7. **Create GitHub Release**
   - Aggregate DMGs from both architectures
   - Generate release notes
   - Upload both DMGs

### Release Output

```
SubtitlesEditor-{version}-macOS-arm64.dmg
SubtitlesEditor-{version}-macOS-x64.dmg
```

## Scripts

### scripts/build-deps-macos.sh (new)

Encapsulates the full dependency build process:

```bash
#!/usr/bin/env bash
# build-deps-macos.sh — Build all dependencies for macOS
# Usage: ./scripts/build-deps-macos.sh --arch arm64|--x64 --output <dir>

Arguments:
  --arch <arm64|x64>    Target architecture
  --output <dir>        Output directory for compiled deps
  --qt-version <ver>    Qt version (default: 6.5.3)
  --jobs <n>            Parallel jobs (default: $(sysctl -n hw.ncpu))
```

### scripts/package-macos.sh (modify)

Add CI-friendly options:

```bash
# New options:
#   --deps-dir <dir>     Use pre-compiled dependencies from this directory
#   --arch <arch>        Target architecture (arm64 or x64)
```

When `--deps-dir` is provided, skip the dependency discovery phase and use the pre-compiled deps directly.

## Key Design Decisions

### Why GitHub Release for dependencies (not Artifact)?

- Artifacts expire after 7 days (free tier)
- Dependencies rarely change, Release is more appropriate
- Can be versioned with tags (`deps-v1`, `deps-v2`, ...)
- 2GB per release is sufficient for two architecture packages

### Why separate dependency build workflow?

- Dependency compilation takes 40-60 minutes
- Only needs to run when upgrading Qt/FFmpeg/QWindowKit
- Saves GitHub Actions minutes for regular releases
- Allows manual control over dependency versions

### Why not universal binaries?

- Universal binaries complicate the build process
- Separate DMGs are clearer for users
- Each architecture can be optimized independently
- Most users know their architecture

## Cost Estimation

| Operation | Runner | Time | Cost (10x macOS) |
|-----------|--------|------|-------------------|
| Deps build (arm64) | macos-14 | ~45 min | ~450 min |
| Deps build (x64) | macos-13 | ~45 min | ~450 min |
| Release build (arm64) | macos-14 | ~10 min | ~100 min |
| Release build (x64) | macos-13 | ~10 min | ~100 min |
| **Total (one-time deps)** | | | **~900 min** |
| **Total (per release)** | | | **~200 min** |

Free tier: 2,000 min/month. First dependency build uses ~900 min. Each release uses ~200 min. Budget allows ~5 releases/month after initial setup.

## Future Enhancements (out of scope)

- Windows build support
- Linux build support (AppImage/deb)
- macOS code signing with Apple Developer ID
- Notarization
- Automatic dependency version updates

## Resolved Decisions

- **Qt version**: 6.5.9 (latest open-source 6.5.x, from https://download.qt.io/official_releases/qt/6.5/6.5.9/src/single/qt-everywhere-opensource-src-6.5.9.zip)
- **QWindowKit source**: Public GitHub repository (URL to be provided during implementation)
- **FFmpeg version**: Latest stable release from ffmpeg.org
- **macOS minimum**: 12.0 (Monterey) via `-DCMAKE_OSX_DEPLOYMENT_TARGET=12.0`
