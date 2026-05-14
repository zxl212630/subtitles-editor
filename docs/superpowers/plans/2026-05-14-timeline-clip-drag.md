# 时间线字幕 Clip 拖拽调整 实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** TimelinePanel 支持字幕 clip 拖拽移动和边缘缩放，带碰撞检测、自定义光标、仅刻度尺 seek。

**Architecture:** 在 TimelinePanel 中新增 `ClipMove`/`ClipResizeLeft`/`ClipResizeRight` 三种交互状态，通过 mousePressEvent 判断点击位置进入不同模式。拖拽期间使用 `previewStartMs_`/`previewEndMs_` 做视觉预览，mouseReleaseEvent 时提交 `track_->updateItem()`。Seek 行为限制为仅刻度尺区域（Y < 36）。

**Tech Stack:** C++17, Qt6 Widgets, QPainter canvas 绘制

---

### Task 1: 更新 TimelinePanel.h — 新枚举、成员变量、方法声明

**File:** `include/TimelinePanel.h`

- [ ] **Step 1: 在 class TimelinePanel 的 private 区域新增 InteractionMode 枚举**

在 line 76（`RULER_HEIGHT` 等常量区域后）添加：

```cpp
  enum class InteractionMode { None, ClipMove, ClipResizeLeft, ClipResizeRight };
```

- [ ] **Step 2: 新增常量**

在现有常量区域添加：

```cpp
  static constexpr int EDGE_HIT_ZONE = 6;
  static constexpr int MIN_CLIP_DURATION_MS = 100;
```

- [ ] **Step 3: 新增方法声明（private 区域）**

```cpp
  const SubtitleItem* hitTestClip(const QPoint &pos) const;
  QPair<const SubtitleItem*, const SubtitleItem*> findAdjacentClips(const QString &id) const;
```

- [ ] **Step 4: 新增成员变量（private 区域）**

```cpp
  InteractionMode interactionMode_ = InteractionMode::None;
  QString draggingItemId_;
  qint64 dragOriginalStartMs_ = 0;
  qint64 dragOriginalEndMs_ = 0;
  qint64 previewStartMs_ = 0;
  qint64 previewEndMs_ = 0;
  QCursor resizeLeftCursor_;
  QCursor resizeRightCursor_;
```

---

### Task 2: 添加 SVG 图标到 qrc 并加载光标

**Files:**
- Modify: `resources/resources.qrc`
- Modify: `src/TimelinePanel.cpp` (constructor)

- [ ] **Step 1: 在 resources.qrc 中添加两个图标**

在 `resources/resources.qrc` 的 `<qresource>` 中，在现有 `<file>` 条目后添加：

```xml
        <file>icons/resize-left.svg</file>
        <file>icons/resize-right.svg</file>
```

- [ ] **Step 2: 在 TimelinePanel 构造函数中加载自定义光标**

在 `src/TimelinePanel.cpp` 构造函数中 `setMouseTracking(true)` 之前添加：

```cpp
  resizeLeftCursor_ = QCursor(QIcon(":/icons/resize-left.svg").pixmap(24, 24), 0, 12);
  resizeRightCursor_ = QCursor(QIcon(":/icons/resize-right.svg").pixmap(24, 24), 23, 12);
```

需在文件头添加 `#include <QIcon>`。

---

### Task 3: 实现 hitTestClip 和 findAdjacentClips 辅助方法

**File:** `src/TimelinePanel.cpp`

在两个方法实现区域（如 `drawEmptyState` 之前或之后）添加。

- [ ] **Step 1: 实现 hitTestClip**

```cpp
const SubtitleItem* TimelinePanel::hitTestClip(const QPoint &pos) const {
  if (!track_)
    return nullptr;
  int trackY = RULER_HEIGHT;
  if (pos.y() < trackY || pos.y() >= trackY + SUBTITLE_TRACK_HEIGHT)
    return nullptr;

  for (const auto &item : track_->items()) {
    int x = timeToX(item.startMs);
    int w = timeToX(item.endMs) - x;
    if (w < 4)
      w = 4;
    if (pos.x() >= x && pos.x() <= x + w)
      return &item;
  }
  return nullptr;
}
```

- [ ] **Step 2: 实现 findAdjacentClips**

```cpp
QPair<const SubtitleItem*, const SubtitleItem*> TimelinePanel::findAdjacentClips(const QString &id) const {
  if (!track_)
    return {nullptr, nullptr};

  const SubtitleItem *current = track_->findItem(id);
  if (!current)
    return {nullptr, nullptr};

  const SubtitleItem *prev = nullptr;
  const SubtitleItem *next = nullptr;

  for (const auto &item : track_->items()) {
    if (item.id == id)
      continue;
    if (item.endMs <= current->startMs) {
      if (!prev || item.endMs > prev->endMs)
        prev = &item;
    }
    if (item.startMs >= current->endMs) {
      if (!next || item.startMs < next->startMs)
        next = &item;
    }
  }
  return {prev, next};
}
```

---

### Task 4: 重写 mousePressEvent / mouseMoveEvent / mouseReleaseEvent

**File:** `src/TimelinePanel.cpp`

**Note:** 这是核心改动，将替换现有的三个鼠标事件处理函数。

- [ ] **Step 1: 重写 mousePressEvent**

替换 line 562-590 的现有实现：

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

  // Ruler area: seek behavior (Y < RULER_HEIGHT)
  if (event->y() < RULER_HEIGHT && event->x() >= TRACK_HEAD_WIDTH) {
    mousePressed_ = true;
    isDragging_ = false;
    dragStartX_ = event->x();

    qint64 ms = xToTime(event->x());
    if (ms < 0) ms = 0;
    if (ms > totalDurationMs_) ms = totalDurationMs_;

    currentTimeMs_ = ms;
    lastPreviewSystemTime_ = QDateTime::currentMSecsSinceEpoch();
    canvas_->update();
    return;
  }

  // Track area: clip interaction
  const SubtitleItem *hitClip = hitTestClip(event->pos());
  if (!hitClip)
    return;

  draggingItemId_ = hitClip->id;
  dragOriginalStartMs_ = hitClip->startMs;
  dragOriginalEndMs_ = hitClip->endMs;
  dragStartX_ = event->x();

  int clipX = timeToX(hitClip->startMs);
  int clipW = timeToX(hitClip->endMs) - clipX;
  if (clipW < 4) clipW = 4;

  int relX = event->x() - clipX;

  if (relX <= EDGE_HIT_ZONE) {
    interactionMode_ = InteractionMode::ClipResizeLeft;
  } else if (relX >= clipW - EDGE_HIT_ZONE) {
    interactionMode_ = InteractionMode::ClipResizeRight;
  } else {
    interactionMode_ = InteractionMode::ClipMove;
  }
}
```

- [ ] **Step 2: 重写 mouseMoveEvent**

替换 line 592-623 的现有实现：

```cpp
void TimelinePanel::mouseMoveEvent(QMouseEvent *event) {
  // Not pressed: update cursor
  if (!mousePressed_ && interactionMode_ == InteractionMode::None) {
    const SubtitleItem *hovered = hitTestClip(event->pos());
    if (hovered) {
      int clipX = timeToX(hovered->startMs);
      int clipW = timeToX(hovered->endMs) - clipX;
      if (clipW < 4) clipW = 4;
      int relX = event->x() - clipX;
      if (relX <= EDGE_HIT_ZONE) {
        setCursor(resizeLeftCursor_);
      } else if (relX >= clipW - EDGE_HIT_ZONE) {
        setCursor(resizeRightCursor_);
      } else {
        setCursor(Qt::ArrowCursor);
      }
    } else {
      setCursor(Qt::ArrowCursor);
    }
    return;
  }

  // Clip interaction drag
  if (interactionMode_ != InteractionMode::None) {
    qint64 ms = xToTime(event->x());
    auto [prev, next] = findAdjacentClips(draggingItemId_);

    switch (interactionMode_) {
    case InteractionMode::ClipMove: {
      qint64 delta = ms - xToTime(dragStartX_);
      qint64 duration = dragOriginalEndMs_ - dragOriginalStartMs_;
      qint64 newStart = dragOriginalStartMs_ + delta;

      qint64 leftBound = prev ? prev->endMs + 1 : 0;
      qint64 rightBound = next ? next->startMs - 1 : totalDurationMs_ - 1;

      if (newStart < leftBound)
        newStart = leftBound;
      if (newStart + duration > rightBound)
        newStart = rightBound - duration;

      previewStartMs_ = newStart;
      previewEndMs_ = newStart + duration;
      break;
    }
    case InteractionMode::ClipResizeLeft: {
      qint64 newStart = ms;
      qint64 leftBound = prev ? prev->endMs + 1 : 0;
      if (newStart < leftBound)
        newStart = leftBound;
      if (newStart >= dragOriginalEndMs_ - MIN_CLIP_DURATION_MS)
        newStart = dragOriginalEndMs_ - MIN_CLIP_DURATION_MS;

      previewStartMs_ = newStart;
      previewEndMs_ = dragOriginalEndMs_;
      break;
    }
    case InteractionMode::ClipResizeRight: {
      qint64 newEnd = ms;
      qint64 rightBound = next ? next->startMs - 1 : totalDurationMs_ - 1;
      if (newEnd > rightBound)
        newEnd = rightBound;
      if (newEnd <= dragOriginalStartMs_ + MIN_CLIP_DURATION_MS)
        newEnd = dragOriginalStartMs_ + MIN_CLIP_DURATION_MS;

      previewStartMs_ = dragOriginalStartMs_;
      previewEndMs_ = newEnd;
      break;
    }
    default:
      break;
    }

    canvas_->update();
    return;
  }

  // Seek drag (existing behavior)
  if (!mousePressed_)
    return;
  if (event->x() < TRACK_HEAD_WIDTH)
    return;

  if (!isDragging_) {
    if (qAbs(event->x() - dragStartX_) < DRAG_THRESHOLD_PX)
      return;
    isDragging_ = true;
    lastPreviewSystemTime_ = QDateTime::currentMSecsSinceEpoch();
  }

  qint64 ms = xToTime(event->x());
  if (ms < 0) ms = 0;
  if (ms > totalDurationMs_) ms = totalDurationMs_;

  currentTimeMs_ = ms;
  canvas_->update();

  qint64 now = QDateTime::currentMSecsSinceEpoch();
  qint64 intervalMs = static_cast<qint64>(1000.0 / videoFps_);
  if (now - lastPreviewSystemTime_ >= intervalMs) {
    lastPreviewSystemTime_ = now;
    emit previewSeekRequested(ms);
  }
}
```

- [ ] **Step 3: 重写 mouseReleaseEvent**

替换 line 625-646 的现有实现：

```cpp
void TimelinePanel::mouseReleaseEvent(QMouseEvent *event) {
  if (event->button() != Qt::LeftButton)
    return;

  // Clip interaction: commit changes
  if (interactionMode_ != InteractionMode::None) {
    const SubtitleItem *item = track_->findItem(draggingItemId_);
    if (item) {
      SubtitleItem updated = *item;
      updated.startMs = previewStartMs_;
      updated.endMs = previewEndMs_;
      track_->updateItem(draggingItemId_, updated);
    }

    interactionMode_ = InteractionMode::None;
    draggingItemId_.clear();
    previewStartMs_ = 0;
    previewEndMs_ = 0;
    canvas_->update();
    return;
  }

  // Seek drag or click
  if (isDragging_) {
    emit dragSeekFinished(currentTimeMs_);
    isDragging_ = false;
  } else {
    qint64 ms = xToTime(event->x());
    if (ms < 0) ms = 0;
    if (ms > totalDurationMs_) ms = totalDurationMs_;
    currentTimeMs_ = ms;
    emit timeClicked(ms);
    canvas_->update();
  }

  mousePressed_ = false;
}
```

---

### Task 5: 修改 drawSubtitleTrack — 拖拽预览绘制

**File:** `src/TimelinePanel.cpp`

- [ ] **Step 1: 在 drawSubtitleTrack 的 clip 绘制循环中对被拖拽的 clip 使用预览坐标**

在 line 411-438 的 for 循环中，在获取 `x` 和 `w` 之前插入判断：

```cpp
  for (const auto &item : track_->items()) {
    qint64 drawStart = item.startMs;
    qint64 drawEnd = item.endMs;
    if (!draggingItemId_.isEmpty() && item.id == draggingItemId_) {
      drawStart = previewStartMs_;
      drawEnd = previewEndMs_;
    }
    int x = timeToX(drawStart);
    int w = timeToX(drawEnd) - x;
    if (w < 4)
      w = 4;
    // ... 后面使用 item.text 绘制文本不变 ...
```

注意 `item.text` 仍使用原始 item，仅 `x` 和 `w` 使用 preview 值。

---

### Task 6: 构建验证

**File:** 无需修改文件

- [ ] **Step 1: 构建**

```bash
cmake --build cmake-build-debug
```

Expected: 编译成功，无错误。

- [ ] **Step 2: 运行并手动测试**

```bash
./cmake-build-debug/subtitles-editor
```

Expected 行为：
1. 加载视频后，字幕 clip 显示在轨道上
2. 鼠标移到 clip 左边缘 → 自定义左 resize 光标
3. 鼠标移到 clip 右边缘 → 自定义右 resize 光标
4. 鼠标移到 clip 中间 → 默认箭头
5. 拖拽 clip 主体 → 左右移动，碰到相邻 clip 停止
6. 拖拽 clip 左边缘 → 调整起始时间
7. 拖拽 clip 右边缘 → 调整结束时间
8. 点击刻度尺区域 → seek 行为不受影响
9. 点击轨道空白区域 → 无操作

- [ ] **Step 3: 提交**

```bash
git add include/TimelinePanel.h src/TimelinePanel.cpp resources/resources.qrc
git commit -m "feat: 字幕 clip 拖拽移动和边缘缩放"
```
