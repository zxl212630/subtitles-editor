# Drag-to-Seek Preview Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add drag-to-seek on the timeline playhead with real-time video preview, where the playhead follows the mouse with zero delay.

**Architecture:** Mouse events on TimelinePanel drive playhead position (immediate update) and throttled video preview seeks. MediaPlayer gets a lightweight `previewSeek()` that only updates the atomic seek target. FFmpegDecoder reuses frame buffers and clears stale frames on seek. The seekPreviewMode is modified to render the first available frame instead of chasing the exact target.

**Tech Stack:** C++17, Qt6, FFmpeg 8.0

---

## File Structure

| File | Responsibility |
|------|---------------|
| `include/FFmpegDecoder.h` | Add `clearVideoQueue()`, reusable `AVFrame*` member, RGBA buffer |
| `src/FFmpegDecoder.cpp` | Frame reuse in decode loop, clear video queue on seek |
| `include/MediaPlayer.h` | Add `previewSeek()` slot, `isPreviewDragging_` flag |
| `src/MediaPlayer.cpp` | Implement previewSeek, modify seekPreviewMode for first-frame rendering |
| `include/TimelinePanel.h` | Add drag state members, mouseMoveEvent/mouseReleaseEvent, `previewSeekRequested` signal, `setVideoFps()` |
| `src/TimelinePanel.cpp` | Drag three-phase logic, throttle control, mouseTracking |
| `src/AppWindow.cpp` | Connect previewSeekRequested → previewSeek, mediaLoaded → setVideoFps |

---

### Task 1: FFmpegDecoder — Frame Reuse

**Files:**
- Modify: `include/FFmpegDecoder.h:76-128`
- Modify: `src/FFmpegDecoder.cpp:401-460`

- [ ] **Step 1: Add reusable frame member and RGBA buffer to header**

In `include/FFmpegDecoder.h`, add to the private section (after line 122, before `QMutex queueFullMutex_`):

```cpp
  // Reusable decode buffers (decoder thread only)
  AVFrame *reusableFrame_ = nullptr;
  QByteArray reusableRgbaBuffer_;
```

- [ ] **Step 2: Initialize and free reusable frame**

In `src/FFmpegDecoder.cpp`, in the `open()` method, after `avcodec_open2` succeeds for video (around line 78), add:

```cpp
  reusableFrame_ = av_frame_alloc();
```

In the `close()` method, before the existing cleanup (around line 190), add:

```cpp
  if (reusableFrame_) {
    av_frame_free(&reusableFrame_);
    reusableFrame_ = nullptr;
  }
```

- [ ] **Step 3: Modify decodeVideoPacket to reuse frame**

In `src/FFmpegDecoder.cpp`, replace the `decodeVideoPacket` method. The key changes:
- Use `reusableFrame_` instead of `av_frame_alloc()`/`av_frame_free()`
- Use `reusableRgbaBuffer_` instead of allocating a new `QByteArray` each time
- Swap (not copy) the RGBA data into the `DecodedVideoFrame`

Replace the entire `decodeVideoPacket` method (starting at line 401) with:

```cpp
bool FFmpegDecoder::decodeVideoPacket(AVPacket *packet) {
  int ret = avcodec_send_packet(videoCodecCtx_, packet);

  auto processFrame = [&](AVFrame *f) {
    qint64 pts = f->pts;
    if (pts == AV_NOPTS_VALUE) {
      pts = f->best_effort_timestamp;
    }
    qint64 ptsMs = static_cast<qint64>(pts * av_q2d(videoTimeBase_) * 1000.0);

    int w = f->width;
    int h = f->height;

    {
      QMutexLocker locker(&metadataMutex_);
      if (!swsCtx_ || videoSize_.width() != w || videoSize_.height() != h) {
        if (swsCtx_) {
          sws_freeContext(swsCtx_);
        }
        swsCtx_ = sws_getContext(w, h, static_cast<AVPixelFormat>(f->format), w,
                                 h, AV_PIX_FMT_RGBA, SWS_BILINEAR, nullptr,
                                 nullptr, nullptr);
        videoSize_ = QSize(w, h);
      }
    }

    // Reuse pre-allocated RGBA buffer
    int bufSize = w * h * 4;
    if (reusableRgbaBuffer_.size() != bufSize) {
      reusableRgbaBuffer_.resize(bufSize);
    }
    uint8_t *dstData[4] = {reinterpret_cast<uint8_t *>(reusableRgbaBuffer_.data()),
                           nullptr, nullptr, nullptr};
    int dstLinesize[4] = {w * 4, 0, 0, 0};
    sws_scale(swsCtx_, f->data, f->linesize, 0, h, dstData, dstLinesize);

    DecodedVideoFrame vf;
    vf.ptsMs = ptsMs;
    vf.width = w;
    vf.height = h;
    vf.rgbaData = reusableRgbaBuffer_;  // QByteArray is implicitly shared (copy-on-write)

    QMutexLocker locker(&videoQueueMutex_);
    videoQueue_.enqueue(std::move(vf));
    queueNotFull_.wakeAll();
  };

  while (true) {
    ret = avcodec_receive_frame(videoCodecCtx_, reusableFrame_);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
      break;
    }
    if (ret < 0) {
      char errbuf[256];
      av_strerror(ret, errbuf, sizeof(errbuf));
      LOG_DEC(warning, "Video decode error:" << errbuf);
      return false;
    }
    processFrame(reusableFrame_);
    av_frame_unref(reusableFrame_);
  }
  return true;
}
```

- [ ] **Step 4: Build to verify compilation**

Run: `cmake --build cmake-build-debug`

Expected: Build succeeds with no errors.

- [ ] **Step 5: Commit**

```bash
git add include/FFmpegDecoder.h src/FFmpegDecoder.cpp
git commit -m "perf(decoder): reuse AVFrame and RGBA buffer in decode loop"
```

---

### Task 2: FFmpegDecoder — Clear Video Queue on Seek

**Files:**
- Modify: `include/FFmpegDecoder.h:58-59`
- Modify: `src/FFmpegDecoder.cpp:208-211`

- [ ] **Step 1: Add clearVideoQueue method declaration**

In `include/FFmpegDecoder.h`, add after `clearAudioQueue()` declaration (line 58):

```cpp
  void clearVideoQueue();
```

- [ ] **Step 2: Implement clearVideoQueue**

In `src/FFmpegDecoder.cpp`, add after the `clearAudioQueue` implementation (after line 388):

```cpp
void FFmpegDecoder::clearVideoQueue() {
  QMutexLocker locker(&videoQueueMutex_);
  videoQueue_.clear();
}
```

- [ ] **Step 3: Modify requestSeek to clear video queue**

In `src/FFmpegDecoder.cpp`, modify `requestSeek` (lines 208-211):

```cpp
void FFmpegDecoder::requestSeek(qint64 targetMs) {
  clearVideoQueue();
  seekTargetMs_.store(targetMs);
  seekRequested_.store(true);
}
```

- [ ] **Step 4: Build to verify**

Run: `cmake --build cmake-build-debug`

Expected: Build succeeds.

- [ ] **Step 5: Commit**

```bash
git add include/FFmpegDecoder.h src/FFmpegDecoder.cpp
git commit -m "feat(decoder): clear video queue on seek to prevent stale frames"
```

---

### Task 3: MediaPlayer — Add previewSeek and Modify seekPreviewMode

**Files:**
- Modify: `include/MediaPlayer.h:29,66-68`
- Modify: `src/MediaPlayer.cpp:132-210`

- [ ] **Step 1: Add previewSeek declaration and isPreviewDragging flag**

In `include/MediaPlayer.h`, add `previewSeek` after `seek()` declaration (line 29):

```cpp
  void previewSeek(qint64 ms);
```

Add `isPreviewDragging_` flag after `seekTargetMs_` (line 68):

```cpp
  bool isPreviewDragging_ = false;
```

- [ ] **Step 2: Implement previewSeek**

In `src/MediaPlayer.cpp`, add after the `seek()` method (after line 157):

```cpp
void MediaPlayer::previewSeek(qint64 ms) {
  if (!decoder_ || !decoder_->hasVideo())
    return;

  if (!isPreviewDragging_) {
    // First call during drag: initialize preview mode
    if (state_ == Playing) {
      pause();
    }
    isPreviewDragging_ = true;
    seekPreviewMode_ = true;
    seekTargetMs_ = ms;
    currentTimeMs_ = ms;
    decoder_->setPlaying(true);
    seekPreviewTimer_.start();
    if (!playbackTimerRunning_) {
      playbackTimer_->start(16);
      playbackTimerRunning_ = true;
    }
  } else {
    // Subsequent calls: just update target (atomic, lightweight)
    seekTargetMs_ = ms;
    currentTimeMs_ = ms;
  }

  emit timeChanged(currentTimeMs_);
}
```

- [ ] **Step 3: Add stopPreviewDragging method**

In `include/MediaPlayer.h`, add in the public section (after `previewSeek`):

```cpp
  void stopPreviewDragging();
```

In `src/MediaPlayer.cpp`, add after `previewSeek`:

```cpp
void MediaPlayer::stopPreviewDragging() {
  if (!isPreviewDragging_)
    return;

  isPreviewDragging_ = false;

  // Use existing seek() for a clean precise seek to final position
  // seek() handles stopping preview mode, re-seeking, etc.
  seek(currentTimeMs_);
}
```

- [ ] **Step 4: Modify seekPreviewMode in onPlaybackTimer to render first frame**

In `src/MediaPlayer.cpp`, in `onPlaybackTimer()`, modify the seekPreviewMode block (lines 174-209). Replace the `while` loop that checks `pts >= seekTargetMs_` with a first-frame strategy:

Replace lines 183-196 with:

```cpp
    // Render the first available frame (don't chase exact target position).
    // This ensures responsive preview during drag scrubbing.
    if (decoder_->videoQueueSize() > 0) {
      auto frame = decoder_->dequeueVideoFrame();
      if (frame && videoRenderer_) {
        videoRenderer_->renderFrame(*frame);
      }

      if (!isPreviewDragging_) {
        // Not dragging: stop after showing one frame (normal seek preview)
        decoder_->setPlaying(false);
        playbackTimer_->stop();
        playbackTimerRunning_ = false;
        seekPreviewMode_ = false;
        LOG_MP(info, "seek preview frame rendered pts=" << (frame ? frame->ptsMs : -1));
      } else {
        // Dragging: keep running, just reset the timer for next batch
        seekPreviewTimer_.start();
      }
      return;
    }
```

- [ ] **Step 5: Build to verify**

Run: `cmake --build cmake-build-debug`

Expected: Build succeeds.

- [ ] **Step 6: Commit**

```bash
git add include/MediaPlayer.h src/MediaPlayer.cpp
git commit -m "feat(player): add previewSeek for drag scrubbing with first-frame strategy"
```

---

### Task 4: TimelinePanel — Drag-to-Seek Interaction

**Files:**
- Modify: `include/TimelinePanel.h:25-37,55-70`
- Modify: `src/TimelinePanel.cpp:415-430`

- [ ] **Step 1: Add drag state members and new declarations to header**

In `include/TimelinePanel.h`, add to signals section (after line 30):

```cpp
  void previewSeekRequested(qint64 ms);
```

Add mouseMoveEvent and mouseReleaseEvent to protected section (after line 37):

```cpp
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
```

Add public method (after `setPlaying`):

```cpp
  void setVideoFps(double fps);
```

Add private members (after `isPlaying_` at line 69):

```cpp
  // Drag-to-seek state
  bool isDragging_ = false;
  bool mousePressed_ = false;
  int dragStartX_ = 0;
  qint64 lastPreviewMs_ = 0;
  double videoFps_ = 25.0;
  static constexpr int DRAG_THRESHOLD_PX = 3;
```

- [ ] **Step 2: Implement setVideoFps**

In `src/TimelinePanel.cpp`, add (e.g., after `setPlaying` implementation):

```cpp
void TimelinePanel::setVideoFps(double fps) {
  if (fps > 0.0) {
    videoFps_ = fps;
  }
}
```

- [ ] **Step 3: Enable mouse tracking and modify mousePressEvent**

In `src/TimelinePanel.cpp`, add `#include <QDateTime>` at the top.

In the constructor, add `setMouseTracking(true)` after the existing setup.

Replace `mousePressEvent` (lines 415-430) with:

```cpp
void TimelinePanel::mousePressEvent(QMouseEvent *event) {
  if (event->button() != Qt::LeftButton)
    return;
  if (event->x() < TRACK_HEAD_WIDTH)
    return;

  mousePressed_ = true;
  isDragging_ = false;
  dragStartX_ = event->x();

  qint64 ms = xToTime(event->x());
  if (ms < 0)
    ms = 0;
  if (ms > totalDurationMs_)
    ms = totalDurationMs_;

  // Update playhead position immediately (visual feedback)
  currentTimeMs_ = ms;
  canvas_->update();
}
```

- [ ] **Step 4: Implement mouseMoveEvent**

Add after `mousePressEvent`:

```cpp
void TimelinePanel::mouseMoveEvent(QMouseEvent *event) {
  if (!mousePressed_)
    return;
  if (event->x() < TRACK_HEAD_WIDTH)
    return;

  // Check if we've crossed the drag threshold
  if (!isDragging_) {
    if (qAbs(event->x() - dragStartX_) < DRAG_THRESHOLD_PX)
      return;
    isDragging_ = true;
    lastPreviewMs_ = currentTimeMs_;
  }

  qint64 ms = xToTime(event->x());
  if (ms < 0)
    ms = 0;
  if (ms > totalDurationMs_)
    ms = totalDurationMs_;

  // Always update playhead position (instant, no delay)
  currentTimeMs_ = ms;
  canvas_->update();

  // Throttle video preview based on frame rate
  qint64 now = QDateTime::currentMSecsSinceEpoch();
  qint64 intervalMs = static_cast<qint64>(1000.0 / videoFps_);
  if (now - lastPreviewMs_ >= intervalMs) {
    lastPreviewMs_ = now;
    emit previewSeekRequested(ms);
  }
}
```

- [ ] **Step 5: Implement mouseReleaseEvent**

Add after `mouseMoveEvent`:

```cpp
void TimelinePanel::mouseReleaseEvent(QMouseEvent *event) {
  if (event->button() != Qt::LeftButton)
    return;

  if (isDragging_) {
    // Drag ended: emit final seek to commit the exact position
    emit timeClicked(currentTimeMs_);
    isDragging_ = false;
  } else {
    // Click (no drag): emit seek as before
    qint64 ms = xToTime(event->x());
    if (ms < 0)
      ms = 0;
    if (ms > totalDurationMs_)
      ms = totalDurationMs_;
    currentTimeMs_ = ms;
    emit timeClicked(ms);
    canvas_->update();
  }

  mousePressed_ = false;
}
```

- [ ] **Step 6: Remove seek from mousePressEvent**

The old `mousePressEvent` emitted `timeClicked` immediately. Now it only records the position. The actual seek is emitted from `mouseReleaseEvent`. Verify that `timeClicked` is still connected to `MediaPlayer::seek` in AppWindow — this handles the final precise seek on mouseUp.

- [ ] **Step 7: Build to verify**

Run: `cmake --build cmake-build-debug`

Expected: Build succeeds.

- [ ] **Step 8: Commit**

```bash
git add include/TimelinePanel.h src/TimelinePanel.cpp
git commit -m "feat(timeline): add drag-to-seek with playhead following mouse"
```

---

### Task 5: AppWindow — Signal Wiring

**Files:**
- Modify: `src/AppWindow.cpp:137-162`

- [ ] **Step 1: Connect previewSeekRequested signal**

In `src/AppWindow.cpp`, after the existing connection `TimelinePanel::timeClicked → MediaPlayer::seek` (line 161-162), add:

```cpp
  // 5a. Timeline drag -> MediaPlayer preview seek
  connect(d->timelinePanel, &TimelinePanel::previewSeekRequested,
          d->mediaPlayer, &MediaPlayer::previewSeek);

  // 5b. Timeline drag end -> MediaPlayer stop preview dragging
  connect(d->timelinePanel, &TimelinePanel::timeClicked, d->mediaPlayer,
          [this](qint64 ms) {
            Q_UNUSED(ms)
            d->mediaPlayer->stopPreviewDragging();
          });
```

Wait — this creates a conflict. `timeClicked` is already connected to `MediaPlayer::seek` (line 161-162). We need `timeClicked` to both call `seek()` (for precise final position) and `stopPreviewDragging()`. The simplest approach: modify `stopPreviewDragging()` to do the precise seek internally, and change the existing `timeClicked → seek` connection to only fire for non-drag clicks.

Actually, `stopPreviewDragging` already does a precise seek internally. So on mouseUp during drag: `timeClicked` fires → `seek()` fires (precise seek) AND `stopPreviewDragging()` fires (which also does a precise seek). This is redundant but harmless — the second seek is a no-op since the position is already correct.

A cleaner approach: in `mouseReleaseEvent`, emit `timeClicked` only for click (not drag), and emit a separate signal for drag end. Let me revise.

In `include/TimelinePanel.h`, add another signal:

```cpp
  void dragSeekFinished(qint64 ms);
```

In `src/TimelinePanel.cpp`, in `mouseReleaseEvent`, change the drag branch:

```cpp
  if (isDragging_) {
    emit dragSeekFinished(currentTimeMs_);
    isDragging_ = false;
  } else {
    // ... existing click handling ...
    emit timeClicked(ms);
  }
```

- [ ] **Step 2: Update the connections**

In `src/AppWindow.cpp`, replace the connections:

```cpp
  // 5. Timeline click -> MediaPlayer seek (non-drag)
  connect(d->timelinePanel, &TimelinePanel::timeClicked, d->mediaPlayer,
          &MediaPlayer::seek);

  // 5a. Timeline drag -> MediaPlayer preview seek
  connect(d->timelinePanel, &TimelinePanel::previewSeekRequested,
          d->mediaPlayer, &MediaPlayer::previewSeek);

  // 5b. Timeline drag end -> MediaPlayer commit final position
  connect(d->timelinePanel, &TimelinePanel::dragSeekFinished, d->mediaPlayer,
          &MediaPlayer::stopPreviewDragging);
```

- [ ] **Step 3: Connect mediaLoaded to setVideoFps**

In `src/AppWindow.cpp`, after the `mediaLoaded → setTotalDuration` connection (line 143-144), add:

```cpp
  // 2a. MediaPlayer -> Timeline: video fps for drag throttle
  connect(d->mediaPlayer, &MediaPlayer::mediaLoaded, d->timelinePanel,
          [this](qint64, QSize) {
            d->timelinePanel->setVideoFps(d->mediaPlayer->decoderFps());
          });
```

This requires `MediaPlayer` to expose `decoderFps()`. Add to `include/MediaPlayer.h` in the public section:

```cpp
  double decoderFps() const;
```

In `src/MediaPlayer.cpp`:

```cpp
double MediaPlayer::decoderFps() const {
  return decoder_ ? decoder_->fps() : 25.0;
}
```

- [ ] **Step 4: Build to verify**

Run: `cmake --build cmake-build-debug`

Expected: Build succeeds.

- [ ] **Step 5: Commit**

```bash
git add src/AppWindow.cpp include/MediaPlayer.h src/MediaPlayer.cpp
git commit -m "feat: wire drag-to-seek signals between timeline and player"
```

---

### Task 6: Integration Test — Manual Verification

- [ ] **Step 1: Build the project**

Run: `cmake --build cmake-build-debug`

Expected: Build succeeds with no errors.

- [ ] **Step 2: Run the application**

Run: `./cmake-build-debug/subtitles-editor`

- [ ] **Step 3: Load a video file**

Drag a video file onto the timeline panel (or use existing load mechanism).

- [ ] **Step 4: Test click-to-seek (regression)**

Click on various positions on the timeline. Verify:
- Playhead jumps to clicked position
- Video shows the frame at that position
- Behavior is identical to before the change

- [ ] **Step 5: Test drag-to-seek**

Press and hold left mouse button on the timeline, then drag slowly:
- Playhead follows mouse with zero delay
- Video preview updates as you drag
- Works in both forward and backward directions

- [ ] **Step 6: Test fast drag**

Drag quickly across the timeline:
- Playhead still follows mouse instantly
- Video updates continuously (may lag slightly behind playhead)
- No freezing, no flashing back to old positions

- [ ] **Step 7: Test drag end**

Release mouse during drag:
- Video stays at the final position
- Does not resume previous playback

- [ ] **Step 8: Test playback + drag interaction**

Start playback, then drag on the timeline:
- Playback pauses
- Drag preview works normally
- On release, stays at final position (does not resume playback)

- [ ] **Step 9: Verify build one final time**

Run: `cmake --build cmake-build-debug`

Expected: Clean build.

- [ ] **Step 10: Commit any fixes**

If any issues were found and fixed during testing, commit them.
