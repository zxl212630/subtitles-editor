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

## Runtime Debugging & Interaction

### 1. Logging & Execution
When debugging runtime issues, use `nohup` to start the application and collect logs:
```bash
nohup ./cmake-build-debug/subtitles-editor > /tmp/startup.log 2>&1 &
```
Analyze `/tmp/startup.log` (Qt warnings, FFmpeg errors, custom `qDebug()` output) to diagnose problems.

### 2. Visual Feedback (Screenshots)
When UI verification or error dialog inspection is needed, invoke the system screenshot tool:
- **macOS**: `screencapture -i /tmp/asr_screenshot.png`
- **Action**: Instruct the user to "Select the relevant area in the pop-up tool" and wait for the capture.

### 3. Investigation Workflow
- **Capture**: Run app with logging → Reproduce issue → Collect logs/screenshots.
- **Analyze**: Cross-reference logs with code logic to identify root causes.
- **Verify**: Re-run and confirm logs are clear after fix.

## Architecture

C++17 Qt6 desktop app for video subtitle editing. macOS bundle.

### Key Dependencies

| Dependency | Purpose |
|-----------|---------|
| Qt6 6.5.7 | UI framework (Core, Gui, Widgets) |
| QWindowKit | Custom title bar, frameless window |
| FFmpeg 8.0 | Video/audio decoding |

### Core Components

| Component | Role |
|-----------|---------|
| `AppWindow` | Main window (Pimpl pattern). Owns `SubtitleTrack` and manages layout via QSplitters. |
| `SubtitleTrack` | Shared data model (QObject). Holds `QList<SubtitleItem>`, emits change signals. |
| `SubtitleItem` | Data struct: `id`, `text`, `startMs`, `endMs`, `selected`. |
| `SubtitleListModel` | `QAbstractListModel` adapter over `SubtitleTrack` with custom roles. |
| `SubtitleListPanel` | Left sidebar: search + list view. |
| `VideoPreviewPanel` | Right panel: video display with overlay and format controls. |
| `TimelinePanel` | Bottom panel: ruler, tracks, and playhead. Supports zoom/scroll. |
| `SubtitleExporter` | Static utility. Exports to SRT, ASS, VTT, Premiere Pro XML. |
| `AsrServiceBase` | Abstract ASR interface for automated transcription. |

### Layout Structure

```
AppWindow
├─ TitleBar (Fixed height)
└─ CentralWidget
   └─ VerticalSplitter
      ├─ TopSplitter (Horizontal)
      │  ├─ VideoPreviewPanel
      │  └─ SubtitleListPanel
      └─ TimelinePanel
```

All splitters are non-collapsible.

## Pre-Commit Checks

1. **Format**: `clang-format -i src/*.cpp include/*.h`
2. **Compile**: `cmake --build cmake-build-debug`

## Multi-language / Internationalization (I18n)

All user-facing text must support translation and dynamic language changes:
- **String wrapping**: Wrap all user-visible strings in `tr()` (e.g., `tr("字幕")`, `tr("视频")`, `tr("属性")`).
- **Dynamic switching**: Any custom widget that does drawing or caches translated text must implement `void changeEvent(QEvent *event) override`.
- **Event Handling**: Inside `changeEvent()`, check for `QEvent::LanguageChange`. On this event, trigger updates (e.g., `update()` or custom translation refresh functions) to repaint or reload localized text immediately.

## Theme & Styling (Dynamic Theme)

The application supports dynamic adjustment of theme modes (e.g., Dark/Light) and primary color palettes. All new UI components must support this:
- **Theme Propagation**: Ensure custom widgets respond to theme or palette change events.
- **Dynamic Updates**: Custom styling (such as QSS stylesheets, palette changes, or custom painting) must be re-applied or repainted when the theme or primary color changes.
- **Event Handling**: Custom widgets that cache styling values or use stylesheets should handle dynamic theme updates, for instance by checking for `QEvent::PaletteChange`, `QEvent::StyleChange`, or responding to global theme changed signals.

## SDK Paths

Override via CMake `-D` flags (e.g., `-DQt6_ROOT=/path/to/qt`).

| SDK | Default |
|-----|---------|
| Qt6 | `~/Tools/Qt/6.5.7` |
| QWindowKit | `~/Tools/Qt/QwindowKit/Qt6` |
| FFmpeg | `~/Tools/ffmpeg/8.0` |
