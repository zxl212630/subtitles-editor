# Video Export Design

## Overview

Add video export capability to the subtitle editor. The primary goal is to burn subtitles into video (hard subtitles), preserving all styling (font, position, background images, rotation, etc.) exactly as seen in the preview.

## Requirements

- **Hard subtitle burn-in**: Subtitles rendered directly onto video frames, style fully preserved, visible on any player, cannot be toggled off
- **FFmpeg C API encoding**: Use FFmpeg's C API directly (not CLI subprocess) for decoding, subtitle rendering, and re-encoding
- **User-selectable export settings**: Encoder, quality (CRF), resolution, frame rate, audio handling
- **Modal progress dialog**: Progress bar, frame count, elapsed time, estimated remaining time, cancel support
- **Unified export dialog**: Single dialog for both video and subtitle export, with collapsible sections

## Architecture

### Approach: Single-thread Sequential Pipeline

```
VideoExporter (QThread)
  for each packet:
    if video -> decode -> QPainter render subtitles
             -> sws RGB->YUV -> encode -> mux
    if audio -> copy/re-encode -> mux
```

Chosen over multi-thread pipeline for simplicity, reliability, and consistency with the existing `FFmpegDecoder` pattern.

### New Files

| File | Role |
|------|------|
| `include/VideoExporter.h` + `src/VideoExporter.cpp` | Export thread: decode -> render -> encode -> mux |
| `include/ExportDialog.h` + `src/ExportDialog.cpp` | Unified export settings dialog (collapsible sections) |
| `include/VideoExportDialog.h` + `src/VideoExportDialog.cpp` | Modal progress dialog during video export |
| `include/SubtitleRenderer.h` + `src/SubtitleRenderer.cpp` | Stateless utility: render subtitles onto QImage |

### Data Flow

```
User clicks "Export..."
    |
    v
ExportDialog (select video/subtitle/settings)
    | confirm
    v
VideoExportDialog (modal progress)
    |
    v
VideoExporter.start(config)
    |
    +-- 1. avformat_open_input (open source video)
    +-- 2. avformat_alloc_output_context2 (create output context)
    +-- 3. Add video stream (init encoder) + audio stream (copy or AAC)
    +-- 4. avformat_write_header
    |
    +-- 5. Main loop (per frame):
    |   +-- Read packet (av_read_frame)
    |   +-- if video:
    |   |   +-- decode -> AVFrame (YUV)
    |   |   +-- sws_scale YUV -> RGB
    |   |   +-- Wrap RGB data as QImage
    |   |   +-- SubtitleRenderer::render(track, image, currentPtsMs)
    |   |   +-- sws_scale RGB -> YUV
    |   |   +-- avcodec_send_frame / avcodec_receive_packet
    |   |   +-- av_interleaved_write_frame
    |   +-- if audio:
    |   |   +-- copy mode -> remux timestamps -> write
    |   |   +-- else -> decode -> resample -> encode AAC -> write
    |   +-- Update progress -> emit progressChanged(percent)
    |
    +-- 6. Flush encoder
    +-- 7. av_write_trailer
    +-- 8. cleanup() -> emit exportFinished()
```

### Component Relationships

```
AppWindow
+-- SubtitleTrack (shared data model)
+-- MediaPlayer -> FFmpegDecoder (used only for video property queries)
+-- [NEW] VideoExporter (independent thread, direct FFmpeg API)
    +-- Reads SubtitleTrack.items() for subtitle list
    +-- Uses SubtitleRenderer to draw subtitles on frames
    +-- Direct FFmpeg C API for encode/decode
```

## VideoExporter

### Config Struct

```cpp
struct VideoExportConfig {
  QString inputPath;          // Source video path
  QString outputPath;         // Output video path

  // Video encoding
  QString videoCodec;         // "h264" | "hevc"
  int crf = 23;               // Quality (0-51, lower = better)
  QString preset = "medium";  // Encoding speed preset

  // Output video properties
  int outputWidth = 0;        // 0 = keep original
  int outputHeight = 0;       // 0 = keep original
  double outputFps = 0.0;     // 0.0 = keep original

  // Audio
  bool copyAudio = true;      // true = stream copy, false = AAC re-encode
  int audioBitRate = 192;     // kbps (only when re-encoding)
};
```

### Class Interface

```cpp
class VideoExporter : public QThread {
  Q_OBJECT

public:
  explicit VideoExporter(QObject *parent = nullptr);

  void setConfig(const VideoExportConfig &config);
  void setSubtitleTrack(const SubtitleTrack *track);
  void requestCancel();

  int progressPercent() const;
  qint64 elapsedMs() const;
  qint64 estimatedRemainingMs() const;

signals:
  void progressChanged(int percent);
  void exportFinished(const QString &outputPath);
  void exportFailed(const QString &error);
  void exportCancelled();

protected:
  void run() override;

private:
  bool openInput();
  bool setupOutput();
  bool initVideoEncoder();
  bool initAudioStream();
  bool processFrames();
  bool processVideoPacket(AVPacket *pkt);
  bool processAudioPacket(AVPacket *pkt);
  void renderSubtitleOnFrame(AVFrame *frame, qint64 ptsMs);
  void flushEncoder();
  void finalize();
  void cleanup();

  VideoExportConfig config_;
  const SubtitleTrack *track_ = nullptr;
  std::atomic<bool> cancelRequested_{false};

  // FFmpeg contexts
  AVFormatContext *inputFmtCtx_ = nullptr;
  AVFormatContext *outputFmtCtx_ = nullptr;
  AVCodecContext *videoEncCtx_ = nullptr;
  AVCodecContext *audioDecCtx_ = nullptr;
  AVCodecContext *audioEncCtx_ = nullptr;
  SwsContext *swsToRgb_ = nullptr;
  SwsContext *swsFromRgb_ = nullptr;
  SwsContext *swsScale_ = nullptr;       // Resolution scaling
  SwrContext *swrCtx_ = nullptr;

  int inputVideoStream_ = -1;
  int inputAudioStream_ = -1;
  int outputVideoStream_ = -1;
  int outputAudioStream_ = -1;
  bool audioCopyMode_ = true;

  // Progress tracking
  QElapsedTimer elapsedTimer_;
  qint64 totalFrames_ = 0;
  qint64 processedFrames_ = 0;
};
```

### Cancel Mechanism

- `cancelRequested_` atomic flag, checked every frame in main loop
- On cancel: skip `av_write_trailer`, call `cleanup()`, delete incomplete output file
- Emit `exportCancelled()` signal

### Resource Cleanup (RAII pattern in cleanup())

```cpp
void VideoExporter::cleanup() {
  if (swsToRgb_)     { sws_freeContext(swsToRgb_); swsToRgb_ = nullptr; }
  if (swsFromRgb_)   { sws_freeContext(swsFromRgb_); swsFromRgb_ = nullptr; }
  if (swsScale_)     { sws_freeContext(swsScale_); swsScale_ = nullptr; }
  if (swrCtx_)       { swr_free(&swrCtx_); }
  if (videoEncCtx_)  { avcodec_free_context(&videoEncCtx_); }
  if (audioEncCtx_)  { avcodec_free_context(&audioEncCtx_); }
  if (audioDecCtx_)  { avcodec_free_context(&audioDecCtx_); }
  if (outputFmtCtx_) { avformat_free_context(outputFmtCtx_); outputFmtCtx_ = nullptr; }
  if (inputFmtCtx_)  { avformat_close_input(&inputFmtCtx_); }
}
```

## SubtitleRenderer

Extracted from `SoftwareVideoRenderer::paintEvent()` subtitle drawing logic into a stateless utility class.

### Interface

```cpp
class SubtitleRenderer {
public:
  static void render(const SubtitleTrack &track,
                     QImage &image,
                     qint64 currentPtsMs,
                     const QSize &videoSize);

private:
  static void renderItem(QPainter &painter,
                         const SubtitleItem &item,
                         const SubtitleTrack &track,
                         const QSize &videoSize);

  static void drawNinePatch(QPainter &painter,
                            const QImage &src,
                            const QRect &target,
                            const QMargins &margins);

  static QRect calculateItemRect(const SubtitleItem &item,
                                  const SubtitleTrack &track,
                                  const QSize &videoSize);

  static QFont buildFont(const SubtitleItem &item,
                         const SubtitleTrack &track,
                         const QSize &videoSize);
};
```

### Render Flow

```
render(track, image, ptsMs, videoSize):
  1. Create QPainter(&image)
  2. Iterate track.items():
     - Filter: item.startMs <= ptsMs < item.endMs
     - For each match: renderItem(painter, item, track, videoSize)
  3. painter.end()

renderItem(painter, item, track, videoSize):
  1. Build font (merge per-item style + global defaults)
     - Scale font size by output height: fontSize * (videoSize.height / referenceHeight)
  2. Calculate subtitle bounding box:
     - item.rectX/Y/W/H (normalized) -> pixel coordinates
     - Apply rotation transform
  3. Draw background:
     - If speaker has bgImage -> drawNinePatch
     - Else draw semi-transparent fill
  4. Draw text:
     - painter.setFont(font)
     - painter.drawText(rect, alignment, text)
```

### SoftwareVideoRenderer Refactor

After extraction, `SoftwareVideoRenderer::paintEvent()` calls `SubtitleRenderer::render()` for subtitle overlay. Both preview and export use the same rendering logic, ensuring visual consistency.

### Font Size Scaling

```
scaledFontSize = item.fontSize * (outputHeight / referenceHeight)
```

Where `referenceHeight` is the video's original height, or 1080 if unavailable.

## UI Components

### ExportDialog (Unified Export Dialog)

Single-column layout with collapsible sections:

```
+----------------------------------------------+
|  Export                                       |
+----------------------------------------------+
|                                              |
|  [x] Export Video                             |
|  v Video Settings                             |
|  +----------------------------------------+  |
|  | Encoder:   [H.264 v]                    |  |
|  | Quality:   [====o====] CRF: 23          |  |
|  | Speed:     [medium v]                   |  |
|  | Resolution:[Original 1920x1080 v]       |  |
|  | Frame Rate:[Original 25fps v]           |  |
|  | [x] Copy audio stream (no re-encode)   |  |
|  |   Bitrate: [192 kbps v]                |  |
|  +----------------------------------------+  |
|                                              |
|  [x] Export Subtitles                         |
|  v Subtitle Settings                          |
|  +----------------------------------------+  |
|  | Format: ( )SRT (*)ASS ( )TXT            |  |
|  |         ( )Premiere XML ( )FCPXML       |  |
|  +----------------------------------------+  |
|                                              |
|  Output Path:                                 |
|  [/path/to/output] [Browse]                   |
|                                              |
|  ------------------------------------------  |
|                          [Cancel]   [Export]  |
+----------------------------------------------+
```

**Collapsed state:**

```
|  [x] Export Video                             |
|  > Video Settings                   (collapsed)|
|                                              |
|  [x] Export Subtitles                         |
|  > Subtitle Settings                (collapsed)|
```

### Interaction Logic

- Two independent checkboxes: "Export Video" and "Export Subtitles"
- Unchecking -> auto-collapse and disable corresponding settings section
- Checking -> auto-expand corresponding settings section
- Section headers are clickable for manual expand/collapse (accordion)
- At least one must be checked; "Export" button disabled if neither is checked
- Output path file dialog filter changes based on selection:
  - Video only -> `"Video Files (*.mp4 *.mkv)"`
  - Subtitle only -> filter based on selected subtitle format
  - Both -> select video path first, subtitle file goes to same directory

### VideoExportDialog (Progress Dialog)

```
+----------------------------------------------+
|  Exporting Video...                           |
+----------------------------------------------+
|                                              |
|  [=========================........] 62%      |
|                                              |
|  Processed: 1860 / 3000 frames               |
|  Elapsed: 02:15                              |
|  Estimated remaining: 01:22                  |
|                                              |
|                [Cancel Export]                |
+----------------------------------------------+
```

- Modal dialog (`QDialog::exec()`)
- Progress bar + frame count + elapsed time + estimated remaining
- Estimated remaining = elapsed / percent * (100 - percent)
- Cancel button -> `VideoExporter::requestCancel()` -> wait for thread -> close
- Dialog does not auto-close; driven by VideoExporter signals:
  - `exportFinished` -> show success message -> close
  - `exportFailed` -> show error message -> close
  - `exportCancelled` -> close

### Menu Integration

Single entry point for all exports:

```
File Menu
+-- New Project
+-- Open Project
+-- ...
+-- Export...     <- Opens unified ExportDialog
+-- ...

Title bar export button -> Same ExportDialog
```

No quick-export submenus. All export operations go through the unified dialog.

## Export Execution Flow

```
Click "Export"
  |
  +-- Subtitle only -> Call SubtitleExporter directly -> success/failure toast
  |
  +-- Video only -> VideoExportDialog (progress)
  |   +-- VideoExporter thread
  |
  +-- Both -> VideoExportDialog (progress)
      +-- Export subtitle file first (fast, progress 0-5%)
      +-- Export video (progress 5-100%)
```

## Error Handling

### Strategy

Every FFmpeg API call wrapped with error check:

```cpp
if (ret < 0) {
  char errbuf[AV_ERROR_MAX_STRING_SIZE];
  av_strerror(ret, errbuf, sizeof(errbuf));
  emit exportFailed(tr("Failed to initialize video encoder: %1")
                        .arg(QString::fromUtf8(errbuf)));
  return false;
}
```

- Each pipeline stage returns `false` -> `run()` calls `cleanup()` + `emit exportFailed()`
- Cancel is not an error -> `emit exportCancelled()` path
- All error messages wrapped in `tr()` for i18n

### Edge Cases

| Scenario | Handling |
|----------|----------|
| Source has no audio stream | Skip audio, encode video+subtitles only |
| Source has no video stream | `exportFailed("Source file has no video stream")` |
| Output resolution differs from source | Add `SwsContext` for scaling, subtitle coords calculated against output resolution |
| Output FPS differs from source | Recalculate PTS timestamps, align subtitle timing to output frame rate |
| Source codec == output codec | Full decode -> render subtitles -> re-encode (cannot copy, subtitles must be burned) |
| Disk space insufficient | FFmpeg write failure -> `exportFailed` |
| Output path not writable | Check before encoding starts, fail early |
| Subtitle list is empty | Allow export (pure video transcode), no confirmation prompt |
| Source file deleted during export | `av_read_frame` fails -> `exportFailed` |

## I18n and Theme

### Internationalization

- All user-visible strings wrapped in `tr()`
- `changeEvent()` handles `QEvent::LanguageChange` to refresh text

### Theme Support

- Use palette colors, no hardcoded color values
- Collapsible section styling via QSS in `ThemeManager`
- `changeEvent()` handles `QEvent::PaletteChange` to repaint

## CMakeLists.txt Changes

Add new source files (no new library dependencies needed):

```cmake
# New sources
src/VideoExporter.cpp
src/VideoExportDialog.cpp
src/ExportDialog.cpp
src/SubtitleRenderer.cpp

# New headers
include/VideoExporter.h
include/VideoExportDialog.h
include/ExportDialog.h
include/SubtitleRenderer.h
```

Existing `avcodec`, `avformat`, `avutil`, `swscale`, `swresample` dependencies are sufficient.
