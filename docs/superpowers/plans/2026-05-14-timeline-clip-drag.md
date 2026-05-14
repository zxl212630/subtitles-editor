# Timeline Clip Drag Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add clip drag-move and edge-resize to TimelinePanel's subtitle track.

**Architecture:** Extend TimelinePanel's mouse event handling with a state machine (Idle/ClipMove/ClipResizeLeft/ClipResizeRight/Seek). Two cursor SVGs added to resources. Drag uses local temp variables for preview, commits via `track_->updateItem()` only on mouse release.

**Tech Stack:** C++17, Qt6, QPainter, QCursor, SVG resources

---

## File Structure

| File | Action | Purpose |
|------|--------|---------|
| `resources/icons/resize-left.svg` | Create | Left resize cursor SVG |
| `resources/icons/resize-right.svg` | Create | Right resize cursor SVG |
| `resources/resources.qrc` | Modify | Register the two new SVGs |
| `include/TimelinePanel.h` | Modify | Add ClipInteractionMode enum, drag state members, helper methods |
| `src/TimelinePanel.cpp` | Modify | Implement interaction logic in mouse handlers, cursor management, draw with temp values |

---

### Task 1: Create cursor SVGs and register in resources.qrc

**Files:**
- Create: `resources/icons/resize-left.svg`
- Create: `resources/icons/resize-right.svg`
- Modify: `resources/resources.qrc`

- [ ] **Step 1: Create resize-left.svg**

Create file `resources/icons/resize-left.svg`:

```svg
<svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 16 16">
  <rect x="1" y="1" width="3" height="14" rx="1" fill="#ffffff" stroke="#000000" stroke-width="1"/>
  <path d="M12 5 L8 8 L12 11" fill="none" stroke="#000000" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"/>
</svg>
```

- [ ] **Step 2: Create resize-right.svg**

Create file `resources/icons/resize-right.svg`:

```svg
<svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 16 16">
  <rect x="12" y="1" width="3" height="14" rx="1" fill="#ffffff" stroke="#000000" stroke-width="1"/>
  <path d="M4 5 L8 8 L4 11" fill="none" stroke="#000000" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"/>
</svg>
```

- [ ] **Step 3: Register SVGs in resources.qrc**

In `resources/resources.qrc`, add the two new entries inside the `<qresource>` block:

```xml
<file>icons/resize-left.svg</file>
<file>icons/resize-right.svg</file>
```

- [ ] **Step 4: Build to verify resources compile**

Run: `cmake --build cmake-build-debug`

---

### Task 2: Add state enums and member variables to TimelinePanel.h

**Files:**
- Modify: `include/TimelinePanel.h`

- [ ] **Step 1: Add ClipInteractionMode enum**

In `include/TimelinePanel.h`, add the enum inside the `TimelinePanel` class, after the `PlayheadAnchor` enum (line 16):

```cpp
  enum class ClipInteractionMode {
    Idle,
    ClipMove,
    ClipResizeLeft,
    ClipResizeRight,
    Seek
  };
```

- [ ] **Step 2: Add member variables for clip drag state**

In the private section at the end of the class, add:

```cpp
  // Clip drag/resize state
  ClipInteractionMode clipMode_ = ClipInteractionMode::Idle;
  QString dragTargetId_;          // id of the clip being dragged
  qint64 dragOrigStartMs_ = 0;    // original startMs before drag
  qint64 dragOrigEndMs_ = 0;      // original endMs before drag
  qint64 dragTempStartMs_ = 0;    // current temp startMs during drag
  qint64 dragTempEndMs_ = 0;      // current temp endMs during drag
  int dragEdgeThreshold_ = 6;     // px from edge to trigger resize
```

- [ ] **Step 3: Add helper method declarations**

In the private section, add:

```cpp
  ClipInteractionMode hitTestClip(int mouseX, int mouseY, QString *outId) const;
  void updateClipCursor(int mouseX, int mouseY);
  void resetClipCursor();
```

- [ ] **Step 4: Build to verify header compiles**

Run: `cmake --build cmake-build-debug`

---

### Task 3: Implement hitTestClip helper

**Files:**
- Modify: `src/TimelinePanel.cpp`

- [ ] **Step 1: Implement hitTestClip**

Add at the end of `src/TimelinePanel.cpp` (before closing), or in a logical spot near the other helpers:

```cpp
TimelinePanel::ClipInteractionMode
TimelinePanel::hitTestClip(int mouseX, int mouseY, QString *outId) const {
  // Only handle clicks in the subtitle track area
  int trackY = RULER_HEIGHT;
  if (mouseY < trackY || mouseY >= trackY + SUBTITLE_TRACK_HEIGHT)
    return ClipInteractionMode::Idle;
  if (mouseX < TRACK_HEAD_WIDTH)
    return ClipInteractionMode::Idle;
  if (!track_)
    return ClipInteractionMode::Idle;

  qint64 clickMs = xToTime(mouseX);

  // Find which clip was clicked
  for (const auto &item : track_->items()) {
    int clipX = timeToX(item.startMs);
    int clipEndX = timeToX(item.endMs);

    // Must be within the clip's horizontal bounds
    if (mouseX < clipX || mouseX > clipEndX)
      continue;

    *outId = item.id;

    // Check if near left edge
    if (mouseX - clipX <= dragEdgeThreshold_) {
      return ClipInteractionMode::ClipResizeLeft;
    }
    // Check if near right edge
    if (clipEndX - mouseX <= dragEdgeThreshold_) {
      return ClipInteractionMode::ClipResizeRight;
    }
    // Otherwise it's a move
    return ClipInteractionMode::ClipMove;
  }

  return ClipInteractionMode::Idle;
}
```

- [ ] **Step 2: Build to verify**

Run: `cmake --build cmake-build-debug`

---

### Task 4: Implement cursor management helpers

**Files:**
- Modify: `src/TimelinePanel.cpp`

- [ ] **Step 1: Implement updateClipCursor and resetClipCursor**

Add these two methods in `src/TimelinePanel.cpp`:

```cpp
void TimelinePanel::updateClipCursor(int mouseX, int mouseY) {
  QString id;
  ClipInteractionMode mode = hitTestClip(mouseX, mouseY, &id);
  switch (mode) {
  case ClipInteractionMode::ClipResizeLeft:
    setCursor(QCursor(QPixmap(":/icons/resize-left.svg")));
    break;
  case ClipInteractionMode::ClipResizeRight:
    setCursor(QCursor(QPixmap(":/icons/resize-right.svg")));
    break;
  default:
    unsetCursor();
    break;
  }
}

void TimelinePanel::resetClipCursor() { unsetCursor(); }
```

- [ ] **Step 2: Build to verify**

Run: `cmake --build cmake-build-debug`

---

### Task 5: Modify mousePressEvent for clip interaction

**Files:**
- Modify: `src/TimelinePanel.cpp`

- [ ] **Step 1: Update mousePressEvent**

Replace the existing `mousePressEvent` implementation in `src/TimelinePanel.cpp` (lines 562-590) with:

```cpp
void TimelinePanel::mousePressEvent(QMouseEvent *event) {
  if (event->button() != Qt::LeftButton)
    return;

  // Empty state import button click
  if (totalDurationMs_ == 0 && !emptyStateRect_.isEmpty() &&
      emptyStateRect_.contains(event->pos())) {
    emit importMediaRequested();
    return;
  }

  if (event->x() < TRACK_HEAD_WIDTH)
    return;

  // Determine interaction mode based on click position
  QString hitId;
  ClipInteractionMode mode = hitTestClip(event->x(), event->y(), &hitId);

  if (mode == ClipInteractionMode::ClipMove ||
      mode == ClipInteractionMode::ClipResizeLeft ||
      mode == ClipInteractionMode::ClipResizeRight) {
    // Start clip drag/resize
    clipMode_ = mode;
    dragTargetId_ = hitId;
    const SubtitleItem *item = track_->findItem(hitId);
    if (!item) {
      clipMode_ = ClipInteractionMode::Idle;
      return;
    }
    dragOrigStartMs_ = item->startMs;
    dragOrigEndMs_ = item->endMs;
    dragTempStartMs_ = item->startMs;
    dragTempEndMs_ = item->endMs;

    mousePressed_ = true;
    isDragging_ = false;
    dragStartX_ = event->x();
  } else if (event->y() < RULER_HEIGHT) {
    // Click on ruler: seek behavior (existing)
    mousePressed_ = true;
    isDragging_ = false;
    dragStartX_ = event->x();

    qint64 ms = xToTime(event->x());
    if (ms < 0)
      ms = 0;
    if (ms > totalDurationMs_)
      ms = totalDurationMs_;
    currentTimeMs_ = ms;
    lastPreviewSystemTime_ = QDateTime::currentMSecsSinceEpoch();
    canvas_->update();
  } else {
    // Click on empty area: no-op, but set mousePressed for seek fallback
    mousePressed_ = true;
    isDragging_ = false;
    dragStartX_ = event->x();

    qint64 ms = xToTime(event->x());
    if (ms < 0)
      ms = 0;
    if (ms > totalDurationMs_)
      ms = totalDurationMs_;
    currentTimeMs_ = ms;
    lastPreviewSystemTime_ = QDateTime::currentMSecsSinceEpoch();
    canvas_->update();
  }
}
```

- [ ] **Step 2: Build to verify**

Run: `cmake --build cmake-build-debug`

---

### Task 6: Modify mouseMoveEvent for clip drag/resize

**Files:**
- Modify: `src/TimelinePanel.cpp`

- [ ] **Step 1: Update mouseMoveEvent**

Replace the existing `mouseMoveEvent` implementation in `src/TimelinePanel.cpp` (lines 592-623) with:

```cpp
void TimelinePanel::mouseMoveEvent(QMouseEvent *event) {
  // Update cursor when not dragging
  if (!mousePressed_) {
    updateClipCursor(event->x(), event->y());
    return;
  }

  if (event->x() < TRACK_HEAD_WIDTH)
    return;

  // Check if we've crossed the drag threshold
  if (!isDragging_) {
    if (qAbs(event->x() - dragStartX_) < DRAG_THRESHOLD_PX)
      return;
    isDragging_ = true;
    lastPreviewSystemTime_ = QDateTime::currentMSecsSinceEpoch();
  }

  if (clipMode_ == ClipInteractionMode::ClipMove ||
      clipMode_ == ClipInteractionMode::ClipResizeLeft ||
      clipMode_ == ClipInteractionMode::ClipResizeRight) {
    // --- Clip drag/resize ---
    qint64 mouseMs = xToTime(event->x());
    qint64 origDuration = dragOrigEndMs_ - dragOrigStartMs_;

    if (clipMode_ == ClipInteractionMode::ClipMove) {
      qint64 deltaMs = mouseMs - xToTime(dragStartX_);
      qint64 newStart = dragOrigStartMs_ + deltaMs;
      qint64 newEnd = dragOrigEndMs_ + deltaMs;

      // Find adjacent clips
      qint64 prevEnd = -1;   // endMs of previous clip, or -1 if none
      qint64 nextStart = -1; // startMs of next clip, or -1 if none
      for (const auto &item : track_->items()) {
        if (item.id == dragTargetId_)
          continue;
        if (item.endMs <= dragOrigStartMs_) {
          if (prevEnd < 0 || item.endMs > prevEnd)
            prevEnd = item.endMs;
        }
        if (item.startMs >= dragOrigEndMs_) {
          if (nextStart < 0 || item.startMs < nextStart)
            nextStart = item.startMs;
        }
      }

      // Collision: left move
      if (prevEnd >= 0 && newStart <= prevEnd)
        newStart = prevEnd + 1;
      // Collision: right move
      if (nextStart >= 0 && newEnd >= nextStart)
        newEnd = nextStart - 1;

      // Re-derive the other end to preserve duration
      newEnd = newStart + origDuration;
      // Re-check right collision with new end
      if (nextStart >= 0 && newEnd >= nextStart) {
        newEnd = nextStart - 1;
        newStart = newEnd - origDuration;
      }

      // Boundary
      if (newStart < 0) {
        newStart = 0;
        newEnd = origDuration;
      }
      if (newEnd > totalDurationMs_ - 1) {
        newEnd = totalDurationMs_ - 1;
        newStart = newEnd - origDuration;
      }
      if (newStart < 0)
        newStart = 0;

      dragTempStartMs_ = newStart;
      dragTempEndMs_ = newEnd;

    } else if (clipMode_ == ClipInteractionMode::ClipResizeLeft) {
      qint64 newStart = xToTime(event->x());

      // Find previous clip
      qint64 prevEnd = -1;
      for (const auto &item : track_->items()) {
        if (item.id == dragTargetId_)
          continue;
        if (item.endMs <= dragOrigStartMs_) {
          if (prevEnd < 0 || item.endMs > prevEnd)
            prevEnd = item.endMs;
        }
      }

      // Collision
      if (prevEnd >= 0 && newStart <= prevEnd)
        newStart = prevEnd + 1;

      // Minimum duration
      if (dragOrigEndMs_ - newStart < 100)
        newStart = dragOrigEndMs_ - 100;

      // Boundary
      if (newStart < 0)
        newStart = 0;

      dragTempStartMs_ = newStart;
      dragTempEndMs_ = dragOrigEndMs_;

    } else if (clipMode_ == ClipInteractionMode::ClipResizeRight) {
      qint64 newEnd = xToTime(event->x());

      // Find next clip
      qint64 nextStart = -1;
      for (const auto &item : track_->items()) {
        if (item.id == dragTargetId_)
          continue;
        if (item.startMs >= dragOrigEndMs_) {
          if (nextStart < 0 || item.startMs < nextStart)
            nextStart = item.startMs;
        }
      }

      // Collision
      if (nextStart >= 0 && newEnd >= nextStart)
        newEnd = nextStart - 1;

      // Minimum duration
      if (newEnd - dragOrigStartMs_ < 100)
        newEnd = dragOrigStartMs_ + 100;

      // Boundary
      if (newEnd > totalDurationMs_ - 1)
        newEnd = totalDurationMs_ - 1;

      dragTempStartMs_ = dragOrigStartMs_;
      dragTempEndMs_ = newEnd;
    }

    canvas_->update();

  } else {
    // --- Original seek drag behavior ---
    qint64 ms = xToTime(event->x());
    if (ms < 0)
      ms = 0;
    if (ms > totalDurationMs_)
      ms = totalDurationMs_;

    currentTimeMs_ = ms;
    canvas_->update();

    qint64 now = QDateTime::currentMSecsSinceEpoch();
    qint64 intervalMs = static_cast<qint64>(1000.0 / videoFps_);
    if (now - lastPreviewSystemTime_ >= intervalMs) {
      lastPreviewSystemTime_ = now;
      emit previewSeekRequested(ms);
    }
  }
}
```

- [ ] **Step 2: Build to verify**

Run: `cmake --build cmake-build-debug`

---

### Task 7: Modify mouseReleaseEvent for clip commit

**Files:**
- Modify: `src/TimelinePanel.cpp`

- [ ] **Step 1: Update mouseReleaseEvent**

Replace the existing `mouseReleaseEvent` implementation in `src/TimelinePanel.cpp` (lines 625-646) with:

```cpp
void TimelinePanel::mouseReleaseEvent(QMouseEvent *event) {
  if (event->button() != Qt::LeftButton)
    return;

  if (isDragging_ &&
      (clipMode_ == ClipInteractionMode::ClipMove ||
       clipMode_ == ClipInteractionMode::ClipResizeLeft ||
       clipMode_ == ClipInteractionMode::ClipResizeRight)) {
    // Commit clip position: apply temp values to the track
    if (track_ && !dragTargetId_.isEmpty()) {
      SubtitleItem item;
      item.id = dragTargetId_;
      item.text = track_->findItem(dragTargetId_)->text;
      item.startMs = dragTempStartMs_;
      item.endMs = dragTempEndMs_;
      item.selected = true;
      track_->updateItem(dragTargetId_, item);
    }
    clipMode_ = ClipInteractionMode::Idle;
    dragTargetId_.clear();

  } else if (isDragging_) {
    // Drag seek ended
    emit dragSeekFinished(currentTimeMs_);
  } else {
    // Click (no drag): emit seek
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
  isDragging_ = false;
}
```

- [ ] **Step 2: Build to verify**

Run: `cmake --build cmake-build-debug`

---

### Task 8: Modify drawSubtitleTrack to use temp values during drag

**Files:**
- Modify: `src/TimelinePanel.cpp`

- [ ] **Step 1: Update drawSubtitleTrack**

In `drawSubtitleTrack`, modify the subtitle bars loop (around lines 411-438) to use temporary values when the clip is being dragged. Replace the loop with:

```cpp
  for (auto item : track_->items()) {
    // Use temp values if this clip is being dragged
    if (isDragging_ && item.id == dragTargetId_) {
      item.startMs = dragTempStartMs_;
      item.endMs = dragTempEndMs_;
    }

    int x = timeToX(item.startMs);
    int w = timeToX(item.endMs) - x;
    if (w < 4)
      w = 4;
    if (x + w < TRACK_HEAD_WIDTH)
      continue;
    if (x > canvas_->width() - PANEL_RIGHT_MARGIN)
      continue;

    QColor barColor = item.selected ? QColor("#0ea5e9") : QColor("#38bdf8");
    painter.setPen(Qt::NoPen);
    painter.setBrush(barColor);
    painter.drawRoundedRect(x, y + 2, w, SUBTITLE_TRACK_HEIGHT - 4, 4, 4);

    painter.setPen(QColor("#e5e5e5"));
    QFont barFont = painter.font();
    barFont.setPointSize(9);
    painter.setFont(barFont);

    int textX = qMax(x + 8, TRACK_HEAD_WIDTH + 4);
    int textMaxWidth = qMax(0, x + w - 4 - textX);
    if (textMaxWidth > 0) {
      QFontMetrics fm(barFont);
      QString elided = fm.elidedText(item.text, Qt::ElideRight, textMaxWidth);
      painter.drawText(textX, y + 18, elided);
    }
  }
```

Note: The loop variable changes from `const auto &item` to `auto item` (by value) so we can modify its startMs/endMs for rendering.

- [ ] **Step 2: Build to verify**

Run: `cmake --build cmake-build-debug`

---

### Task 9: Add QApplication include for cursor support

**Files:**
- Modify: `src/TimelinePanel.cpp`

- [ ] **Step 1: Verify QApplication is already included**

The file already includes `<QApplication>` on line 9. Also verify `<QCursor>` is available — it's included transitively via QWidget. No changes needed for includes.

- [ ] **Step 2: Full build and manual test**

Run: `cmake --build cmake-build-debug`

Then run the app and verify:
- Hovering over clip left/right edge shows resize cursor
- Dragging clip middle moves it
- Dragging left edge adjusts startMs
- Dragging right edge adjusts endMs
- Release commits the change
- Clips don't overlap adjacent clips
- Minimum duration of 100ms is enforced

---

### Task 10: clang-format and final build

**Files:**
- Modify: all changed files

- [ ] **Step 1: Run clang-format**

```bash
clang-format -i src/TimelinePanel.cpp include/TimelinePanel.h
```

- [ ] **Step 2: Final build**

```bash
cmake --build cmake-build-debug
```

- [ ] **Step 3: Verify no warnings**

Check build output for warnings. Fix any if present.

---

## Self-Review

1. **Spec coverage:** All sections addressed — interaction modes, cursor rules, collision rules, min duration, boundaries, commit strategy, adjacent clip lookup.
2. **Placeholder scan:** No TBD/TODO placeholders found. All steps have concrete code.
3. **Type consistency:** `ClipInteractionMode` enum used consistently across header and implementation. Member variable names match between declaration and usage.
