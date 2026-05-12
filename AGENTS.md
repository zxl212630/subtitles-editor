# AGENTS.md

## Build & Run

```bash
cmake -B cmake-build-debug -S .
cmake --build cmake-build-debug
./cmake-build-debug/subtitles-editor            # optional: pass video file path as arg
```

SDK paths default to `~/Tools/Qt/6.5.7`, `~/Tools/Qt/QwindowKit/Qt6`, `~/Tools/ffmpeg/8.0`. Override with `-DQt6_ROOT=…`, `-DQWindowKit_ROOT=…`, `-DFFMPEG_ROOT=…`.

**Note:** CMakeLists.txt says Qt 6.5.7 but CLAUDE.md says 6.5.8. The CMakeLists.txt is the source of truth.

## Pre-Commit

```bash
clang-format -i src/*.cpp include/*.h
cmake --build cmake-build-debug
```

No test suite exists.

## Architecture

C++17 Qt6 macOS desktop app (frameless window via QWindowKit). FFmpeg 8.0 for video/audio decode.

- **AppWindow** — Pimpl, owns everything, wires cross-panel signals in `setupSplitterLayout()`
- **SubtitleTrack** — Shared `QList<SubtitleItem>` data model, emits `dataChanged`
- **MediaPlayer** — Owns `FFmpegDecoder`, `QtAudioOutput`, `SoftwareVideoRenderer`; drives playback timer
- **FFmpegDecoder** — `QThread` subclass, owns all AV contexts, queues decoded frames
- **ConfigManager** — Singleton, reads `config.ini` from `QStandardPaths::AppConfigLocation`
- **SubtitleExporter** — Static methods: `exportToSRT`, `exportToASS`, `exportToVTT`, `exportToPremiereXML`
- **AsrServiceBase** — Abstract ASR interface; `TencentAsrService` impl requires Tencent cloud + Aliyun OSS config

Signal flow: `TimelinePanel → MediaPlayer` (seek/play), `MediaPlayer → TimelinePanel/VideoPreviewPanel` (time/state sync), `SubtitleTrack → VideoPreviewPanel` (subtitle refresh).

## Conventions

- Pimpl pattern for `AppWindow` (struct `Private`, `std::unique_ptr<Private> d`)
- All Qt Widgets classes, no QML
- `#pragma once` for include guards
- Chinese UI strings throughout (window titles, message boxes)
- Stylesheet-based theming (dark palette ~`#151515`/`#262626`/`#0a0a0a`)
- Splitter handles 10px, `#0a0a0a`; all splitters non-collapsible

## Config

Copy `config.ini.template` to `~/Library/Application Support/subtitles-editor/config.ini` (or equivalent `AppConfigLocation`), fill in FFmpeg path, Tencent ASR, and Aliyun OSS credentials. App warns at launch if config is incomplete.