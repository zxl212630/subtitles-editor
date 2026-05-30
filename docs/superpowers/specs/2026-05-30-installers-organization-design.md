# Design: Organize Installers by Platform

## Overview

Organize installation files into a dedicated `dist/` directory with platform-specific subdirectories, primarily for GitHub releases and local file management.

## Requirements

1. Create `dist/` directory with platform-specific subdirectories
2. Move existing macOS DMG file to new directory structure
3. Update build script to output to new directory
4. Update CI/CD workflow to use new directory structure
5. Update README with download and troubleshooting information
6. Mark macOS DMG as unsigned (no Apple Developer account)
7. Reserve `dist/windows/` directory for future Windows support

## Design

### 1. Directory Structure

```
dist/
├── macos/
│   └── SubtitlesEditor-1.0.0-macOS-arm64-unsigned.dmg
└── windows/
    └── (future Windows installers)
```

- Existing `SubtitlesEditor-1.0.0-macOS-arm64.dmg` will be moved to `dist/macos/`
- File renamed to include `-unsigned` suffix to indicate unsigned version
- `dist/windows/` directory created but empty (reserved for future)

### 2. Build Script Changes

Modify `scripts/package-macos.sh`:

- Line 9: Change DMG name to `SubtitlesEditor-1.0.0-macOS-arm64-unsigned`
- Lines 248-255: Change output path to `$PROJECT_DIR/dist/macos/$DMG_NAME.dmg`
- Add `mkdir -p "$PROJECT_DIR/dist/macos"` before DMG creation

### 3. CI/CD Workflow Changes

Modify `.github/workflows/release.yml`:

- Line 23: Change file path to `dist/macos/SubtitlesEditor-*-macOS-arm64-unsigned.dmg`

### 4. README Updates

Add the following sections to README.md:

#### Download Section
- Add note that macOS version is unsigned
- Point to GitHub Releases page for downloads

#### Troubleshooting Section
- Add solutions for "cannot be opened because the developer cannot be verified" error:
  1. Method 1: Go to System Preferences > Security & Privacy, click "Open Anyway"
  2. Method 2: Right-click the app, select "Open", click "Open" in popup
  3. Method 3: Run `xattr -cr /Applications/SubtitlesEditor.app` in Terminal

#### Platform Support Section
- Note that only macOS (Apple Silicon) is currently supported
- Windows support is planned

## Verification

1. Verify `dist/macos/` directory exists and contains DMG file
2. Verify `dist/windows/` directory exists (empty)
3. Verify build script outputs to correct location
4. Verify CI/CD workflow references correct path
5. Verify README contains all required information
