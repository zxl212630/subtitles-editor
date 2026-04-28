# Timeline Zoom & Scroll Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add zoom (Ctrl+wheel) and horizontal scroll (wheel) to TimelinePanel, with a horizontal scrollbar, adaptive ruler ticks, and auto-scaling subtitle/video bars.

**Architecture:** Replace fixed `PIXELS_PER_SECOND` with runtime `pixelsPerSecond_` and `scrollOffsetX_`. Add a `TimelineCanvas` internal widget for painting and a `QScrollBar` for scrolling. All coordinate conversions go through `timeToX()` / `xToTime()`.

**Tech Stack:** C++17, Qt6, QPainter, QScrollBar

---

## File Structure

- `include/TimelinePanel.h` — Add state variables, helper methods, event overrides, forward-declare `TimelineCanvas`
- `src/TimelinePanel.cpp` — Implement canvas layout, coordinate helpers, adaptive ruler, wheel/scroll logic, scrollbar sync

No other files need changes.

---

### Task 1: Update Header File

**Files:**
- Modify: `include/TimelinePanel.h`

- [ ] **Step 1: Remove fixed constant and add state**

  Remove:
  ```cpp
  static constexpr int PIXELS_PER_SECOND = 100;
  ```

  Add private members:
  ```cpp
  class TimelineCanvas;
  
  double pixelsPerSecond_ = 100.0;
  int scrollOffsetX_ = 0;
  QScrollBar *hScrollBar_ = nullptr;
  TimelineCanvas *canvas_ = nullptr;
  ```

- [ ] **Step 2: Add helper and event declarations**

  Add private methods:
  ```cpp
  int timeToX(qint64 ms) const;
  qint64 xToTime(int x) const;
  void updateScrollBar();
  void clampScrollOffset();
  void drawOnCanvas(QPainter &painter);
  ```

  Add event override:
  ```cpp
  void wheelEvent(QWheelEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;
  ```

  Remove old helpers:
  ```cpp
  qint64 pixelsToMs(int px) const;
  int msToPixels(qint64 ms) const;
  ```

- [ ] **Step 3: Commit**

  ```bash
  git add include/TimelinePanel.h
  git commit -m "feat(timeline): add zoom/scroll state and helpers to header"
  ```

---

### Task 2: Refactor Constructor and Layout

**Files:**
- Modify: `src/TimelinePanel.cpp`

- [ ] **Step 1: Add includes**

  ```cpp
  #include <QScrollBar>
  #include <QVBoxLayout>
  #include <QWheelEvent>
  ```

- [ ] **Step 2: Add internal TimelineCanvas class (top of file, before constructor)**

  ```cpp
  class TimelineCanvas : public QWidget {
  public:
    explicit TimelineCanvas(TimelinePanel *panel, QWidget *parent = nullptr)
        : QWidget(parent), panel_(panel) {
      setAttribute(Qt::WA_StyledBackground);
    }

  protected:
    void paintEvent(QPaintEvent * /*event*/) override {
      QPainter painter(this);
      panel_->drawOnCanvas(painter);
    }

  private:
    TimelinePanel *panel_;
  };
  ```

- [ ] **Step 3: Replace constructor body**

  Replace the current constructor body with:

  ```cpp
  TimelinePanel::TimelinePanel(QWidget *parent) : QWidget(parent) {
    setObjectName("TimelinePanel");
    setAttribute(Qt::WA_StyledBackground);
    setAcceptDrops(true);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    canvas_ = new TimelineCanvas(this, this);
    canvas_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(canvas_);

    hScrollBar_ = new QScrollBar(Qt::Horizontal, this);
    hScrollBar_->setFixedHeight(14);
    hScrollBar_->setStyleSheet(R"(
        QScrollBar:horizontal {
            background: #1e1e1e;
            height: 14px;
            border-radius: 4px;
        }
        QScrollBar::handle:horizontal {
            background: #4a4a4a;
            border-radius: 4px;
            min-width: 20px;
        }
        QScrollBar::handle:horizontal:hover {
            background: #5a5a5a;
        }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
            width: 0px;
            border: none;
        }
    )");
    layout->addWidget(hScrollBar_);

    connect(hScrollBar_, &QScrollBar::valueChanged, this, [this](int value) {
      scrollOffsetX_ = value;
      canvas_->update();
    });

    setStyleSheet(R"(
        QWidget#TimelinePanel {
            background-color: #1e1e1e;
            border-radius: 10px;
        }
    )");
  }
  ```

- [ ] **Step 4: Commit**

  ```bash
  git add src/TimelinePanel.cpp
  git commit -m "feat(timeline): add canvas widget and horizontal scrollbar layout"
  ```

---

### Task 3: Add Coordinate Helpers and Scrollbar Sync

**Files:**
- Modify: `src/TimelinePanel.cpp`

- [ ] **Step 1: Add helper methods**

  Add after the constructor:

  ```cpp
  int TimelinePanel::timeToX(qint64 ms) const {
    return TRACK_HEAD_WIDTH + static_cast<int>(ms * pixelsPerSecond_ / 1000.0) - scrollOffsetX_;
  }

  qint64 TimelinePanel::xToTime(int x) const {
    if (x < TRACK_HEAD_WIDTH)
      return 0;
    int relX = x - TRACK_HEAD_WIDTH + scrollOffsetX_;
    return static_cast<qint64>(relX * 1000.0 / pixelsPerSecond_);
  }

  void TimelinePanel::clampScrollOffset() {
    int canvasWidth = canvas_->width();
    int contentWidth = static_cast<int>(totalDurationMs_ * pixelsPerSecond_ / 1000.0);
    int maxOffset = qMax(0, contentWidth - canvasWidth + TRACK_HEAD_WIDTH);
    if (scrollOffsetX_ < 0)
      scrollOffsetX_ = 0;
    if (scrollOffsetX_ > maxOffset)
      scrollOffsetX_ = maxOffset;
  }

  void TimelinePanel::updateScrollBar() {
    int canvasWidth = canvas_->width();
    int contentWidth = static_cast<int>(totalDurationMs_ * pixelsPerSecond_ / 1000.0);
    int maxOffset = qMax(0, contentWidth - canvasWidth + TRACK_HEAD_WIDTH);

    hScrollBar_->setRange(0, maxOffset);
    hScrollBar_->setPageStep(canvasWidth);
    hScrollBar_->setSingleStep(static_cast<int>(pixelsPerSecond_)); // ~1 second

    if (scrollOffsetX_ > maxOffset)
      scrollOffsetX_ = maxOffset;
    hScrollBar_->setValue(scrollOffsetX_);
  }
  ```

- [ ] **Step 2: Commit**

  ```bash
  git add src/TimelinePanel.cpp
  git commit -m "feat(timeline): add timeToX, xToTime, and scrollbar sync helpers"
  ```

---

### Task 4: Replace Paint Event and Drawing Logic

**Files:**
- Modify: `src/TimelinePanel.cpp`

- [ ] **Step 1: Replace paintEvent with drawOnCanvas**

  Replace the old `paintEvent` method with:

  ```cpp
  void TimelinePanel::drawOnCanvas(QPainter &painter) {
    painter.setRenderHint(QPainter::Antialiasing);

    // Clip to rounded rect so border-radius works with paintEvent
    QPainterPath clipPath;
    clipPath.addRoundedRect(canvas_->rect().adjusted(1, 1, -1, -1), 10, 10);
    painter.setClipPath(clipPath);

    // Background
    painter.fillRect(canvas_->rect(), QColor("#1e1e1e"));

    drawRuler(painter);
    drawSubtitleTrack(painter, RULER_HEIGHT);
    drawVideoTrack(painter, RULER_HEIGHT + SUBTITLE_TRACK_HEIGHT);
    drawPlayhead(painter);
  }
  ```

- [ ] **Step 2: Update drawRuler with adaptive ticks**

  Replace `drawRuler` entirely:

  ```cpp
  void TimelinePanel::drawRuler(QPainter &painter) {
    painter.setPen(QColor("#6b7280"));
    QFont font = painter.font();
    font.setPointSize(8);
    painter.setFont(font);

    // Determine tick interval based on zoom
    double majorIntervalSec = 1.0;
    double minorIntervalSec = 0.5;
    if (pixelsPerSecond_ < 20.0) {
      majorIntervalSec = 10.0;
      minorIntervalSec = 5.0;
    } else if (pixelsPerSecond_ < 50.0) {
      majorIntervalSec = 5.0;
      minorIntervalSec = 1.0;
    } else if (pixelsPerSecond_ < 100.0) {
      majorIntervalSec = 2.0;
      minorIntervalSec = 1.0;
    }

    bool showHours = totalDurationMs_ >= 3600000;

    int canvasWidth = canvas_->width();
    int startSec = static_cast<int>(xToTime(TRACK_HEAD_WIDTH) / 1000.0);
    int endSec = static_cast<int>(xToTime(canvasWidth) / 1000.0) + 1;
    if (startSec < 0) startSec = 0;

    for (double s = startSec; s <= endSec; s += majorIntervalSec) {
      int x = timeToX(static_cast<qint64>(s * 1000));
      if (x > canvasWidth)
        break;

      int sec = static_cast<int>(s) % 60;
      int min = (static_cast<int>(s) / 60) % 60;
      int hr = static_cast<int>(s) / 3600;
      QString label;
      if (showHours) {
        label = QString("%1:%2:%3").arg(hr, 2, 10, QChar('0')).arg(min, 2, 10, QChar('0')).arg(sec, 2, 10, QChar('0'));
      } else {
        label = QString("%1:%2").arg(min, 2, 10, QChar('0')).arg(sec, 2, 10, QChar('0'));
      }
      painter.drawText(x - 30, 8, 60, 14, Qt::AlignCenter, label);

      painter.setPen(QColor("#333333"));
      painter.drawLine(x, 24, x, 34);
      painter.setPen(QColor("#6b7280"));
    }

    // Minor ticks
    painter.setPen(QColor("#404040"));
    for (double s = startSec; s <= endSec; s += minorIntervalSec) {
      int x = timeToX(static_cast<qint64>(s * 1000));
      if (x < TRACK_HEAD_WIDTH || x > canvasWidth)
        continue;
      // Skip if too close to a major tick
      int majorX = timeToX(static_cast<qint64>((s / majorIntervalSec) * majorIntervalSec * 1000));
      if (qAbs(x - majorX) < 3)
        continue;
      painter.drawLine(x, 28, x, 31);
    }
  }
  ```

- [ ] **Step 3: Update drawSubtitleTrack to use timeToX**

  Replace the subtitle bars loop:

  ```cpp
  // Subtitle bars
  for (const auto &item : track_->items()) {
    int x = timeToX(item.startMs);
    int w = timeToX(item.endMs) - x;
    if (w < 4)
      w = 4;
    if (x + w < TRACK_HEAD_WIDTH)
      continue;
    if (x > canvas_->width())
      continue;

    QColor barColor = item.selected ? QColor("#0ea5e9") : QColor("#38bdf8");
    painter.setPen(Qt::NoPen);
    painter.setBrush(barColor);
    painter.drawRoundedRect(x, y + 2, w, SUBTITLE_TRACK_HEIGHT - 4, 4, 4);

    painter.setPen(QColor("#e5e5e5"));
    QFont barFont = painter.font();
    barFont.setPointSize(9);
    painter.setFont(barFont);
    if (x >= TRACK_HEAD_WIDTH) {
      painter.drawText(x + 8, y + 18, item.text);
    }
  }
  ```

- [ ] **Step 4: Update drawVideoTrack to use timeToX**

  Replace the video bar section:

  ```cpp
  // Video bar (duration-based or placeholder)
  painter.setPen(Qt::NoPen);
  painter.setBrush(QColor("#0284c7"));
  if (totalDurationMs_ > 0) {
    int videoX = timeToX(0);
    int videoEndX = timeToX(totalDurationMs_);
    int videoWidth = videoEndX - videoX;
    if (videoWidth < 4) videoWidth = 4;
    painter.drawRoundedRect(videoX + 4, y + 2, videoWidth - 8,
                            VIDEO_TRACK_HEIGHT - 4, 4, 4);
  } else {
    painter.drawRoundedRect(TRACK_HEAD_WIDTH + 4, y + 2, 400,
                            VIDEO_TRACK_HEIGHT - 4, 4, 4);
  }
  painter.setPen(QColor("#e5e5e5"));
  painter.drawText(TRACK_HEAD_WIDTH + 16, y + 50, "video.mp4");
  ```

- [ ] **Step 5: Update drawPlayhead to use timeToX**

  ```cpp
  void TimelinePanel::drawPlayhead(QPainter &painter) {
    int x = timeToX(currentTimeMs_);
    const int triangleTop = 19;
    const int triangleTip = 31;

    // Only draw if within visible area
    if (x < TRACK_HEAD_WIDTH || x > canvas_->width())
      return;

    // Triangle pointer below time labels
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor("#f59e0b"));
    QPointF triangle[3] = {QPointF(x - 7, triangleTop),
                           QPointF(x + 7, triangleTop), QPointF(x, triangleTip)};
    painter.drawPolygon(triangle, 3);

    // Vertical line starts from triangle tip
    painter.setPen(QColor("#f59e0b"));
    painter.drawLine(x, triangleTip, x, canvas_->height());
  }
  ```

- [ ] **Step 6: Commit**

  ```bash
  git add src/TimelinePanel.cpp
  git commit -m "feat(timeline): replace fixed scale with adaptive ruler and timeToX/xToTime"
  ```

---

### Task 5: Add Wheel Zoom and Scroll Events

**Files:**
- Modify: `src/TimelinePanel.cpp`

- [ ] **Step 1: Implement wheelEvent**

  Add:

  ```cpp
  void TimelinePanel::wheelEvent(QWheelEvent *event) {
    if (event->modifiers() & Qt::ControlModifier) {
      // Zoom
      QPoint pos = event->position().toPoint();
      qint64 t = xToTime(pos.x());

      double factor = (event->angleDelta().y() > 0) ? 1.25 : 0.8;
      double newPps = pixelsPerSecond_ * factor;
      newPps = qBound(10.0, newPps, 1000.0);

      // Adjust scroll so the time under cursor stays at the same screen X
      int cursorRelX = pos.x() - TRACK_HEAD_WIDTH;
      double newScroll = (t * newPps / 1000.0) - cursorRelX;
      scrollOffsetX_ = static_cast<int>(newScroll);

      pixelsPerSecond_ = newPps;
      clampScrollOffset();
      updateScrollBar();
      canvas_->update();
    } else {
      // Horizontal scroll
      int delta = event->angleDelta().y();
      // Support horizontal wheel / trackpad
      if (delta == 0)
        delta = event->angleDelta().x();
      scrollOffsetX_ -= delta;
      clampScrollOffset();
      hScrollBar_->setValue(scrollOffsetX_);
      canvas_->update();
    }
    event->accept();
  }
  ```

- [ ] **Step 2: Implement resizeEvent**

  Add:

  ```cpp
  void TimelinePanel::resizeEvent(QResizeEvent * /*event*/) {
    updateScrollBar();
  }
  ```

- [ ] **Step 3: Commit**

  ```bash
  git add src/TimelinePanel.cpp
  git commit -m "feat(timeline): add Ctrl+wheel zoom and wheel scroll"
  ```

---

### Task 6: Update Mouse Click and TotalDuration

**Files:**
- Modify: `src/TimelinePanel.cpp`

- [ ] **Step 1: Update mousePressEvent to use xToTime**

  Replace `mousePressEvent`:

  ```cpp
  void TimelinePanel::mousePressEvent(QMouseEvent *event) {
    if (event->x() < TRACK_HEAD_WIDTH)
      return;

    qint64 ms = xToTime(event->x());
    if (ms < 0)
      ms = 0;
    if (ms > totalDurationMs_)
      ms = totalDurationMs_;

    currentTimeMs_ = ms;
    emit timeClicked(ms);
    canvas_->update();
  }
  ```

- [ ] **Step 2: Update setTotalDuration to sync scrollbar**

  Replace `setTotalDuration`:

  ```cpp
  void TimelinePanel::setTotalDuration(qint64 ms) {
    totalDurationMs_ = ms;
    clampScrollOffset();
    updateScrollBar();
    canvas_->update();
  }
  ```

- [ ] **Step 3: Commit**

  ```bash
  git add src/TimelinePanel.cpp
  git commit -m "feat(timeline): update mouse click and duration handling for zoom/scroll"
  ```

---

### Task 7: Build and Verify

**Files:**
- All modified files

- [ ] **Step 1: Format code**

  ```bash
  clang-format -i src/TimelinePanel.cpp include/TimelinePanel.h
  ```

- [ ] **Step 2: Build**

  ```bash
  cmake --build cmake-build-debug
  ```

  Expected: compilation succeeds with zero errors.

- [ ] **Step 3: Run smoke test**

  ```bash
  ./cmake-build-debug/subtitles-editor
  ```

  Verify visually:
  1. Dummy subtitle bars appear at correct positions.
  2. Scrollbar is visible at the bottom of the timeline panel.
  3. Mouse wheel scrolls the timeline horizontally.
  4. Ctrl + mouse wheel zooms in/out (bars expand/contract, ruler labels adapt).
  5. Clicking on the timeline seeks to the correct time.

- [ ] **Step 4: Commit formatting**

  ```bash
  git add -A
  git commit -m "style(timeline): apply clang-format"
  ```

---

## Spec Coverage Check

| Spec Requirement | Task |
|------------------|------|
| Replace fixed `PIXELS_PER_SECOND` with `pixelsPerSecond_` | Task 1, 4 |
| Mouse wheel scrolls horizontally | Task 5 |
| Ctrl + mouse wheel zooms around cursor | Task 5 |
| Horizontal scrollbar at bottom | Task 2, 3 |
| Ruler tick density adapts to zoom | Task 4 |
| Subtitle bars scale with zoom | Task 4 |
| Video clip length scales with zoom | Task 4 |
| Public interfaces unchanged | All (no changes to AppWindow) |
| Dark scrollbar styling | Task 2 |

## Placeholder Scan

- No "TBD", "TODO", "implement later" found.
- All code blocks contain complete, copy-pasteable C++.
- All method signatures match between header and implementation.

## Type Consistency

- `timeToX` returns `int`, `xToTime` returns `qint64` — consistent throughout.
- `pixelsPerSecond_` is `double`, clamped with `qBound<double>` equivalent.
- `scrollOffsetX_` is `int`, synced with `QScrollBar::valueChanged` (int).
