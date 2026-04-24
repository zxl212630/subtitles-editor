# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

> ⚠️ Architecture information may be incomplete or outdated. When making component, signal, or layout changes, sync updates to this file.

## Build Commands

```bash
# Configure (default SDK paths, override with -D flags)
cmake -B cmake-build-debug -S .

# Build
cmake --build cmake-build-debug

# Run
./cmake-build-debug/subtitles-editor
```

## Formatting & Analysis

```bash
# Format code (required before commit)
clang-format -i src/*.cpp include/*.h

# Static analysis
clang-tidy src/*.cpp -- -std=c++17

# Build
cmake --build cmake-build-debug
```

## Architecture

C++17 Qt6 desktop app for video subtitle editing. macOS bundle.

### Key Dependencies

| Dependency | Purpose |
|-----------|---------|
| Qt6 6.5.8 | UI framework (Core, Gui, Widgets) |
| QWindowKit | Custom title bar, frameless window |
| FFmpeg 8.0 | Video/audio decoding |

### Core Components

| Component | Role |
|-----------|---------|
| `AppWindow` | Main window (Pimpl pattern). Owns `SubtitleTrack` and manages layout via QSplitters. |
| `SubtitleTrack` | Shared data model (QObject). Holds `QList<SubtitleItem>`, emits change signals. |
| `SubtitleItem` | Data struct: `id`, `text`, `startMs`, `endMs`, `selected`. |
| `SubtitleListModel` | `QAbstractListModel` adapter over `SubtitleTrack` with custom roles. |
| `SubtitleListPanel` | Left sidebar: search + list view. Emits `itemSelected`, `itemDeleteRequested`. |
| `VideoPreviewPanel` | Right panel: video display with font/size controls. |
| `TimelinePanel` | Bottom panel: ruler, subtitle track, playhead. `PIXELS_PER_SECOND = 100`. |
| `SubtitleExporter` | Static utility. Exports to SRT, ASS, VTT, Premiere Pro XML. |
| `AsrServiceBase` | Abstract ASR interface with `transcribeFinished` / `transcribeProgress` signals. |

### Layout Structure

```
AppWindow
├─ TitleBar (36px fixed)
└─ CentralWidget
   └─ VerticalSplitter
      ├─ TopSplitter (horizontal)
      │  ├─ VideoPreviewPanel (stretch 1, min 400px)
      │  └─ SubtitleListPanel (stretch 0, min 300px)
      └─ TimelinePanel (min 150px, max 400px)
```

All splitters: `setCollapsible(false)`, handle width 10px, color `#0a0a0a`.

## UI Design Tokens

| Token | Hex |
|-------|-----|
| Window background | `#151515` |
| Title bar / panel bg | `#262626` |
| Central / splitter bg | `#0a0a0a` |
| Control / header bg | `#1a1a1a` |
| Hover / pressed bg | `#2a2a2a` |
| Border | `#303030` |
| Dimmed text | `#9ca3af` |

Font: Inter, 12px (titlebar / time labels).

## Pre-Commit Checks

```bash
clang-format -i src/*.cpp include/*.h  # Format
cmake --build cmake-build-debug        # Compile
```

## SDK Paths

Override via CMake `-D` flags (e.g., `-DQt6_ROOT=/path/to/qt`).

| SDK | Default |
|-----|---------|
| Qt6 | `~/Tools/Qt/6.5.8` |
| QWindowKit | `~/Tools/Qt/QwindowKit/Qt6` |
| FFmpeg | `~/Tools/ffmpeg/8.0` |
